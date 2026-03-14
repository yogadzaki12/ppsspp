// depth_pyramid.csh — Min-depth downsampler for Hierarchical SSR
// Dispatched 3x to build mip levels 1-3 of the depth pyramid:
//   Pass 0: srcTex = framebuffer depth (full-res), dstImg = pyramid[0] (w/2)
//   Pass 1: srcTex = pyramid[0],                  dstImg = pyramid[1] (w/4)
//   Pass 2: srcTex = pyramid[1],                  dstImg = pyramid[2] (w/8)
#version 310 es
precision highp float;

layout(local_size_x = 8, local_size_y = 8) in;

// Source: previous mip level (depth tex or pyramid RGBA8, depth in .r)
layout(binding = 1) uniform highp sampler2D srcTex;

// Destination: current pyramid level (RGBA8, min-depth stored in R channel)
layout(rgba8, binding = 0) writeonly uniform highp image2D dstImg;

void main() {
	ivec2 dstPx = ivec2(gl_GlobalInvocationID.xy);
	ivec2 dstSz = imageSize(dstImg);
	if (dstPx.x >= dstSz.x || dstPx.y >= dstSz.y) {
		return;
	}

	// 2×2 footprint in source texture
	ivec2 srcPx = dstPx * 2;
	ivec2 srcSz = textureSize(srcTex, 0) - ivec2(1);

	float d00 = texelFetch(srcTex, min(srcPx + ivec2(0, 0), srcSz), 0).r;
	float d10 = texelFetch(srcTex, min(srcPx + ivec2(1, 0), srcSz), 0).r;
	float d01 = texelFetch(srcTex, min(srcPx + ivec2(0, 1), srcSz), 0).r;
	float d11 = texelFetch(srcTex, min(srcPx + ivec2(1, 1), srcSz), 0).r;

	// Conservative minimum: keep the closest (smallest) depth value
	float minDepth = min(min(d00, d10), min(d01, d11));
	imageStore(dstImg, dstPx, vec4(minDepth, 0.0, 0.0, 1.0));
}
