/**
 * @file CQTOpenGLShaderSyntaxHighlighter.hpp
 * @brief Qt syntax highlighter for the GLSL and HLSL editors.
 *
 * FontPacker's Shader test tab contains two editors: one for GLSL that is
 * compiled directly, and one for HLSL that is transpiled to GLSL. This class
 * keeps the two rule sets separate instead of pretending HLSL is Lua.
 */

#ifndef CQTOPENGLSHADERSYNTAXHIGHLIGHTER_HPP
#define CQTOPENGLSHADERSYNTAXHIGHLIGHTER_HPP

#include <QRegularExpression>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QVector>

class QTextDocument;

namespace argos {

/**
 * @brief Language selected for a shader editor.
 * @enum ShaderLanguage
 */
enum class ShaderLanguage {
GLSL, ///< GLSL 3.30 / GLSL ES syntax.
HLSL  ///< Direct3D HLSL shader syntax.
};

/**
 * @brief Regex-based GLSL/HLSL syntax highlighter.
 *
 * The highlighter colours keywords, types, function calls, numeric literals,
 * preprocessor directives, HLSL semantics, and both single-line and block
 * comments. Block-comment state is carried across text blocks, so a block
 * comment can span multiple lines correctly.
 */
class CQTOpenGLShaderSyntaxHighlighter : public QSyntaxHighlighter {
Q_OBJECT

public:
/**
 * @brief Attach a highlighter to a document.
 * @param document Document to highlight. The highlighter is owned by it.
 * @param language Initial shader language.
 */
explicit CQTOpenGLShaderSyntaxHighlighter(QTextDocument* document,
ShaderLanguage language = ShaderLanguage::GLSL);

/// @brief Destructor.
~CQTOpenGLShaderSyntaxHighlighter() override = default;

/**
 * @brief Switch the active rule set and re-highlight the document.
 * @param language New shader language.
 */
void setLanguage(ShaderLanguage language);

	/**
	 * @brief Return the active shader language.
	 * @return Current language.
	 */
	ShaderLanguage language() const noexcept { return m_language; }

protected:
/**
 * @brief Re-apply highlighting to one text block.
 * @param text Contents of the block.
 */
void highlightBlock(const QString& text) override;

private:
/**
 * @brief One single-line highlighting rule.
 * @struct HighlightingRule
 */
struct HighlightingRule {
QRegularExpression pattern; ///< Pattern to match inside a block.
QTextCharFormat format;     ///< Format applied to matched text.
};

/// @brief Rebuild the rule list after a language change.
void rebuildRules();

	/**
	 * @brief Apply all non-comment rules to one block.
	 * @param text Block contents.
	 */
	void applyRules(const QString& text);

	/**
	 * @brief Apply line and block comments, including cross-block state.
	 * @param text Block contents.
	 */
	void applyComments(const QString& text);

QVector<HighlightingRule> m_rules; ///< Keywords, types, functions, numbers, directives.
QRegularExpression m_blockCommentStart; ///< `/*` delimiter.
QRegularExpression m_blockCommentEnd;   ///< `*/` delimiter.
QRegularExpression m_singleLineComment; ///< `//` delimiter.
QTextCharFormat m_blockCommentFormat;   ///< Format for block comments.
QTextCharFormat m_singleLineCommentFormat; ///< Format for line comments.
ShaderLanguage m_language; ///< Active language.
};

} // namespace argos

#endif // CQTOPENGLSHADERSYNTAXHIGHLIGHTER_HPP
