#include "ShaderExamples.hpp"

const std::vector<ShaderExample>& shaderExamples()
{
	static const std::vector<ShaderExample> examples = {
		{"MSDF: median coverage", R"GLSL(#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D sdf_tex;
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}
void main() {
    vec4 raw = texture(sdf_tex, TexCoords);
    float d = median(raw.r, raw.g, raw.b);
    FragColor = vec4(1.0, 1.0, 1.0, smoothstep(0.48, 0.52, d));
}
)GLSL", R"HLSL(Texture2D<float4> sdf_tex : register(t0);
SamplerState sdf_sampler : register(s0);
float median3(float3 v) { return max(min(v.x, v.y), min(max(v.x, v.y), v.z)); }
float4 main(float2 TexCoords : TEXCOORD0) : SV_Target {
    float4 raw = sdf_tex.Sample(sdf_sampler, TexCoords);
    float d = median3(raw.rgb);
    return float4(1.0, 1.0, 1.0, smoothstep(0.48, 0.52, d));
}
)HLSL"},
		{"MSDFA: cyan glow", R"GLSL(#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D sdf_tex;
float median(float r, float g, float b) {
    return max(min(r, g), min(max(r, g), b));
}
void main() {
    vec4 raw = texture(sdf_tex, TexCoords);
    if (raw.a < 0.003921568627451) discard;
    float shape = smoothstep(0.47, 0.50, median(raw.r, raw.g, raw.b));
    float glow = smoothstep(0.10, 0.50, raw.a);
    vec3 glowColor = vec3(0.0, 1.0, 1.0);
    vec3 textColor = vec3(0.8, 1.0, 1.0);
    FragColor = vec4(mix(glowColor * glow, textColor, shape), max(shape, glow));
}
)GLSL", R"HLSL(Texture2D<float4> sdf_tex : register(t0);
SamplerState sdf_sampler : register(s0);
float median3(float3 v) { return max(min(v.x, v.y), min(max(v.x, v.y), v.z)); }
float4 main(float2 TexCoords : TEXCOORD0) : SV_Target {
    float4 raw = sdf_tex.Sample(sdf_sampler, TexCoords);
    if (raw.a < 0.003921568627451) discard;
    float shape = smoothstep(0.47, 0.50, median3(raw.rgb));
    float glow = smoothstep(0.10, 0.50, raw.a);
    float3 glowColor = float3(0.0, 1.0, 1.0);
    float3 textColor = float3(0.8, 1.0, 1.0);
    return float4(lerp(glowColor * glow, textColor, shape), max(shape, glow));
}
)HLSL"},
		{"SDF: yellow glow", R"GLSL(#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D sdf_tex;
void main() {
    float d = texture(sdf_tex, TexCoords).r;
    float alpha = smoothstep(0.45, 0.46, d);
    float glow = smoothstep(0.30, 0.45, d);
    vec3 glyphColor = vec3(0.2, 0.6, 1.0);
    vec3 glowColor = vec3(1.0, 1.0, 0.0);
    FragColor = vec4(mix(glowColor, glyphColor, alpha), max(alpha, glow));
}
)GLSL", R"HLSL(Texture2D<float4> sdf_tex : register(t0);
SamplerState sdf_sampler : register(s0);
float4 main(float2 TexCoords : TEXCOORD0) : SV_Target {
    float d = sdf_tex.Sample(sdf_sampler, TexCoords).r;
    float alpha = smoothstep(0.45, 0.46, d);
    float glow = smoothstep(0.30, 0.45, d);
    float3 glyphColor = float3(0.2, 0.6, 1.0);
    float3 glowColor = float3(1.0, 1.0, 0.0);
    return float4(lerp(glowColor, glyphColor, alpha), max(alpha, glow));
}
)HLSL"},
		{"SDF: N64 emboss", R"GLSL(#version 330 core
out vec4 FragColor;
in vec2 TexCoords;
uniform sampler2D sdf_tex;
vec4 n64Filter(vec2 uv) {
    vec2 size = vec2(textureSize(sdf_tex, 0));
    vec2 texel = 1.0 / size;
    vec2 pixelPos = uv * size - 0.5;
    vec2 basePixel = floor(pixelPos);
    vec2 f = pixelPos - basePixel;
    vec2 baseUV = (basePixel + 0.5) / size;
    if (f.x + f.y < 1.0) {
        vec4 a = texture(sdf_tex, baseUV);
        vec4 b = texture(sdf_tex, baseUV + vec2(texel.x, 0.0));
        vec4 c = texture(sdf_tex, baseUV + vec2(0.0, texel.y));
        return a * (1.0 - f.x - f.y) + b * f.x + c * f.y;
    }
    vec4 a = texture(sdf_tex, baseUV + texel);
    vec4 b = texture(sdf_tex, baseUV + vec2(texel.x, 0.0));
    vec4 c = texture(sdf_tex, baseUV + vec2(0.0, texel.y));
    return a * (f.x + f.y - 1.0) + b * (1.0 - f.y) + c * (1.0 - f.x);
}
void main() {
    float d = n64Filter(TexCoords).r;
    vec2 texel = 1.0 / vec2(textureSize(sdf_tex, 0));
    float right = n64Filter(TexCoords + vec2(texel.x, 0.0)).r;
    float bottom = n64Filter(TexCoords + vec2(0.0, texel.y)).r;
    vec3 normal = normalize(vec3(right - d, bottom - d, 1.0));
    float emboss = dot(normal, normalize(vec3(-1.0, -1.0, 1.0)));
    FragColor = vec4(vec3(emboss * 0.5 + 0.5), smoothstep(0.49, 0.51, d));
}
)GLSL", R"HLSL(Texture2D<float4> sdf_tex : register(t0);
SamplerState sdf_sampler : register(s0);
float4 n64Filter(float2 uv) {
    uint w, h;
    sdf_tex.GetDimensions(w, h);
    float2 size = float2(w, h);
    float2 texel = 1.0 / size;
    float2 pixelPos = uv * size - 0.5;
    float2 basePixel = floor(pixelPos);
    float2 f = pixelPos - basePixel;
    float2 baseUV = (basePixel + 0.5) / size;
    if (f.x + f.y < 1.0) {
        float4 a = sdf_tex.Sample(sdf_sampler, baseUV);
        float4 b = sdf_tex.Sample(sdf_sampler, baseUV + float2(texel.x, 0.0));
        float4 c = sdf_tex.Sample(sdf_sampler, baseUV + float2(0.0, texel.y));
        return a * (1.0 - f.x - f.y) + b * f.x + c * f.y;
    }
    float4 a = sdf_tex.Sample(sdf_sampler, baseUV + texel);
    float4 b = sdf_tex.Sample(sdf_sampler, baseUV + float2(texel.x, 0.0));
    float4 c = sdf_tex.Sample(sdf_sampler, baseUV + float2(0.0, texel.y));
    return a * (f.x + f.y - 1.0) + b * (1.0 - f.y) + c * (1.0 - f.x);
}
float4 main(float2 TexCoords : TEXCOORD0) : SV_Target {
    float d = n64Filter(TexCoords).r;
    uint w, h;
    sdf_tex.GetDimensions(w, h);
    float2 texel = 1.0 / float2(w, h);
    float right = n64Filter(TexCoords + float2(texel.x, 0.0)).r;
    float bottom = n64Filter(TexCoords + float2(0.0, texel.y)).r;
    float3 normal = normalize(float3(right - d, bottom - d, 1.0));
    float emboss = dot(normal, normalize(float3(-1.0, -1.0, 1.0)));
    return float4(emboss * 0.5 + 0.5, emboss * 0.5 + 0.5, emboss * 0.5 + 0.5, smoothstep(0.49, 0.51, d));
}
)HLSL"}
	};
	return examples;
}

