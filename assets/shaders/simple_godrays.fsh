// Ported and adapted from Shadertoy 4tfSRn: "simple godrays in screenspace" by public_int_i.
// This PPSSPP version is object-based: rays are extracted from bright game pixels.

#ifdef GL_ES
precision mediump float;
precision mediump int;
#endif

uniform sampler2D sampler0;
varying vec2 v_texcoord0;
uniform vec4 u_setting;

const int GODRAY_ITER = 20;
const vec3 RAY_TINT = vec3(1.0, 0.95, 0.95);

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

float brightMask(vec3 color, float threshold) {
    return max(0.0, luminance(color) - threshold) / max(0.0001, 1.0 - threshold);
}

vec3 accumulateRays(vec2 uv, vec2 dir, float stepSize, float threshold, float decay, int maxIter) {
    vec3 sum = vec3(0.0);
    float weight = 1.0;

    for (int i = 1; i <= GODRAY_ITER; i++) {
        if (i > maxIter) {
            break;
        }

        vec2 offset = dir * stepSize * float(i);
        vec3 sampleA = texture2D(sampler0, uv + offset).rgb;
        vec3 sampleB = texture2D(sampler0, uv - offset).rgb;

        sum += sampleA * brightMask(sampleA, threshold) * weight;
        sum += sampleB * brightMask(sampleB, threshold) * weight;
        weight *= decay;
    }

    return sum;
}

void main() {
    vec2 uv = v_texcoord0.xy;
    vec3 baseColor = texture2D(sampler0, uv).rgb;

    float intensity = max(0.0, u_setting.x);
    float sampleStep = max(0.00035, u_setting.y * 0.003);
    float rayLength = max(0.1, u_setting.z);
    float threshold = clamp(u_setting.w, 0.0, 1.0);
    float decay = mix(0.88, 0.97, clamp(rayLength / 1.5, 0.0, 1.0));
    int maxIter = int(float(GODRAY_ITER) * clamp(rayLength / 1.5, 0.2, 1.0));

    vec3 rays = vec3(0.0);
    rays += accumulateRays(uv, vec2(1.0, 0.0), sampleStep, threshold, decay, maxIter);
    rays += accumulateRays(uv, vec2(0.0, 1.0), sampleStep, threshold, decay, maxIter);
    rays += accumulateRays(uv, normalize(vec2(1.0, 1.0)), sampleStep, threshold, decay, maxIter);
    rays += accumulateRays(uv, normalize(vec2(1.0, -1.0)), sampleStep, threshold, decay, maxIter);

    vec3 color = baseColor + rays * (intensity / float(GODRAY_ITER)) * 0.5 * RAY_TINT;

    gl_FragColor = vec4(color, 1.0);
}