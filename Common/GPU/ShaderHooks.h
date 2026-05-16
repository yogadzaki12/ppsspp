#pragma once

#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

class ShaderWriter;

namespace ppsspp::shaderhooks {

enum class ShaderStage {
	Vertex,
	Fragment,
};

enum class HookPoint {
	BeforeTexture,
	AfterTexture,
	BeforeLighting,
	AfterLighting,
	BeforeFog,
	AfterFog,
	BeforeBlend,
	AfterBlend,
	BeforeFinalColor,
};

enum class CommandType {
	Unknown = 0,
	ReduceBloom,
	ClampLighting,
	ReduceSpecular,
	ReduceGlow,
	ReduceEmission,
	ReduceOverbright,
	ReduceAdditiveBlend,
	ReduceAlphaBlend,
	ForceOpaque,
	ReduceParticles,
	DisableParticles,
	ReduceParticleBrightness,
	ReduceFogDensity,
	DisableFog,
	ClampFogBrightness,
	DesaturateTexture,
	ReduceTextureBrightness,
	SharpenTexture,
	BlurTexture,
	Saturation,
	Contrast,
	Gamma,
	Tint,
	ClampHDR,
	ReduceFramebufferGlow,
	LimitFinalBrightness,
};

struct HookContext {
	std::string hookName;
	std::string author;
	std::string gameId;
	ShaderStage stage = ShaderStage::Fragment;
	HookPoint point = HookPoint::BeforeFinalColor;
	bool enabled = true;
};

class ShaderHookCommand {
public:
	virtual ~ShaderHookCommand() = default;

	virtual CommandType Type() const = 0;
	virtual const std::string &Name() const = 0;
	virtual const std::vector<float> &Arguments() const = 0;
	virtual bool Validate(std::string *error) const = 0;
	virtual std::string Describe() const = 0;
	virtual void Execute(std::ostream &out) const = 0;
};

class ParsedShaderHookCommand final : public ShaderHookCommand {
public:
	ParsedShaderHookCommand(CommandType type, std::string name, std::vector<float> arguments);

	CommandType Type() const override;
	const std::string &Name() const override;
	const std::vector<float> &Arguments() const override;
	bool Validate(std::string *error) const override;
	std::string Describe() const override;
	void Execute(std::ostream &out) const override;

private:
	CommandType type_ = CommandType::Unknown;
	std::string name_;
	std::vector<float> arguments_;
};

struct ShaderHookDefinition {
	HookContext context;
	std::filesystem::path sourcePath;
	std::vector<std::unique_ptr<ShaderHookCommand>> commands;
	std::vector<std::string> warnings;
};

struct HookParseResult {
	bool success = false;
	std::string error;
	ShaderHookDefinition hook;
};

class HookParser {
public:
	HookParseResult ParseFile(const std::filesystem::path &path) const;
	HookParseResult ParseText(const std::string &text, const std::filesystem::path &sourcePath = {}) const;
};

class HookFileLoader {
public:
	explicit HookFileLoader(std::filesystem::path root = std::filesystem::path("PSP/Shaders"));

	std::vector<std::filesystem::path> Scan(bool recursive = false) const;
	std::vector<HookParseResult> LoadAll(const HookParser &parser, bool recursive = false) const;

	// Scan and load from multiple predefined paths (PSP/Shaders, current dir, user config dir)
	static std::vector<HookParseResult> ScanAndLoadDefaultPaths(const HookParser &parser, bool recursive = false);

private:
	std::filesystem::path root_;
};

std::vector<HookParseResult> LoadShaderHooksFromDisk(bool recursive = false);
std::vector<HookParseResult> LoadShaderHooksFromDisk(const std::filesystem::path &basePath, bool recursive = false);
const std::vector<ShaderHookDefinition> &GetLoadedShaderHooks();

class ShaderHookExecutor {
public:
	void Execute(const ShaderHookDefinition &hook, std::ostream &out) const;
};

void WriteFragmentHookTransforms(ShaderWriter &writer, std::string_view gameId, const char *colorVar);
// Variant that targets a specific hook point in the fragment shader generation.
void WriteFragmentHookTransforms(ShaderWriter &writer, std::string_view gameId, const char *colorVar, HookPoint point);

// Vertex-stage hook writer. Targets vertex shader variables (variable name passed as `varName`).
void WriteVertexHookTransforms(ShaderWriter &writer, std::string_view gameId, const char *varName, HookPoint point);

const char *ToString(ShaderStage stage);
const char *ToString(HookPoint point);
const char *ToString(CommandType type);

}  // namespace ppsspp::shaderhooks