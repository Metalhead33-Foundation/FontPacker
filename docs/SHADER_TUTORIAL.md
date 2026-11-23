# Shader preview and HLSL tutorial

FontPacker's **Shader test** tab renders the selected glyph through a user
fragment shader. It is a small, deliberately simple preview harness: one
full-screen quad, one SDF texture, and a GLSL 3.30 fragment shader.

## Texture conventions

| Field type | Texture | How to sample |
|---|---|---|
| SDF | one-channel `r8` grayscale | `texture(sdf_tex, TexCoords).r` |
| MSDF | RGB channels (alpha is normally 1) | median of `raw.r`, `raw.g`, `raw.b` |
| MSDFA | RGB = MSDF, A = regular SDF | median for the contour, alpha for glow/bevel |

The contour is around `0.5`. Use `smoothstep` around that midpoint; do not
average the MSDF channels, because the median is what reconstructs the
contour correctly.

The preview vertex shader supplies `in vec2 TexCoords` and the fragment shader
writes `out vec4 FragColor`.

## Minimal GLSL examples

SDF:

```glsl
#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D sdf_tex;
void main() {
    float d = texture(sdf_tex, TexCoords).r;
    FragColor = vec4(1.0, 1.0, 1.0, smoothstep(0.48, 0.52, d));
}
```

MSDF:

```glsl
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}
void main() {
    vec4 raw = texture(sdf_tex, TexCoords);
    float d = median(raw.r, raw.g, raw.b);
    FragColor = vec4(1.0, 1.0, 1.0, smoothstep(0.48, 0.52, d));
}
```

MSDFA glow:

```glsl
void main() {
    vec4 raw = texture(sdf_tex, TexCoords);
    if (raw.a < 1.0 / 255.0) discard;
    float shape = smoothstep(0.47, 0.50, median(raw.r, raw.g, raw.b));
    float glow = smoothstep(0.10, 0.50, raw.a);
    FragColor = vec4(mix(vec3(0.0, 1.0, 1.0) * glow, vec3(0.8, 1.0, 1.0), shape), max(shape, glow));
}
```

## HLSL input and transpilation

Tick **HLSL**, edit the HLSL pane, and press **Transpile**. The built-in
converter runs:

1. **glslang** to compile HLSL to Vulkan SPIR-V.
2. **SPIRV-Cross** to cross-compile that SPIR-V to GLSL 3.30 for the preview.

The result is inserted into the GLSL pane; press **Compile** to upload it to the
preview canvas.

Use this HLSL skeleton so the generated interface matches the preview:

```hlsl
Texture2D<float4> sdf_tex : register(t0);
SamplerState sdf_sampler : register(s0);

float4 main(float2 TexCoords : TEXCOORD0) : SV_Target
{
    float d = sdf_tex.Sample(sdf_sampler, TexCoords).r;
    return float4(1.0, 1.0, 1.0, smoothstep(0.48, 0.52, d));
}
```

For MSDF, take the median of `raw.rgb`; for MSDFA, `raw.a` is the regular SDF
channel.

> **Deprecation note.** glslang's HLSL front-end is deprecated upstream and is
> expected to be removed in a future glslang major release. The converter is
> kept behind `lib/HlslToGlsl.hpp` so a future implementation can switch to DXC
> or Slang without changing the GUI. The GLSL cross-compilation half
> (SPIRV-Cross) is independent of that choice.

## Built-in examples

The **Shader test** tab has an example selector with MSDF coverage, MSDFA glow,
SDF glow, and SDF N64-style emboss examples. Each example has both a GLSL and an
HLSL version. **Load shader** and **Save shader** read/write the active editor
(GLSL when the HLSL checkbox is off, HLSL when it is on).

## Related files

- `lib/HlslToGlsl.{hpp,cpp}` - the converter API and implementation.
- `lib/ShaderExamples.{hpp,cpp}` - the built-in examples and in-app tutorial.
- `lib/sdf_opencl.cl`, `lib/shader*.glsl` - the SDF/MSDF generation shaders
  (these generate assets, not the preview).
