/**
 * @file ShaderExamples.hpp
 * @brief Built-in GLSL/HLSL examples and tutorial text for the Shader test tab.
 */

#ifndef SHADEREXAMPLES_HPP
#define SHADEREXAMPLES_HPP

#include <vector>

/**
 * @brief One selectable preview-shader example.
 * @struct ShaderExample
 */
struct ShaderExample {
	const char* name; ///< Display name.
	const char* glsl; ///< Ready-to-compile GLSL fragment source.
	const char* hlsl; ///< Equivalent HLSL fragment source for the transpiler.
};

/**
 * @brief Return the built-in GLSL/HLSL examples.
 * @return Immutable example list; the strings have static lifetime.
 */
const std::vector<ShaderExample>& shaderExamples();

/**
 * @brief Return the short in-application shader tutorial text.
 * @return Static tutorial string.
 */
const char* shaderTutorialText();

#endif // SHADEREXAMPLES_HPP
