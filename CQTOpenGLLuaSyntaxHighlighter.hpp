#ifndef CQTOPENGLLUASYNTAXHIGHLIGHTER_HPP
#define CQTOPENGLLUASYNTAXHIGHLIGHTER_HPP
/**
 * @file <argos3/plugins/simulator/visualizations/qt-opengl/qtopengl_lua_syntax_highlighter.h>
 *
 * @author Carlo Pinciroli <ilpincy@gmail.com>
 */

#include <QHash>
#include <QSyntaxHighlighter>
#include <QTextCharFormat>
#include <QRegExp>

class QTextDocument;

namespace argos {

class CQTOpenGLLuaSyntaxHighlighter : public QSyntaxHighlighter {

	Q_OBJECT

public:

	CQTOpenGLLuaSyntaxHighlighter(QTextDocument* pc_text, bool luaMode = false);
	virtual ~CQTOpenGLLuaSyntaxHighlighter() {}

protected:

	void highlightBlock(const QString& str_text);

private:

	struct SHighlightingRule
	{
		QRegExp Pattern;
		QTextCharFormat Format;
	};
	QVector<SHighlightingRule> m_vecHighlightingRules;

	QRegExp m_cCommentStartExpression;
	QRegExp m_cCommentEndExpression;
	QRegExp m_cCommentStartExpression2;
	QRegExp m_cCommentEndExpression2;

	QTextCharFormat m_cKeywordFormat;
	QTextCharFormat m_cSingleLineCommentFormat;
	QTextCharFormat m_cMultiLineCommentFormat;
	QTextCharFormat m_cQuotationFormat;
	QTextCharFormat m_cFunctionFormat;

	bool luaMode;
};

}


#endif // CQTOPENGLLUASYNTAXHIGHLIGHTER_HPP
