#version 440
layout(location = 0) in vec2 uv;
layout(location = 1) in vec2 worldPx;
layout(location = 2) in vec2 spriteFlags;
layout(location = 0) out vec4 fragColor;
layout(std140, binding = 0) uniform DrawUniforms {
    mat4 matrix;
    vec4 tint;
    vec4 rect;
    vec4 atlasAndOffset;
    vec4 lightRect;
    vec4 options;
};
layout(binding = 1) uniform sampler2D atlas;
layout(binding = 2) uniform sampler2D light;

vec3 zoneColor(vec3 base, float zone)
{
    int f = int(zone + 0.5);
    if ((f & 64) != 0) return mix(base, vec3(0.30, 0.52, 1.0), 0.30);
    if ((f & 1) != 0) return mix(base, vec3(0.22, 1.0, 0.34), 0.44);
    if ((f & 4) != 0) return mix(base, vec3(0.94, 0.30, 0.84), 0.36);
    if ((f & 8) != 0) return mix(base, vec3(1.0, 0.84, 0.24), 0.38);
    if ((f & 16) != 0) return mix(base, vec3(1.0, 0.30, 0.16), 0.38);
    return base;
}
void main()
{
    int mode = int(options.z + 0.5);
    if (mode == 3) {
        fragColor = texture(atlas, uv);
    } else if (mode == 0) {
        vec4 c = texture(atlas, uv);
        if (c.a < 0.01) discard;
        vec3 color = zoneColor(c.rgb * tint.rgb * (spriteFlags.x > 0.5 ? 0.5 : 1.0), spriteFlags.y);
        if (options.x > 0.5) {
            vec2 lightUv = (worldPx / 32.0 - lightRect.xy) / lightRect.zw;
            color *= texture(light, clamp(lightUv, vec2(0), vec2(1))).rgb;
        }
        fragColor = vec4(color, c.a * tint.a);
    } else {
        fragColor = tint;
    }
}
