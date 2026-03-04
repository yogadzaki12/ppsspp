// Ported and adapted from Shadertoy 4tfSRn: "simple godrays in screenspace" by public_int_i.
// This version applies as a PPSSPP post-processing shader on top of the game frame.

#ifdef GL_ES
precision mediump float;
precision mediump int;
#endif

uniform sampler2D sampler0;
varying vec2 v_texcoord0;
uniform vec4 u_setting;

const vec2 SUN_POS = vec2(0.5, 0.5);
const vec3 SUN_COLOR = vec3(1.0, 0.95, 0.95);
const int GODRAY_ITER = 32;
const float GODRAY_DECAY = 0.96;

float luminance(vec3 color) {
    return dot(color, vec3(0.2126, 0.7152, 0.0722));
}

void main() {
    vec2 uv = v_texcoord0.xy;
    vec3 baseColor = texture2D(sampler0, uv).rgb;

    float sunSize = max(0.01, u_setting.y);
    float godrayReach = max(sunSize + 0.01, u_setting.z);
    float godrayIntensity = max(0.0, u_setting.x);
    float threshold = clamp(u_setting.w, 0.0, 1.0);

    vec2 sunDir = SUN_POS - uv;
    float sunLen = length(sunDir);
    vec3 color = baseColor;

    if (sunLen < godrayReach) {
        sunDir = normalize(sunDir);
        float reachFactor = 1.0 - sunLen / godrayReach;
        float godrayStep = (godrayReach * 0.5) / float(GODRAY_ITER);
        vec2 sampleUV = uv + sunDir * max(0.0, sunLen - sunSize);
        float cl = 0.0;
        float weight = 1.0;
        int maxIter = int(reachFactor * float(GODRAY_ITER));

        for (int i = 0; i < GODRAY_ITER; i++) {
            if (i > maxIter) {
                break;
            }

            vec3 sampleColor = texture2D(sampler0, sampleUV).rgb;
            float lightMask = smoothstep(threshold, 1.0, luminance(sampleColor));
            cl += lightMask * weight;
            weight *= GODRAY_DECAY;

            sampleUV += sunDir * godrayStep;
        }

        float rays = cl * godrayIntensity * reachFactor;
        color += min(1.0, rays) * SUN_COLOR;
    }

    float sunDisk = 1.0 - smoothstep(sunSize * 0.5, sunSize, sunLen);
    color += SUN_COLOR * sunDisk * godrayIntensity * 0.2;

    gl_FragColor = vec4(color, 1.0);
}