/**
 * @file HlslToGlsl.hpp
 * @brief HLSL-to-GLSL transpilation through glslang and SPIRV-Cross.
 *
 * The GUI shader editor accepts HLSL fragments and needs GLSL for the
 * OpenGL 3.3 preview. This helper performs the same two-stage translation as
 * the graphics engine: glslang compiles HLSL to SPIR-V, then SPIRV-Cross
 * cross-compiles that SPIR-V to desktop GLSL.
 *
 * @warning glslang's HLSL front-end is deprecated upstream. It is used here
 *          as a pragmatic, currently available implementation; the API is
 *          intentionally small so a future DXC or Slang backend can replace
 *          the implementation without touching the UI.
 */

#ifndef HLSLTOGLSL_HPP
#define HLSLTOGLSL_HPP

#include <cstdint>
#include <string>

/**
 * @brief Options for the HLSL-to-GLSL translation.
 * @struct HlslToGlslOptions
 */
struct HlslToGlslOptions {
uint32_t glslVersion = 330; ///< GLSL version written by SPIRV-Cross (desktop GLSL by default).
std::string entryPoint = "main"; ///< HLSL entry point name.
};

/**
 * @brief Transpile one HLSL fragment shader to GLSL.
 *
 * The input must contain a fragment entry point (normally `main`) and should
 * use the preview conventions documented in the Shader test tab:
 * `Texture2D sdf_tex : register(t0)`, `SamplerState sdf_sampler : register(s0)`
 * and `float4 main(float2 TexCoords : TEXCOORD0) : SV_Target`.
 *
 * @param hlsl HLSL fragment source.
 * @param options Translation options.
 * @return Generated GLSL source.
 * @throws std::runtime_error if glslang cannot compile or SPIRV-Cross cannot
 *         cross-compile the source. The message contains the compiler log.
 */
std::string hlslFragmentToGlsl(const std::string& hlsl, const HlslToGlslOptions& options = {});

#endif // HLSLTOGLSL_HPP
