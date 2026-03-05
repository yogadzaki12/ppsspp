// Cubemap-style glossy reflection shader for PPSSPP.
// Inspired by Cubemaps.glsl but adapted to PPSSPP post-processing inputs.

#ifdef GL_ES
precision mediump float;
precision mediump int;
#endif

uniform sampler2D sampler0;
uniform vec4 u_time;
uniform vec4 u_setting;
varying vec2 v_texcoord0;

float luma(vec3 c) {
  return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

vec3 sampleEnv(vec2 dir) {
  // Approximate environment from the current frame with a curved lookup.
  vec2 uv = vec2(0.5) + dir * vec2(0.42, 0.30);
  uv = clamp(uv, 0.0, 1.0);
  return texture2D(sampler0, uv).rgb;
}

void main() {
  vec2 uv = v_texcoord0;
  vec3 base = texture2D(sampler0, uv).rgb;

  float strength = clamp(u_setting.x, 0.0, 1.0);
  float roughness = clamp(u_setting.y, 0.0, 1.0);

  // Reflection area (bottom half), with gentle falloff.
  float horizon = 0.48;
  float below = max(uv.y - horizon, 0.0);
  float mask = smoothstep(0.0, 0.03, below) * (1.0 - smoothstep(0.04, 0.65, below));

  // Distorted mirrored source coordinates.
  float t = u_time.x;
  float wave = sin(uv.y * 95.0 - t * 2.2) * (0.0012 + roughness * 0.0020);
  wave += sin(uv.y * 165.0 + t * 1.4) * (0.0007 + roughness * 0.0011);
  vec2 srcUV = vec2(uv.x + wave, horizon - below * 0.96);
  srcUV = clamp(srcUV, 0.0, 1.0);

  // UI cut disabled by request.
  float uiMask = 1.0;

  // Construct a pseudo normal from local luminance gradient.
  float px = 1.0 / 480.0;
  float py = 1.0 / 272.0;
  float l = luma(texture2D(sampler0, srcUV + vec2(-px, 0.0)).rgb);
  float r = luma(texture2D(sampler0, srcUV + vec2( px, 0.0)).rgb);
  float u = luma(texture2D(sampler0, srcUV + vec2(0.0, -py)).rgb);
  float d = luma(texture2D(sampler0, srcUV + vec2(0.0,  py)).rgb);
  vec3 n = normalize(vec3((l - r) * 2.0, (u - d) * 2.0, 1.0));

  // View direction pointing into the screen.
  vec3 v = normalize(vec3((uv - 0.5) * vec2(1.4, 0.9), -1.0));
  vec3 ref = reflect(v, n);

  // Pseudo environment lookup.
  vec3 env = sampleEnv(ref.xy);

  // Fresnel-like boost for glancing angles.
  float fre = 0.2 + 0.8 * pow(1.0 - clamp(dot(n, -v), 0.0, 1.0), 4.0);

  // Simple roughness blur.
  float rb = mix(0.0005, 0.006, roughness);
  env = env * 0.5
      + texture2D(sampler0, clamp(srcUV + ref.xy * rb, 0.0, 1.0)).rgb * 0.25
      + texture2D(sampler0, clamp(srcUV - ref.xy * rb, 0.0, 1.0)).rgb * 0.25;

  // Favor detailed/colored objects.
  vec3 src = texture2D(sampler0, srcUV).rgb;
  float sat = max(src.r, max(src.g, src.b)) - min(src.r, min(src.g, src.b));
  float detail = clamp(abs(l - r) + abs(u - d) + sat * 0.8, 0.0, 1.0);

  vec3 color = base + env * (mask * uiMask * strength * fre * mix(0.45, 1.45, detail));
  gl_FragColor = vec4(color, 1.0);
}
