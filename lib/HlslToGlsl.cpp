/**
 * @file HlslToGlsl.cpp
 * @brief Implementation of the HLSL-to-GLSL helper.
 */

#include "HlslToGlsl.hpp"

#include <glslang/Public/ResourceLimits.h>
#include <glslang/Public/ShaderLang.h>
#include <glslang/SPIRV/GlslangToSpv.h>
#include <spirv_cross/spirv_glsl.hpp>

#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

/** @brief Initialise glslang once for the lifetime of the process. */
void ensureGlslangInitialised()
{
	static std::once_flag once;
	std::call_once(once, [] { glslang::InitializeProcess(); });
}

/** @brief Build a readable glslang parse error. */
std::runtime_error parseError(glslang::TShader& shader)
{
	std::string message = "HLSL parsing failed:\n";
	message.append(shader.getInfoLog());
	const char* debugLog = shader.getInfoDebugLog();
	if(debugLog != nullptr && *debugLog != '\0') {
		message.append("\n");
		message.append(debugLog);
	}
	return std::runtime_error(message);
}

/** @brief Build a readable glslang link error. */
std::runtime_error linkError(glslang::TProgram& program)
{
	std::string message = "HLSL program linking failed:\n";
	message.append(program.getInfoLog());
	const char* debugLog = program.getInfoDebugLog();
	if(debugLog != nullptr && *debugLog != '\0') {
		message.append("\n");
		message.append(debugLog);
	}
	return std::runtime_error(message);
}

} // namespace

std::string hlslFragmentToGlsl(const std::string& hlsl, const HlslToGlslOptions& options)
{
	if(hlsl.empty()) throw std::invalid_argument("HLSL source is empty.");
	ensureGlslangInitialised();

	const TBuiltInResource* resources = ::GetDefaultResources();
	if(resources == nullptr) throw std::runtime_error("glslang has no default resource limits.");

	glslang::TShader shader(EShLangFragment);
	const char* source = hlsl.data();
	const int sourceLength = static_cast<int>(hlsl.size());
	shader.setStringsWithLengths(&source, &sourceLength, 1);
	shader.setEntryPoint(options.entryPoint.c_str());
	shader.setSourceEntryPoint(options.entryPoint.c_str());
	shader.setEnvInput(glslang::EShSourceHlsl, EShLangFragment, glslang::EShClientVulkan, 450);
	shader.setEnvClient(glslang::EShClientVulkan, glslang::EShTargetVulkan_1_0);
	shader.setEnvTarget(glslang::EShTargetSpv, glslang::EShTargetSpv_1_0);
	shader.setAutoMapBindings(true);
	shader.setAutoMapLocations(true);

	const EShMessages messages = static_cast<EShMessages>(EShMsgSpvRules | EShMsgVulkanRules);
	if(!shader.parse(resources, 450, false, messages)) {
		throw parseError(shader);
	}

	glslang::TProgram program;
	program.addShader(&shader);
	if(!program.link(messages)) {
		throw linkError(program);
	}

	glslang::TIntermediate* intermediate = program.getIntermediate(EShLangFragment);
	if(intermediate == nullptr) throw std::runtime_error("glslang produced no fragment intermediate.");

	std::vector<uint32_t> spirv;
	glslang::SpvOptions spvOptions;
	spvOptions.generateDebugInfo = false;
	spvOptions.disableOptimizer = false;
	spvOptions.optimizeSize = false;
	spvOptions.validate = true;
	glslang::GlslangToSpv(*intermediate, spirv, &spvOptions);
	if(spirv.empty()) throw std::runtime_error("glslang produced no SPIR-V.");

	spirv_cross::CompilerGLSL glsl(std::move(spirv));
	spirv_cross::CompilerGLSL::Options glslOptions;
	glslOptions.version = options.glslVersion;
	glslOptions.es = false;
	glslOptions.vulkan_semantics = false;
	glslOptions.fragment.default_float_precision = spirv_cross::CompilerGLSL::Options::Highp;
	glslOptions.fragment.default_int_precision = spirv_cross::CompilerGLSL::Options::Highp;
	glsl.set_common_options(glslOptions);

	// The preview uses a single combined texture/sampler uniform. SPIRV-Cross
	// creates the combined object; give it the image name (`sdf_tex`) so the
	// generated GLSL follows the same convention as hand-written examples.
	// HLSL `GetDimensions` lowers to texelFetch without a sampler, so create a
	// dummy sampler before combining the image/sampler pairs.
	glsl.build_dummy_sampler_for_combined_images();
	glsl.build_combined_image_samplers();
	for(const spirv_cross::CombinedImageSampler& remap : glsl.get_combined_image_samplers()) {
		glsl.set_name(remap.combined_id, glsl.get_name(remap.image_id));
	}

	return glsl.compile();
}
