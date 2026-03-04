// Shadertoy-style reflection shader for PPSSPP.
// Designed to be lightweight and compatible with low-end GPUs.

#ifdef GL_ES
precision mediump float;
precision mediump int;
#endif

uniform sampler2D sampler0;
varying vec2 v_texcoord0;
uniform vec4 u_time;
uniform vec4 u_setting;

float luma(vec3 c) {
  return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

float sat(vec3 c) {
  float mn = min(c.r, min(c.g, c.b));
  float mx = max(c.r, max(c.g, c.b));
  return mx - mn;
}

void main() {
  vec2 uv = v_texcoord0;
  vec3 base = texture2D(sampler0, uv).rgb;

  // User controls.
  float strength = clamp(u_setting.x, 0.0, 1.0);
  float ripple = mix(0.2, 2.5, clamp(u_setting.y, 0.0, 1.0));
  float blurAmt = mix(0.0005, 0.0080, clamp(u_setting.z, 0.0, 1.0));
  float uiCut = mix(0.08, 0.45, clamp(u_setting.w, 0.0, 1.0));

  // Reflection starts below this line.
  float horizon = 0.46;
  float below = max(uv.y - horizon, 0.0);
  float mask = smoothstep(0.0, 0.02, below) * (1.0 - smoothstep(0.02, 0.60, below));

  // Horizontal wave distortion.
  float t = u_time.x;
  float wave = sin((uv.y * 90.0) - t * (2.0 + ripple * 2.0)) * (0.0010 + ripple * 0.0025);
  wave += sin((uv.y * 180.0) + t * 1.3) * (0.0005 + ripple * 0.0015);

  // Mirror top half onto bottom half.
  vec2 ruv = vec2(uv.x + wave, horizon - below * 0.95);
  ruv.y = clamp(ruv.y, 0.0, 1.0);

  // Exclude likely UI/HUD source regions from reflection.
  float uiMaskTop = smoothstep(uiCut, uiCut + 0.04, ruv.y);
  float uiMaskSide = smoothstep(0.02, 0.08, ruv.x) * (1.0 - smoothstep(0.92, 0.98, ruv.x));
  float uiMask = uiMaskTop * uiMaskSide;

  vec3 srcC = texture2D(sampler0, ruv).rgb;

  // Small blur to mimic watery reflection softness.
  vec3 refl = srcC * 0.40;
  refl += texture2D(sampler0, ruv + vec2(blurAmt, 0.0)).rgb * 0.20;
  refl += texture2D(sampler0, ruv - vec2(blurAmt, 0.0)).rgb * 0.20;
  refl += texture2D(sampler0, ruv + vec2(0.0, blurAmt * 0.75)).rgb * 0.10;
  refl += texture2D(sampler0, ruv - vec2(0.0, blurAmt * 0.75)).rgb * 0.10;

  // Object-sensitive boost from mirrored scene detail.
  float lumC = luma(srcC);
  float lumL = luma(texture2D(sampler0, ruv + vec2(-blurAmt * 1.5, 0.0)).rgb);
  float lumR = luma(texture2D(sampler0, ruv + vec2( blurAmt * 1.5, 0.0)).rgb);
  float lumU = luma(texture2D(sampler0, ruv + vec2(0.0, -blurAmt * 1.5)).rgb);
  float lumD = luma(texture2D(sampler0, ruv + vec2(0.0,  blurAmt * 1.5)).rgb);

  float edge = clamp(abs(lumL - lumR) + abs(lumU - lumD), 0.0, 1.0);
  float objDetail = smoothstep(0.04, 0.30, edge + sat(srcC) * 0.6 + lumC * 0.25);

  // Make bright areas reflect slightly stronger.
  float bright = smoothstep(0.25, 1.0, luma(refl));
  float boost = mix(0.75, 1.25, bright);

  vec3 color = base + refl * (mask * uiMask * strength * boost * mix(0.55, 1.60, objDetail));
  gl_FragColor = vec4(color, 1.0);
}
