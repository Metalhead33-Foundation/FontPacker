#include <catch2/catch_test_macros.hpp>

#include "CQTOpenGLShaderSyntaxHighlighter.hpp"

#include <QTextBlock>
#include <QTextDocument>
#include <QTextLayout>
#include <QColor>

namespace {

int formatCount(const QTextDocument& document)
{
	int count = 0;
	for(QTextBlock block = document.begin(); block.isValid(); block = block.next()) {
		if(block.layout() != nullptr) count += block.layout()->formats().size();
	}
	return count;
}

int blockFormatCount(const QTextDocument& document, int blockNumber)
{
	const QTextBlock block = document.findBlockByNumber(blockNumber);
	return block.isValid() && block.layout() != nullptr ? block.layout()->formats().size() : 0;
}

bool hasCommentFormat(const QTextDocument& document, int blockNumber)
{
	const QTextBlock block = document.findBlockByNumber(blockNumber);
	if(!block.isValid() || block.layout() == nullptr) return false;
	const QColor commentColor(QStringLiteral("#6a737d"));
	for(const QTextLayout::FormatRange& range : block.layout()->formats()) {
		if(range.format.foreground().color() == commentColor) return true;
	}
	return false;
}

}

TEST_CASE("Shader highlighter handles GLSL and HLSL", "[highlighter]")
{
	QTextDocument glsl;
	glsl.setPlainText(QStringLiteral(
"#version 330 core\n"
"uniform sampler2D sdf_tex;\n"
"// line comment\n"
"vec4 x = texture(sdf_tex, uv);\n"
"/* block\n"
"still block */\n"
"float y;"));
	argos::CQTOpenGLShaderSyntaxHighlighter glslHighlighter(&glsl, argos::ShaderLanguage::GLSL);
	glslHighlighter.rehighlight();
	CHECK(formatCount(glsl) > 5);
	CHECK(blockFormatCount(glsl, 0) > 0);
	CHECK(blockFormatCount(glsl, 2) > 0);
	CHECK(blockFormatCount(glsl, 4) > 0);
	CHECK(blockFormatCount(glsl, 5) > 0);

	QTextDocument lineCommentEdgeCase;
	lineCommentEdgeCase.setPlainText(QStringLiteral("// /* this is not an opening block comment\nfloat x;"));
	argos::CQTOpenGLShaderSyntaxHighlighter edgeHighlighter(&lineCommentEdgeCase, argos::ShaderLanguage::GLSL);
	edgeHighlighter.rehighlight();
	CHECK(hasCommentFormat(lineCommentEdgeCase, 0));
	CHECK(!hasCommentFormat(lineCommentEdgeCase, 1));

	QTextDocument hlsl;
	hlsl.setPlainText(QStringLiteral(
"Texture2D<float4> sdf_tex : register(t0);\n"
"SamplerState sdf_sampler : register(s0);\n"
"float4 main(float2 TexCoords : TEXCOORD0) : SV_Target {\n"
"    return sdf_tex.Sample(sdf_sampler, TexCoords);\n"
"}"));
	argos::CQTOpenGLShaderSyntaxHighlighter hlslHighlighter(&hlsl, argos::ShaderLanguage::HLSL);
	hlslHighlighter.rehighlight();
	CHECK(formatCount(hlsl) > 5);
	CHECK(hlslHighlighter.language() == argos::ShaderLanguage::HLSL);

	hlslHighlighter.setLanguage(argos::ShaderLanguage::GLSL);
	CHECK(hlslHighlighter.language() == argos::ShaderLanguage::GLSL);
}