const char* shaderTutorialText()
{
	return R"TUTORIAL(FontPacker shader preview tutorial
===============================

The preview binds the selected SDF glyph to `uniform sampler2D sdf_tex` and
passes the quad UV through `in vec2 TexCoords`. The default output is
`out vec4 FragColor`.

SDF:
  Sample the red channel. Values around 0.5 are the contour; use smoothstep
  around the midpoint for antialiasing:
      float d = texture(sdf_tex, TexCoords).r;
      float a = smoothstep(0.48, 0.52, d);

MSDF:
  Use the median of RGB for the contour. Do not average the channels:
      float d = median(raw.r, raw.g, raw.b);
      float a = smoothstep(0.48, 0.52, d);

MSDFA:
  RGB is the MSDF; alpha is a regular SDF. Use alpha for glow/bevel effects
  and discard pixels whose alpha is essentially zero.

HLSL:
  Tick the HLSL checkbox, edit the HLSL editor, then press Transpile. The
  built-in translator uses glslang (HLSL -> SPIR-V) and SPIRV-Cross
  (SPIR-V -> GLSL). Keep the preview conventions:
      Texture2D<float4> sdf_tex : register(t0);
      SamplerState sdf_sampler : register(s0);
      float4 main(float2 TexCoords : TEXCOORD0) : SV_Target
  The output is placed in the GLSL editor and can be compiled immediately.

Note: glslang's HLSL front-end is deprecated upstream. The translator is kept
behind a small C++ API so it can be replaced by DXC or Slang later.)TUTORIAL";
}
