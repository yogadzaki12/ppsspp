// Ported from Shadertoy 4tfSRn: "simple godrays in screenspace" by public_int_i.

#ifdef GL_ES
precision mediump float;
precision mediump int;
#endif

uniform sampler2D sampler0;
varying vec2 v_texcoord0;
uniform vec4 u_time;
uniform vec4 u_setting;

const vec2 SUN_POS = vec2(0.5, 0.5);
const vec3 SUN_COLOR = vec3(1.0, 0.95, 0.95);
const vec3 BACKGROUND_COLOR = vec3(0.0);
const int GODRAY_ITER = 32;

float occlusionMap(vec2 uv, vec2 occlusionSize) {
    vec2 occlusionLoc = vec2(sin(u_time.x * 0.6) * 0.2 + 0.5, cos(u_time.x * 0.76) * 0.2 + 0.5);
    float d = length(max(abs(uv - occlusionLoc) - occlusionSize, vec2(0.0)));
    d = max(-(length(mod(uv - occlusionLoc, occlusionSize * 0.5) - occlusionSize * 0.25) - occlusionSize.x * 0.5), d);
    return floor(1.03 - d);
}

void main() {
    vec2 uv = v_texcoord0.xy;

    float sunSize = max(0.02, u_setting.y);
    float godrayReach = max(sunSize + 0.01, u_setting.z);
    float godrayIntensity = max(0.0, u_setting.x);
    vec2 occlusionSize = vec2(max(0.01, u_setting.w));
    float godrayStep = (sunSize * 0.5) / float(GODRAY_ITER);

    float cl = occlusionMap(uv, occlusionSize);
    vec2 sunDir = SUN_POS - uv;
    float sunLen = length(sunDir);

    if (sunLen < sunSize) {
        gl_FragColor = vec4(mix(SUN_COLOR, vec3(cl * 0.3), cl), 1.0);
        return;
    }

    vec3 c = mix(BACKGROUND_COLOR, vec3(cl * 0.3), cl);
    cl = 0.0;

    if (sunLen < godrayReach) {
        sunDir = normalize(sunDir);
        uv += sunDir * max(0.0, (sunLen - sunSize));
        sunLen = 1.0 - sunLen / godrayReach;
        int maxIter = int(sunLen * float(GODRAY_ITER));

        for (int i = 0; i < GODRAY_ITER; i++) {
            cl += max(0.0, 1.0 - occlusionMap(uv, occlusionSize)) * sunLen;

            if (i > maxIter) {
                break;
            }

            uv += sunDir * godrayStep;
        }

        cl *= godrayIntensity;
        c += min(1.0, cl) * SUN_COLOR;
    }

    gl_FragColor = vec4(c, 1.0);
}