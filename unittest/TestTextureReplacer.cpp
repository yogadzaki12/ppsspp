#include <cstdio>
#include <cstring>
#include <vector>

#include "Common/Data/Format/PNGLoad.h"
#include "Common/File/FileUtil.h"
#include "Common/File/Path.h"
#include "Common/Thread/ThreadManager.h"
#include "GPU/Common/TextureReplacer.h"

#include "UnitTest.h"

// The fake hashes in textures.ini are designed to be readable: the address
// part starts with A, the CLUT hash with C, and the contents hash with 1.
static const u64 KEY_A = 0xA0000001C0000001ULL;  // addr 0xA0000001, clut 0xC0000001
static const u32 HASH_A = 0x10000001;
static const u64 KEY_B = 0xA0000002C0000002ULL;  // two mip levels
static const u32 HASH_B = 0x10000002;
static const u64 KEY_C = 0xA0000003C0000003ULL;  // hashrange
static const u32 HASH_C = 0x10000003;
static const u64 KEY_D = 0xA0000004C0000004ULL;  // ignored (empty filename)
static const u32 HASH_D = 0x10000004;
static const u64 KEY_E = 0xA0000005C0000005ULL;  // file missing
static const u32 HASH_E = 0x10000005;

static bool CreateTestPNG(const Path &filename, int w, int h, u32 color) {
	std::vector<u8> buf((size_t)w * h * 4);
	for (int i = 0; i < w * h; i++) {
		buf[i * 4] = (color >> 24) & 0xFF;
		buf[i * 4 + 1] = (color >> 16) & 0xFF;
		buf[i * 4 + 2] = (color >> 8) & 0xFF;
		buf[i * 4 + 3] = color & 0xFF;
	}
	return pngSave(filename, buf.data(), w, h, 4);
}

static bool CreateTestPack(const Path &packDir) {
	File::DeleteDirRecursively(packDir);
	if (!File::CreateDir(packDir)) {
		return false;
	}

	static const char *iniContent =
		"[options]\n"
		"hash = quick\n"
		"version = 1\n"
		"\n"
		"[hashes]\n"
		"A0000001C000000110000001 = tex_a.png\n"
		"A0000002C000000210000002 = tex_b0.png\n"
		"A0000002C000000210000002_1 = tex_b1.png\n"
		"A0000003C000000310000003 = tex_c.png\n"
		"A0000004C000000410000004 =\n"
		"A0000005C000000510000005 = missing.png\n"
		"\n"
		"[hashranges]\n"
		"A0000003,512,512 = 256,256\n"
		"\n"
		"[filtering]\n"
		"A0000001C000000110000001 = nearest\n";

	FILE *f = File::OpenCFile(packDir / "textures.ini", "w");
	if (!f) {
		return false;
	}
	fwrite(iniContent, 1, strlen(iniContent), f);
	fclose(f);

	if (!CreateTestPNG(packDir / "tex_a.png", 64, 64, 0xFF0000FF)) return false;
	if (!CreateTestPNG(packDir / "tex_b0.png", 64, 64, 0x00FF00FF)) return false;
	if (!CreateTestPNG(packDir / "tex_b1.png", 32, 32, 0x00FF00FF)) return false;
	if (!CreateTestPNG(packDir / "tex_c.png", 256, 256, 0x0000FFFF)) return false;

	return true;
}

static bool CreateAnimatedTestPack(const Path &packDir) {
	File::DeleteDirRecursively(packDir);
	if (!File::CreateDir(packDir)) {
		return false;
	}

	static const char *iniContent =
		"[options]\n"
		"hash = quick\n"
		"version = 1\n"
		"\n"
		"[hashes]\n"
		"A0000009C000000910000009 = test/mytexture\n"
		"A000000AC000000A10000000A = test/mytexture_noloop\n";
	FILE *f = File::OpenCFile(packDir / "textures.ini", "w");
	if (!f) {
		return false;
	}
	fwrite(iniContent, 1, strlen(iniContent), f);
	fclose(f);

	if (!File::CreateDir(packDir / "test")) return false;
	if (!File::CreateDir(packDir / "test/mytexture")) return false;
	if (!File::CreateDir(packDir / "test/mytexture_noloop")) return false;

	static const char *animIni =
		"[Animation]\n"
		"fps = 10\n"
		"loop = true\n";
	FILE *ini = File::OpenCFile(packDir / "test/mytexture/texture.ini", "w");
	if (!ini) {
		return false;
	}
	fwrite(animIni, 1, strlen(animIni), ini);
	fclose(ini);

	static const char *animIniNoLoop =
		"[Animation]\n"
		"fps = 10\n"
		"loop = false\n";
	FILE *ini2 = File::OpenCFile(packDir / "test/mytexture_noloop/texture.ini", "w");
	if (!ini2) {
		return false;
	}
	fwrite(animIniNoLoop, 1, strlen(animIniNoLoop), ini2);
	fclose(ini2);

	if (!CreateTestPNG(packDir / "test/mytexture/0.png", 64, 64, 0xFF0000FF)) return false;
	if (!CreateTestPNG(packDir / "test/mytexture/1.png", 64, 64, 0x00FF00FF)) return false;
	if (!CreateTestPNG(packDir / "test/mytexture/2.png", 64, 64, 0x0000FFFF)) return false;

	if (!CreateTestPNG(packDir / "test/mytexture_noloop/0.png", 64, 64, 0xFF0000FF)) return false;
	if (!CreateTestPNG(packDir / "test/mytexture_noloop/1.png", 64, 64, 0x00FF00FF)) return false;
	if (!CreateTestPNG(packDir / "test/mytexture_noloop/2.png", 64, 64, 0x0000FFFF)) return false;

	return true;
}

