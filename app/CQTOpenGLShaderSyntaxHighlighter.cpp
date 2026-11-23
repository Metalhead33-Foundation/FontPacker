/**
 * @file CQTOpenGLShaderSyntaxHighlighter.cpp
 * @brief GLSL/HLSL rule implementation for the shader editors.
 */

#include "CQTOpenGLShaderSyntaxHighlighter.hpp"

#include <QColor>
#include <QFont>
#include <QStringList>

namespace argos {

namespace {

/// @brief Block state values stored by QSyntaxHighlighter.
constexpr int NotInBlockComment = 0;
constexpr int InBlockComment = 1;

/// @brief Build a text format with an optional bold/italic style.
QTextCharFormat makeFormat(const QColor& color, bool bold = false, bool italic = false)
{
	QTextCharFormat format;
	format.setForeground(color);
	if(bold) format.setFontWeight(QFont::Bold);
	format.setFontItalic(italic);
	return format;
}

} // namespace

CQTOpenGLShaderSyntaxHighlighter::CQTOpenGLShaderSyntaxHighlighter(QTextDocument* document,
   ShaderLanguage language)
	: QSyntaxHighlighter(document), m_language(language)
{
	m_blockCommentFormat = makeFormat(QColor(QStringLiteral("#6a737d")), false, true);
	m_singleLineCommentFormat = makeFormat(QColor(QStringLiteral("#6a737d")), false, true);
	m_blockCommentStart = QRegularExpression(QStringLiteral("/\\*"));
	m_blockCommentEnd = QRegularExpression(QStringLiteral("\\*/"));
	m_singleLineComment = QRegularExpression(QStringLiteral("//[^\\n]*"));
	rebuildRules();
}

void CQTOpenGLShaderSyntaxHighlighter::setLanguage(ShaderLanguage language)
{
	if(m_language == language) return;
	m_language = language;
	rebuildRules();
	rehighlight();
}

void CQTOpenGLShaderSyntaxHighlighter::rebuildRules()
{
	m_rules.clear();

	const QTextCharFormat keywordFormat = makeFormat(QColor(QStringLiteral("#0033b3")), true);
	const QTextCharFormat typeFormat = makeFormat(QColor(QStringLiteral("#00627a")));
	const QTextCharFormat functionFormat = makeFormat(QColor(QStringLiteral("#795e26")));
	const QTextCharFormat numberFormat = makeFormat(QColor(QStringLiteral("#1750eb")));
	const QTextCharFormat preprocessorFormat = makeFormat(QColor(QStringLiteral("#9e880d")));
	const QTextCharFormat builtinFormat = makeFormat(QColor(QStringLiteral("#b35c00")));

	auto addPattern = [this](const QString& pattern, const QTextCharFormat& format) {
		m_rules.push_back({QRegularExpression(pattern), format});
	};
	auto addWords = [this](const QStringList& words, const QTextCharFormat& format) {
		QStringList escaped;
		escaped.reserve(words.size());
		for(const QString& word : words) escaped.append(QRegularExpression::escape(word));
		m_rules.push_back({QRegularExpression(QStringLiteral("\\b(?:%1)\\b").arg(escaped.join(QLatin1Char('|')))), format});
	};

	// Function calls are matched first so keywords and types can override them
	// when a word is both callable and reserved (for example `main`).
	addPattern(QStringLiteral("\\b[A-Za-z_]\\w*(?=\\s*\\()"), functionFormat);
	addPattern(QStringLiteral("\\b(?:0[xX][0-9A-Fa-f]+|(?:\\d+\\.\\d*|\\.\\d+|\\d+)(?:[eE][+-]?\\d+)?)[fFuUlL]*\\b"),
   numberFormat);

	if(m_language == ShaderLanguage::GLSL) {
		addPattern(QStringLiteral("\\b(?:void|bool|int|uint|float|double|[biud]?vec[234]|d?mat[234](?:x[234])?|[iu]?sampler(?:1D|2D|3D|Cube|2DArray|CubeArray|Buffer|2DMS|2DMSArray|1DArray|2DRect)|[iu]?image(?:1D|2D|3D|Cube|2DArray|CubeArray|Buffer|2DMS|2DMSArray|1DArray|2DRect)|atomic_uint)\\b"),
   typeFormat);
		addWords(QStringList{
QStringLiteral("attribute"), QStringLiteral("const"), QStringLiteral("uniform"),
QStringLiteral("varying"), QStringLiteral("buffer"), QStringLiteral("shared"),
QStringLiteral("coherent"), QStringLiteral("volatile"), QStringLiteral("restrict"),
QStringLiteral("readonly"), QStringLiteral("writeonly"), QStringLiteral("layout"),
QStringLiteral("centroid"), QStringLiteral("flat"), QStringLiteral("smooth"),
QStringLiteral("noperspective"), QStringLiteral("patch"), QStringLiteral("sample"),
QStringLiteral("invariant"), QStringLiteral("precise"), QStringLiteral("precision"),
QStringLiteral("highp"), QStringLiteral("mediump"), QStringLiteral("lowp"),
QStringLiteral("in"), QStringLiteral("out"), QStringLiteral("inout"),
QStringLiteral("break"), QStringLiteral("continue"), QStringLiteral("do"),
QStringLiteral("for"), QStringLiteral("while"), QStringLiteral("switch"),
QStringLiteral("case"), QStringLiteral("default"), QStringLiteral("if"),
QStringLiteral("else"), QStringLiteral("return"), QStringLiteral("discard"),
QStringLiteral("struct"), QStringLiteral("true"), QStringLiteral("false")
}, keywordFormat);
		addPattern(QStringLiteral("\\bgl_[A-Za-z0-9_]+\\b"), builtinFormat);
		addPattern(QStringLiteral("^[ \\t]*#[^\\n]*"), preprocessorFormat);
	} else {
		addPattern(QStringLiteral("\\b(?:void|bool|int|uint|dword|half|float|double|min16float|min10float|min16int|min12int|min16uint|(?:bool|int|uint|half|float|double)[1-4](?:x[1-4])?|vector|matrix)\\b"),
   typeFormat);
		addWords(QStringList{
QStringLiteral("AppendStructuredBuffer"), QStringLiteral("Buffer"),
QStringLiteral("ByteAddressBuffer"), QStringLiteral("ConsumeStructuredBuffer"),
QStringLiteral("RWBuffer"), QStringLiteral("RWByteAddressBuffer"),
QStringLiteral("RWStructuredBuffer"), QStringLiteral("StructuredBuffer"),
QStringLiteral("Texture1D"), QStringLiteral("Texture1DArray"),
QStringLiteral("Texture2D"), QStringLiteral("Texture2DArray"),
QStringLiteral("Texture2DMS"), QStringLiteral("Texture2DMSArray"),
QStringLiteral("Texture3D"), QStringLiteral("TextureCube"),
QStringLiteral("TextureCubeArray"), QStringLiteral("SamplerState"),
QStringLiteral("SamplerComparisonState"), QStringLiteral("BlendState"),
QStringLiteral("DepthStencilState"), QStringLiteral("RasterizerState"),
QStringLiteral("cbuffer"), QStringLiteral("tbuffer"), QStringLiteral("register"),
QStringLiteral("packoffset"), QStringLiteral("row_major"), QStringLiteral("column_major"),
QStringLiteral("nointerpolation"), QStringLiteral("linear"), QStringLiteral("centroid"),
QStringLiteral("noperspective"), QStringLiteral("sample"), QStringLiteral("in"),
QStringLiteral("out"), QStringLiteral("inout"), QStringLiteral("uniform"),
QStringLiteral("extern"), QStringLiteral("static"), QStringLiteral("const"),
QStringLiteral("volatile"), QStringLiteral("precise"), QStringLiteral("shared"),
QStringLiteral("groupshared"), QStringLiteral("struct"), QStringLiteral("class"),
QStringLiteral("interface"), QStringLiteral("namespace"), QStringLiteral("enum"),
QStringLiteral("typedef"), QStringLiteral("if"), QStringLiteral("else"),
QStringLiteral("for"), QStringLiteral("while"), QStringLiteral("do"),
QStringLiteral("switch"), QStringLiteral("case"), QStringLiteral("default"),
QStringLiteral("break"), QStringLiteral("continue"), QStringLiteral("return"),
QStringLiteral("discard"), QStringLiteral("true"), QStringLiteral("false"),
QStringLiteral("technique"), QStringLiteral("technique10"), QStringLiteral("technique11"),
QStringLiteral("pass"), QStringLiteral("compile"), QStringLiteral("compile_fragment"),
QStringLiteral("VertexShader"), QStringLiteral("PixelShader"),
QStringLiteral("GeometryShader"), QStringLiteral("HullShader"),
QStringLiteral("DomainShader"), QStringLiteral("ComputeShader")
}, keywordFormat);
		addPattern(QStringLiteral("\\b(?:SV_[A-Za-z0-9_]+|TEXCOORD\\d+|POSITION\\d*|COLOR\\d*|NORMAL\\d*|BINORMAL\\d*|TANGENT\\d*|DEPTH\\d*|BLENDINDICES\\d*|BLENDWEIGHT\\d*|PSIZE\\d*|FOG|VFACE|VPOS)\\b"),
   builtinFormat);
		addPattern(QStringLiteral("^[ \\t]*#[^\\n]*"), preprocessorFormat);
	}
}

void CQTOpenGLShaderSyntaxHighlighter::applyRules(const QString& text)
{
	for(const HighlightingRule& rule : m_rules) {
		QRegularExpressionMatchIterator matches = rule.pattern.globalMatch(text);
		while(matches.hasNext()) {
			const QRegularExpressionMatch match = matches.next();
			setFormat(match.capturedStart(), match.capturedLength(), rule.format);
		}
	}
}

void CQTOpenGLShaderSyntaxHighlighter::applyComments(const QString& text)
{
int index = 0;
bool inBlockComment = previousBlockState() == InBlockComment;
while(index < text.length()) {
if(inBlockComment) {
const QRegularExpressionMatch end = m_blockCommentEnd.match(text, index);
if(!end.hasMatch()) {
setFormat(index, text.length() - index, m_blockCommentFormat);
setCurrentBlockState(InBlockComment);
return;
}
setFormat(index, end.capturedEnd() - index, m_blockCommentFormat);
index = end.capturedEnd();
inBlockComment = false;
continue;
}

const QRegularExpressionMatch line = m_singleLineComment.match(text, index);
const QRegularExpressionMatch block = m_blockCommentStart.match(text, index);
const int lineStart = line.hasMatch() ? line.capturedStart() : -1;
const int blockStart = block.hasMatch() ? block.capturedStart() : -1;

// A line comment consumes the rest of the block, including any `/*`
// characters that appear after it. This prevents a comment such as
// `// /* not a block` from opening a multi-line comment.
if(lineStart >= 0 && (blockStart < 0 || lineStart < blockStart)) {
setFormat(lineStart, line.capturedLength(), m_singleLineCommentFormat);
return;
}
if(blockStart < 0) break;

const QRegularExpressionMatch end = m_blockCommentEnd.match(text, blockStart + 2);
if(!end.hasMatch()) {
setFormat(blockStart, text.length() - blockStart, m_blockCommentFormat);
setCurrentBlockState(InBlockComment);
return;
}
setFormat(blockStart, end.capturedEnd() - blockStart, m_blockCommentFormat);
index = end.capturedEnd();
}
setCurrentBlockState(NotInBlockComment);
}

void CQTOpenGLShaderSyntaxHighlighter::highlightBlock(const QString& text)
{
	applyRules(text);
	applyComments(text);
}

} // namespace argos
