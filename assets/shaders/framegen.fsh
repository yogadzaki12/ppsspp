#ifdef GL_ES
precision mediump float;
precision mediump int;
#endif

uniform sampler2D sampler0;
uniform sampler2D sampler2;
uniform vec4 u_setting;
varying vec2 v_texcoord0;

float luma(vec3 c) {
  return dot(c, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
  vec2 uv = v_texcoord0;

  // Current and previous frame.
  vec3 cur = texture2D(sampler0, uv).rgb;
  vec3 prev = texture2D(sampler2, uv).rgb;

  // User settings.
  float blendAmt = clamp(u_setting.x, 0.0, 1.0);      // overall framegen strength
  float extrapAmt = clamp(u_setting.y, 0.0, 1.0);     // simple motion extrapolation
  float ghostCut = mix(0.005, 0.12, clamp(u_setting.z, 0.0, 1.0));

  // Motion estimate from temporal luminance difference.
  float d = abs(luma(cur) - luma(prev));
  float motion = smoothstep(ghostCut, ghostCut * 3.5, d);

  // Interpolated frame candidate.
  vec3 interp = mix(prev, cur, 0.5);

  // Lightweight extrapolation to reduce perceived lag during fast motion.
  vec3 delta = cur - prev;
  vec3 predicted = clamp(cur + delta * (0.35 * extrapAmt), 0.0, 1.0);

  // In static areas, stay close to current frame to avoid blur/ghosting.
  vec3 temporal = mix(interp, predicted, motion);
  float amount = blendAmt * mix(0.25, 1.0, motion);

  vec3 outColor = mix(cur, temporal, amount);
  gl_FragColor = vec4(outColor, 1.0);
}
