#version 310 es

layout(local_size_x = 8, local_size_y = 8) in;

layout(binding = 0) uniform sampler2D colorTex;
layout(binding = 1) uniform highp sampler2D depthTex;
layout(rgba8, binding = 0) writeonly uniform highp image2D outImage;

uniform vec2 u_texelDelta;
uniform float radius;
uniform float intensity;

float sampleDepth(vec2 uv) {
	return texture(depthTex, uv).r;
}

void main() {
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	ivec2 imageSizePx = imageSize(outImage);
	if (pixel.x >= imageSizePx.x || pixel.y >= imageSizePx.y) {
		return;
	}

	vec2 uv = (vec2(pixel) + vec2(0.5)) / vec2(imageSizePx);
	vec4 color = texture(colorTex, uv);
	float centerDepth = sampleDepth(uv);
	float ao = 0.0;
	float taps = 0.0;
	float stepRadius = max(radius, 0.5);

	for (int dir = 0; dir < 8; ++dir) {
		float angle = 6.28318530718 * float(dir) / 8.0;
		vec2 direction = vec2(cos(angle), sin(angle));
		for (int stepIndex = 1; stepIndex <= 4; ++stepIndex) {
			float scale = float(stepIndex) * stepRadius * 0.25;
			vec2 offset = direction * u_texelDelta * scale;
			float sampleZ = sampleDepth(clamp(uv + offset, vec2(0.0), vec2(1.0)));
			float delta = max(sampleZ - centerDepth, 0.0);
			ao += smoothstep(0.0, 0.02 + stepRadius * 0.0015, delta);
			taps += 1.0;
		}
	}

	float occlusion = 1.0 - clamp((ao / max(taps, 1.0)) * intensity, 0.0, 1.0);
	imageStore(outImage, pixel, vec4(color.rgb * occlusion, color.a));
}