static bool TestLookups(TextureReplacer *replacer) {
	// Key A: single mip, found.
	ReplacedTexture *texA = replacer->FindReplacement(ReplacementCacheKey(KEY_A, HASH_A), 64, 64);
	EXPECT_TRUE(texA != nullptr);

	// Key B: two mip levels, found.
	ReplacedTexture *texB = replacer->FindReplacement(ReplacementCacheKey(KEY_B, HASH_B), 64, 64);
	EXPECT_TRUE(texB != nullptr);

	// Key C: found via hashrange.
	ReplacedTexture *texC = replacer->FindReplacement(ReplacementCacheKey(KEY_C, HASH_C), 512, 512);
	EXPECT_TRUE(texC != nullptr);

	// Key D: explicitly ignored (empty filename).
	EXPECT_TRUE(replacer->FindReplacement(ReplacementCacheKey(KEY_D, HASH_D), 16, 16) == nullptr);

	// Key E: file missing, still creates a texture object (loads as NOT_FOUND).
	ReplacedTexture *texE = replacer->FindReplacement(ReplacementCacheKey(KEY_E, HASH_E), 16, 16);
	EXPECT_TRUE(texE != nullptr);

	// Unknown key: not in the ini at all.
	ReplacementCacheKey unknownKey(0x2000000020000000ULL, 0x20000000);
	EXPECT_TRUE(replacer->FindReplacement(unknownKey, 16, 16) == nullptr);

	if (!texA || !texB || !texC || !texE) {
		return false;
	}

	// Load the actual textures using the thread manager.
	g_threadManager.Init(1, 1);

	// Key A: 64x64 single level, nearest filtering.
	EXPECT_TRUE(texA->Poll(1.0));
	EXPECT_EQ_INT(texA->NumLevels(), 1);
	int w = 0, h = 0;
	texA->GetSize(0, &w, &h);
	EXPECT_EQ_INT(w, 64);
	EXPECT_EQ_INT(h, 64);
	TextureFiltering filtering = TEX_FILTER_AUTO;
	EXPECT_TRUE(texA->ForceFiltering(&filtering));
	EXPECT_TRUE(filtering == TEX_FILTER_FORCE_NEAREST);

	// Key B: two mip levels, 64x64 and 32x32.
	EXPECT_TRUE(texB->Poll(1.0));
	EXPECT_EQ_INT(texB->NumLevels(), 2);
	texB->GetSize(0, &w, &h);
	EXPECT_EQ_INT(w, 64);
	EXPECT_EQ_INT(h, 64);
	texB->GetSize(1, &w, &h);
	EXPECT_EQ_INT(w, 32);
	EXPECT_EQ_INT(h, 32);

	// Key C: hashrange maps 512x512 -> 256x256, upscaled back to 512x512.
	EXPECT_TRUE(texC->Poll(1.0));
	EXPECT_EQ_INT(texC->NumLevels(), 1);
	texC->GetSize(0, &w, &h);
	EXPECT_EQ_INT(w, 512);
	EXPECT_EQ_INT(h, 512);

	// Key E: missing file should end up NOT_FOUND.
	EXPECT_TRUE(texE->Poll(1.0));
	EXPECT_TRUE(texE->State() == ReplacementState::NOT_FOUND);

	g_threadManager.Teardown();
	return true;
}

