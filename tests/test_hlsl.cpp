#include <catch2/catch_test_macros.hpp>

#include "HlslToGlsl.hpp"
#include "ShaderExamples.hpp"

#include <string>

TEST_CASE("HLSL fragment shaders transpile to GLSL", "[hlsl]")
{
	const std::string hlsl = R"HLSL(
Texture2D<float4> sdf_tex : register(t0);
SamplerState sdf_sampler : register(s0);

float4 main(float2 TexCoords : TEXCOORD0) : SV_Target
{
    float d = sdf_tex.Sample(sdf_sampler, TexCoords).r;
    return float4(1.0, 1.0, 1.0, step(0.5, d));
}
)HLSL";

	const std::string glsl = hlslFragmentToGlsl(hlsl);
	CHECK(glsl.find("#version 330") != std::string::npos);
	CHECK(glsl.find("sdf_tex") != std::string::npos);
	CHECK(glsl.find("TexCoords") != std::string::npos);
}

TEST_CASE("Every built-in HLSL example transpiles", "[hlsl]")
{
	for(const ShaderExample& example : shaderExamples()) {
		INFO(example.name);
		CHECK_NOTHROW(hlslFragmentToGlsl(example.hlsl));
	}
}
