#version 440
layout(location = 0) in vec2 corner;
layout(location = 1) in vec4 instance;
layout(location = 2) in vec2 flags;
layout(location = 0) out vec2 uv;
layout(location = 1) out vec2 worldPx;
layout(location = 2) out vec2 spriteFlags;
layout(std140, binding = 0) uniform DrawUniforms {
    mat4 matrix;
    vec4 tint;
    vec4 rect;
    vec4 atlasAndOffset;
    vec4 lightRect;
    vec4 options;
};
void main()
{
    int mode = int(options.z + 0.5);
    vec2 p;
    uv = vec2(0);
    spriteFlags = flags;
    if (mode == 0) {
        p = instance.xy + atlasAndOffset.zw + corner * 32.0;
        uv = (instance.zw + vec2(0.5) + corner * 31.0) / atlasAndOffset.xy;
    } else if (mode == 1) {
        p = instance.xy + corner * instance.zw;
    } else if (mode == 2) {
        p = mix(rect.xy, rect.zw, corner);
    } else if (mode == 3) {
        p = vec2(corner.x * 2.0 - 1.0, 1.0 - corner.y * 2.0);
        uv = rect.xy + corner * rect.zw;
        if (options.y > 0.5) uv.y = 1.0 - uv.y;
    } else {
        p = corner;
    }
    worldPx = p;
    gl_Position = matrix * vec4(p, 0, 1);
}