static bool TestAnimatedTexture(TextureReplacer *replacer) {
	ReplacementCacheKey animKey(0xA0000009C0000009ULL, 0x10000009);
	ReplacedTexture *tex = replacer->FindReplacement(animKey, 64, 64);
	EXPECT_TRUE(tex != nullptr);
	EXPECT_TRUE(tex->IsAnimated());
	EXPECT_EQ_INT(tex->AnimationFrameCount(), 3);
	EXPECT_EQ_FLOAT(tex->AnimationFPS(), 10.0f);
	EXPECT_TRUE(tex->AnimationLoops());
	if (!tex) {
		return false;
	}
	EXPECT_TRUE(tex->Poll(1.0));
	EXPECT_EQ_INT(tex->AnimationCurrentFrame(), 0);
	
	// Test animation timing and frame advancement
	// FPS = 10, so each frame takes 0.1 seconds
	// Frame 0 initially
	
	// Advance by 0.05 seconds (half a frame) - should stay at frame 0
	tex->UpdateAnimation(0.05);
	EXPECT_EQ_INT(tex->AnimationCurrentFrame(), 0);
	
	// Advance by another 0.06 seconds (total 0.11) - should move to frame 1
	tex->UpdateAnimation(0.06);
	EXPECT_EQ_INT(tex->AnimationCurrentFrame(), 1);
	
	// Advance by 0.2 seconds (should skip frames) - from frame 1 + 0.2 = frame 3
	// But we only have 3 frames (0, 1, 2), so with loop it wraps to frame 0
	tex->UpdateAnimation(0.2);
	EXPECT_EQ_INT(tex->AnimationCurrentFrame(), 0);  // 1 + 2 frames = frame 3 % 3 = frame 0
	
	return true;
}

static bool TestNonLoopingAnimatedTexture(TextureReplacer *replacer) {
	ReplacementCacheKey animKey(0xA000000AC000000AULL, 0x1000000A);
	ReplacedTexture *tex = replacer->FindReplacement(animKey, 64, 64);
	EXPECT_TRUE(tex != nullptr);
	EXPECT_TRUE(tex->IsAnimated());
	EXPECT_EQ_INT(tex->AnimationFrameCount(), 3);
	EXPECT_EQ_FLOAT(tex->AnimationFPS(), 10.0f);
	EXPECT_TRUE(!tex->AnimationLoops());  // Should not loop
	if (!tex) {
		return false;
	}
	EXPECT_TRUE(tex->Poll(1.0));
	EXPECT_EQ_INT(tex->AnimationCurrentFrame(), 0);
	EXPECT_TRUE(tex->AnimationCurrentFrame() == 0 || tex->AnimationCurrentFrame() == 1);  // Allow poll to advance
	
	// Reset to known state
	tex->ResetAnimation();
	EXPECT_EQ_INT(tex->AnimationCurrentFrame(), 0);
	
	// Advance to frame 2 (last frame)
	tex->UpdateAnimation(0.25);  // 2.5 frames = frame 2
	EXPECT_EQ_INT(tex->AnimationCurrentFrame(), 2);
	EXPECT_TRUE(tex->AnimationCurrentFrame() < 3);  // Sanity check
	
	// Advance past last frame - should stay at frame 2 and become inactive
	tex->UpdateAnimation(0.15);  // +1.5 frames, would be frame 3 but clamped to 2
	EXPECT_EQ_INT(tex->AnimationCurrentFrame(), 2);
	// Note: AnimationInstance has 'active' flag but no getter for it
	
	return true;
}

bool TestTextureReplacer() {
	Path packDir = Path("unittest_texture_pack");
	if (!CreateTestPack(packDir)) {
		return false;
	}

	TextureReplacer replacer(nullptr);
	std::string error;
	if (!replacer.LoadPackForTesting(packDir, &error)) {
		ERROR_LOG(Log::G3D, "Failed to load test texture pack: %s", error.c_str());
		File::DeleteDirRecursively(packDir);
		return false;
	}

	if (!TestLookups(&replacer)) {
		File::DeleteDirRecursively(packDir);
		return false;
	}

	File::DeleteDirRecursively(packDir);

	Path animatedPackDir = Path("unittest_animated_texture_pack");
	if (!CreateAnimatedTestPack(animatedPackDir)) {
		return false;
	}

	TextureReplacer animatedReplacer(nullptr);
	if (!animatedReplacer.LoadPackForTesting(animatedPackDir, &error)) {
		ERROR_LOG(Log::G3D, "Failed to load animated test texture pack: %s", error.c_str());
		File::DeleteDirRecursively(animatedPackDir);
		return false;
	}

	if (!TestAnimatedTexture(&animatedReplacer)) {
		File::DeleteDirRecursively(animatedPackDir);
		return false;
	}

	if (!TestNonLoopingAnimatedTexture(&animatedReplacer)) {
		File::DeleteDirRecursively(animatedPackDir);
		return false;
	}

	File::DeleteDirRecursively(animatedPackDir);
	return true;
}
