// ssr.csh — Hierarchical Screen-Space Reflections (GLES 3.1 compute)
// Uses a 4-level depth min-pyramid for coarse-to-fine ray traversal.
//
// Depth levels (separate single-mip textures):
//   depthMip0  — full resolution  (framebuffer depth)
//   depthMip1  — 1/2  resolution  (RGBA8 pyramid, depth in R)
//   depthMip2  — 1/4  resolution
//   depthMip3  — 1/8  resolution
//
// Configurable uniforms: u_steps (default 16), u_intensity, u_stride
#version 310 es
precision highp float;

layout(local_size_x = 8, local_size_y = 8) in;

// ---- Samplers ---------------------------------------------------------------
layout(binding = 0) uniform highp sampler2D colorTex;   // Full-res colour
layout(binding = 1) uniform highp sampler2D depthMip0;  // Full-res depth
layout(binding = 2) uniform highp sampler2D depthMip1;  // Half-res depth
layout(binding = 3) uniform highp sampler2D depthMip2;  // Quarter-res depth
layout(binding = 4) uniform highp sampler2D depthMip3;  // Eighth-res depth

// ---- Output image -----------------------------------------------------------
layout(rgba8, binding = 0) writeonly uniform highp image2D outTex;

// ---- Uniforms ---------------------------------------------------------------
uniform vec2  u_resolution;   // Full render resolution (pixels)
uniform int   u_steps;        // Total hierarchical march steps  (default 16)
uniform float u_intensity;    // Reflection blend factor [0, 1]  (default 0.5)
uniform float u_stride;       // Base UV step per level-0 step   (default 0.02)

// -----------------------------------------------------------------------------
// Sample min-depth from the correct pyramid level
float sampleDepthMip(vec2 uv, int level) {
	if (level <= 0) return textureLod(depthMip0, uv, 0.0).r;
	if (level == 1) return textureLod(depthMip1, uv, 0.0).r;
	if (level == 2) return textureLod(depthMip2, uv, 0.0).r;
	return                textureLod(depthMip3, uv, 0.0).r;
}

void main() {
	ivec2 pixel = ivec2(gl_GlobalInvocationID.xy);
	ivec2 outSz  = imageSize(outTex);
	if (pixel.x >= outSz.x || pixel.y >= outSz.y) {
		return;
	}

	vec2 uv    = (vec2(pixel) + 0.5) / u_resolution;
	vec3 color = texture(colorTex, uv).rgb;

	// ---- Surface normal from depth finite differences -----------------------
	float centerDepth = textureLod(depthMip0, uv, 0.0).r;
	vec2  texel = 1.0 / u_resolution;
	float dxR   = textureLod(depthMip0, uv + vec2(texel.x, 0.0), 0.0).r - centerDepth;
	float dyU   = textureLod(depthMip0, uv + vec2(0.0, texel.y), 0.0).r - centerDepth;
	vec3  n     = normalize(vec3(-dxR / texel.x, -dyU / texel.y, 1.0));

	// ---- Reflection direction -----------------------------------------------
	// View direction pointing into the screen (simplified for screen-space)
	vec3 viewDir = vec3(0.0, 0.0, -1.0);
	vec3 reflDir = normalize(reflect(viewDir, n));

	// ---- Hierarchical ray march ---------------------------------------------
	// Start at coarsest level (3 = 1/8 res), refine on potential intersection.
	// stepMult scales the per-iteration advance: level 3 → ×8, level 0 → ×1.
	vec2  rayUV   = uv;
	float rayZ    = centerDepth;
	vec3  reflection = vec3(0.0);
	float hitWeight  = 0.0;

	int   level    = 3;
	float stepMult = float(1 << 3);  // 8.0

	for (int i = 0; i < u_steps; i++) {
		vec2  dUV = reflDir.xy * u_stride * stepMult;
		float dZ  = reflDir.z  * u_stride * stepMult * 0.5;

		rayUV += dUV;
		rayZ  += dZ;

		// Clip rays that leave the screen
		if (any(lessThan(rayUV, vec2(0.0))) || any(greaterThan(rayUV, vec2(1.0)))) {
			break;
		}

		float sceneDepth = sampleDepthMip(rayUV, level);

		// Intersection test: scene geometry is closer than the ray position
		if (sceneDepth < rayZ) {
			if (level == 0) {
				// Full-resolution confirmation
				float delta = abs(rayZ - sceneDepth);
				hitWeight   = clamp(1.0 - delta * 30.0, 0.0, 1.0);
				reflection  = texture(colorTex, rayUV).rgb;
				break;
			}
			// Step back, descend to a finer level and retry
			rayUV  -= dUV;
			rayZ   -= dZ;
			level--;
			stepMult = float(1 << level);
		}
	}

	// ---- Screen-edge fade to hide clipped reflections -----------------------
	vec2  ef       = smoothstep(vec2(0.0), vec2(0.1), rayUV) *
	                 (vec2(1.0) - smoothstep(vec2(0.9), vec2(1.0), rayUV));
	float edgeFade = ef.x * ef.y;

	// ---- Fresnel approximation (stronger at grazing angles) -----------------
	float cosA   = clamp(dot(n, -viewDir), 0.0, 1.0);
	float fresnel = mix(0.04, 1.0, pow(1.0 - cosA, 5.0));

	vec3 finalColor = mix(color, reflection,
	                      u_intensity * hitWeight * edgeFade * fresnel);
	imageStore(outTex, pixel, vec4(finalColor, 1.0));
}
