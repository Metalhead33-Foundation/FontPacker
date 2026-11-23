/**
 * @file CQTOpenGLLuaSyntaxHighlighter.hpp
 * @brief Syntax highlighter used by the shader/GUI text panes.
 *
 * Adapted from Argos 3's Qt-OpenGL Lua syntax highlighter
 * (`qtopengl_lua_syntax_highlighter.h`, Carlo Pinciroli). FontPacker reuses it
 * to colour GLSL source in the GUI and keeps a Lua mode for the original use
 * case.
 *
 * The header intentionally stays close to the upstream implementation: Qt 6
 * builds it against `core5compat` because it uses `QRegExp`.
 */

#ifndef CQTOPENGLLUASYNTAXHIGHLIGHTER_HPP
#define CQTOPENGLLUASYNTAXHIGHLIGHTER_HPP

#include <QHash>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegExp>

class QTextDocument;

namespace argos {

/**
 * @brief Regex-based highlighter for GLSL (or Lua) text.
 * @class CQTOpenGLLuaSyntaxHighlighter
 */
class CQTOpenGLLuaSyntaxHighlighter : public QSyntaxHighlighter {

	Q_OBJECT

public:

	/**
	 * @brief Attach a highlighter to a document.
	 * @param pc_text Document to highlight.
	 * @param luaMode When true, use the Lua rule set; otherwise use GLSL.
	 */
	CQTOpenGLLuaSyntaxHighlighter(QTextDocument* pc_text, bool luaMode = false);

	/// @brief Destructor.
	virtual ~CQTOpenGLLuaSyntaxHighlighter() {}

protected:

	/**
	 * @brief Re-apply highlighting to one text block.
	 * @param str_text Contents of the block.
	 */
	void highlightBlock(const QString& str_text) override;

private:

	/**
	 * @brief One single-line highlighting rule.
	 * @struct SHighlightingRule
	 */
	struct SHighlightingRule
	{
		QRegExp Pattern;          ///< Pattern to match inside a block.
		QTextCharFormat Format;   ///< Format applied to the matched text.
	};

	QVector<SHighlightingRule> m_vecHighlightingRules; ///< All single-line rules (keywords, numbers, quotes, ...).

	QRegExp m_cCommentStartExpression;  ///< Start delimiter of the first multi-line comment style.
	QRegExp m_cCommentEndExpression;    ///< End delimiter of the first multi-line comment style.
	QRegExp m_cCommentStartExpression2; ///< Start delimiter of the second multi-line comment style.
	QRegExp m_cCommentEndExpression2;   ///< End delimiter of the second multi-line comment style.

	QTextCharFormat m_cKeywordFormat;            ///< Format for keywords.
	QTextCharFormat m_cSingleLineCommentFormat;  ///< Format for `//` comments.
	QTextCharFormat m_cMultiLineCommentFormat;   ///< Format for multi-line comments.
	QTextCharFormat m_cQuotationFormat;          ///< Format for quoted strings.
	QTextCharFormat m_cFunctionFormat;           ///< Format for function names.

	bool luaMode; ///< True when the Lua rule set is active, false for GLSL.
};

}

#endif // CQTOPENGLLUASYNTAXHIGHLIGHTER_HPP
