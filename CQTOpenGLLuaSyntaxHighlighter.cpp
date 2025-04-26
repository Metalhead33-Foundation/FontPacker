#include "CQTOpenGLLuaSyntaxHighlighter.hpp"

namespace argos {

	/****************************************/
	/****************************************/

	static int NOT_MULTILINE_COMMENT = 0;
	static int MULTILINE_COMMENT = 1;

	/****************************************/
	/****************************************/

	CQTOpenGLLuaSyntaxHighlighter::CQTOpenGLLuaSyntaxHighlighter(QTextDocument* pc_text, bool luaMode) :
		QSyntaxHighlighter(pc_text), luaMode(luaMode) {
		SHighlightingRule sRule;
		m_cKeywordFormat.setForeground(Qt::darkBlue);
		m_cKeywordFormat.setFontWeight(QFont::Bold);
		QStringList cKeywordPatterns;
		cKeywordPatterns << "\\band\\b"    << "\\bbreak\\b"  << "\\bdo\\b"   << "\\belse\\b"     << "\\belseif\\b"
						 << "\\bend\\b"    << "\\bfalse\\b"  << "\\bfor\\b"  << "\\bfunction\\b" << "\\bif\\b"
						 << "\\bin\\b"     << "\\blocal\\b"  << "\\bnil\\b"  << "\\bnot\\b"      << "\\bor\\b"
						 << "\\brepeat\\b" << "\\breturn\\b" << "\\bthen\\b" << "\\btrue\\b"     << "\\buntil\\b" << "\\bwhile\\b"
						// Now for some GLSL and HLSL specific stuff
						 << "\\bactive\\b"
						 <<"\\bAppendStructuredBuffer\\b"
						 <<"\\basm\\b"
						 <<"\\basm_fragment\\b"
						 <<"\\battribute\\b"
						 <<"\\bBlendState\\b"
						 <<"\\bbool\\b"
						 <<"\\bbreak\\b"
						 <<"\\bBuffer\\b"
						 <<"\\bbvec2\\b"
						 <<"\\bbvec3\\b"
						 <<"\\bbvec4\\b"
						 <<"\\bByteAddressBuffer\\b"
						 <<"\\bcase\\b"
						 <<"\\bcast\\b"
						 <<"\\bcbuffer\\b"
						 <<"\\bcentroid\\b"
						 <<"\\bclass\\b"
						 <<"\\bcolumn_major\\b"
						 <<"\\bcommon\\b"
						 <<"\\bcompile\\b"
						 <<"\\bcompile_fragment\\b"
						 <<"\\bCompileShader\\b"
						 <<"\\bComputeShader\\b"
						 <<"\\bconst\\b"
						 <<"\\bConsumeStructuredBuffer\\b"
						 <<"\\bcontinue\\b"
						 <<"\\bdefault\\b"
						 <<"\\bDepthStencilState\\b"
						 <<"\\bDepthStencilView\\b"
						 <<"\\bdiscard\\b"
						 <<"\\bdmat2\\b"
						 <<"\\bdmat2x2\\b"
						 <<"\\bdmat2x3\\b"
						 <<"\\bdmat2x4\\b"
						 <<"\\bdmat3\\b"
						 <<"\\bdmat3x2\\b"
						 <<"\\bdmat3x3\\b"
						 <<"\\bdmat3x4\\b"
						 <<"\\bdmat4\\b"
						 <<"\\bdmat4x2\\b"
						 <<"\\bdmat4x3\\b"
						 <<"\\bdmat4x4\\b"
						 <<"\\bdo\\b"
						 <<"\\bDomainShader\\b"
						 <<"\\bdouble\\b"
						 <<"\\bdvec2\\b"
						 <<"\\bdvec3\\b"
						 <<"\\bdvec4\\b"
						 <<"\\bdword\\b"
						 <<"\\belse\\b"
						 <<"\\benum\\b"
						 <<"\\bexport\\b"
						 <<"\\bextern\\b"
						 <<"\\bexternal\\b"
						 <<"\\bfalse\\b"
						 <<"\\bfilter\\b"
						 <<"\\bfixed\\b"
						 <<"\\bflat\\b"
						 <<"\\bfloat\\b"
						 <<"\\bfloat1\\b"
						 <<"\\bfloat1x1\\b"
						 <<"\\bfloat1x2\\b"
						 <<"\\bfloat1x3\\b"
						 <<"\\bfloat1x4\\b"
						 <<"\\bfloat2\\b"
						 <<"\\bfloat2x1\\b"
						 <<"\\bfloat2x2\\b"
						 <<"\\bfloat2x3\\b"
						 <<"\\bfloat2x4\\b"
						 <<"\\bfloat3\\b"
						 <<"\\bfloat3x1\\b"
						 <<"\\bfloat3x2\\b"
						 <<"\\bfloat3x3\\b"
						 <<"\\bfloat3x4\\b"
						 <<"\\bfloat4\\b"
						 <<"\\bfloat4x1\\b"
						 <<"\\bfloat4x2\\b"
						 <<"\\bfloat4x3\\b"
						 <<"\\bfloat4x4\\b"
						 <<"\\bfor\\b"
						 <<"\\bfvec2\\b"
						 <<"\\bfvec3\\b"
						 <<"\\bfvec4\\b"
						 <<"\\bfxgroup\\b"
						 <<"\\bGeometryShader\\b"
						 <<"\\bgoto\\b"
						 <<"\\bgroupshared\\b"
						 <<"\\bhalf\\b"
						 <<"\\bhighp\\b"
						 <<"\\bHullshader\\b"
						 <<"\\bhvec2\\b"
						 <<"\\bhvec3\\b"
						 <<"\\bhvec4\\b"
						 <<"\\bif\\b"
						 <<"\\biimage1D\\b"
						 <<"\\biimage1DArray\\b"
						 <<"\\biimage2D\\b"
						 <<"\\biimage2DArray\\b"
						 <<"\\biimage3D\\b"
						 <<"\\biimageBuffer\\b"
						 <<"\\biimageCube\\b"
						 <<"\\bimage1D\\b"
						 <<"\\bimage1DArray\\b"
						 <<"\\bimage1DArrayShadow\\b"
						 <<"\\bimage1DShadow\\b"
						 <<"\\bimage2D\\b"
						 <<"\\bimage2DArray\\b"
						 <<"\\bimage2DArrayShadow\\b"
						 <<"\\bimage2DShadow\\b"
						 <<"\\bimage3D\\b"
						 <<"\\bimageBuffer\\b"
						 <<"\\bimageCube\\b"
						 <<"\\bin\\b"
						 <<"\\binline\\b"
						 <<"\\binout\\b"
						 <<"\\binput\\b"
						 <<"\\bInputPatch\\b"
						 <<"\\bint\\b"
						 <<"\\binterface\\b"
						 <<"\\binvariant\\b"
						 <<"\\bisampler1D\\b"
						 <<"\\bisampler1DArray\\b"
						 <<"\\bisampler2D\\b"
						 <<"\\bisampler2DArray\\b"
						 <<"\\bisampler2DMS\\b"
						 <<"\\bisampler2DMSArray\\b"
						 <<"\\bisampler2DRect\\b"
						 <<"\\bisampler3D\\b"
						 <<"\\bisamplerBuffer\\b"
						 <<"\\bisamplerCube\\b"
						 <<"\\bisamplerCubeArray\\b"
						 <<"\\bivec2\\b"
						 <<"\\bivec3\\b"
						 <<"\\bivec4\\b"
						 <<"\\blayout\\b"
						 <<"\\bline\\b"
						 <<"\\blineadj\\b"
						 <<"\\blinear\\b"
						 <<"\\bLineStream\\b"
						 <<"\\blong\\b"
						 <<"\\blowp\\b"
						 <<"\\bmat2\\b"
						 <<"\\bmat2x2\\b"
						 <<"\\bmat2x3\\b"
						 <<"\\bmat2x4\\b"
						 <<"\\bmat3\\b"
						 <<"\\bmat3x2\\b"
						 <<"\\bmat3x3\\b"
						 <<"\\bmat3x4\\b"
						 <<"\\bmat4\\b"
						 <<"\\bmat4x2\\b"
						 <<"\\bmat4x3\\b"
						 <<"\\bmat4x4\\b"
						 <<"\\bmatrix\\b"
						 <<"\\bmediump\\b"
						 <<"\\bmin10float\\b"
						 <<"\\bmin12int\\b"
						 <<"\\bmin16float\\b"
						 <<"\\bmin16int\\b"
						 <<"\\bmin16uint\\b"
						 <<"\\bnamespace\\b"
						 <<"\\bnoinline\\b"
						 <<"\\bnointerpolation\\b"
						 <<"\\bnoperspective\\b"
						 <<"\\bNULL\\b"
						 <<"\\bout\\b"
						 <<"\\boutput\\b"
						 <<"\\bOutputPatch\\b"
						 <<"\\bpacked\\b"
						 <<"\\bpackoffset\\b"
						 <<"\\bpartition\\b"
						 <<"\\bpass\\b"
						 <<"\\bpatch\\b"
						 <<"\\bpixelfragment\\b"
						 <<"\\bPixelShader\\b"
						 <<"\\bpoint\\b"
						 <<"\\bPointStream\\b"
						 <<"\\bprecise\\b"
						 <<"\\bprecision\\b"
						 <<"\\bpublic\\b"
						 <<"\\bRasterizerState\\b"
						 <<"\\bregister\\b"
						 <<"\\bRenderTargetView\\b"
						 <<"\\breturn\\b"
						 <<"\\brow_major\\b"
						 <<"\\bRWBuffer\\b"
						 <<"\\bRWByteAddressBuffer\\b"
						 <<"\\bRWStructuredBuffer\\b"
						 <<"\\bRWTexture1D\\b"
						 <<"\\bRWTexture1DArray\\b"
						 <<"\\bRWTexture2D\\b"
						 <<"\\bRWTexture2DArray\\b"
						 <<"\\bRWTexture3D\\b"
						 <<"\\bsample\\b"
						 <<"\\bsampler\\b"
						 <<"\\bsampler1D\\b"
						 <<"\\bsampler1DArray\\b"
						 <<"\\bsampler1DArrayShadow\\b"
						 <<"\\bsampler1DShadow\\b"
						 <<"\\bsampler2D\\b"
						 <<"\\bsampler2DArray\\b"
						 <<"\\bsampler2DArrayShadow\\b"
						 <<"\\bsampler2DMS\\b"
						 <<"\\bsampler2DMSArray\\b"
						 <<"\\bsampler2DRect\\b"
						 <<"\\bsampler2DRectShadow\\b"
						 <<"\\bsampler2DShadow\\b"
						 <<"\\bsampler3D\\b"
						 <<"\\bsampler3DRect\\b"
						 <<"\\bsamplerBuffer\\b"
						 <<"\\bSamplerComparisonState\\b"
						 <<"\\bsamplerCube\\b"
						 <<"\\bsamplerCubeArray\\b"
						 <<"\\bsamplerCubeArrayShadow\\b"
						 <<"\\bsamplerCubeShadow\\b"
						 <<"\\bSamplerState\\b"
						 <<"\\bshared\\b"
						 <<"\\bshort\\b"
						 <<"\\bsizeof\\b"
						 <<"\\bsmooth\\b"
						 <<"\\bsnorm\\b"
						 <<"\\bstateblock\\b"
						 <<"\\bstateblock_state\\b"
						 <<"\\bstatic\\b"
						 <<"\\bstring\\b"
						 <<"\\bstruct\\b"
						 <<"\\bStructuredBuffer\\b"
						 <<"\\bsubroutine\\b"
						 <<"\\bsuperp\\b"
						 <<"\\bswitch\\b"
						 <<"\\btbuffer\\b"
						 <<"\\btechnique\\b"
						 <<"\\btechnique10\\b"
						 <<"\\btechnique11\\b"
						 <<"\\btemplate\\b"
						 <<"\\btexture\\b"
						 <<"\\bTexture1D\\b"
						 <<"\\bTexture1DArray\\b"
						 <<"\\bTexture2D\\b"
						 <<"\\bTexture2DArray\\b"
						 <<"\\bTexture2DMS\\b"
						 <<"\\bTexture2DMSArray\\b"
						 <<"\\bTexture3D\\b"
						 <<"\\bTextureCube\\b"
						 <<"\\bTextureCubeArray\\b"
						 <<"\\bthis\\b"
						 <<"\\btriangle\\b"
						 <<"\\btriangleadj\\b"
						 <<"\\bTriangleStream\\b"
						 <<"\\btrue\\b"
						 <<"\\btypedef\\b"
						 <<"\\buimage1D\\b"
						 <<"\\buimage1DArray\\b"
						 <<"\\buimage2D\\b"
						 <<"\\buimage2DArray\\b"
						 <<"\\buimage3D\\b"
						 <<"\\buimageBuffer\\b"
						 <<"\\buimageCube\\b"
						 <<"\\buint\\b"
						 <<"\\buniform\\b"
						 <<"\\bunion\\b"
						 <<"\\bunorm\\b"
						 <<"\\bunsigned\\b"
						 <<"\\busampler1D\\b"
						 <<"\\busampler1DArray\\b"
						 <<"\\busampler2D\\b"
						 <<"\\busampler2DArray\\b"
						 <<"\\busampler2DMS\\b"
						 <<"\\busampler2DMSArray\\b"
						 <<"\\busampler2DRect\\b"
						 <<"\\busampler3D\\b"
						 <<"\\busamplerBuffer\\b"
						 <<"\\busamplerCube\\b"
						 <<"\\busamplerCubeArray\\b"
						 <<"\\busing\\b"
						 <<"\\buvec2\\b"
						 <<"\\buvec3\\b"
						 <<"\\buvec4\\b"
						 <<"\\bvarying\\b"
						 <<"\\bvec2\\b"
						 <<"\\bvec3\\b"
						 <<"\\bvec4\\b"
						 <<"\\bvector\\b"
						 <<"\\bvertexfragment\\b"
						 <<"\\bVertexShader\\b"
						 <<"\\bvoid\\b"
						 <<"\\bvolatile\\b"
						 <<"\\bwhile\\b";
		foreach (const QString& cPattern, cKeywordPatterns) {
			sRule.Pattern = QRegExp(cPattern);
			sRule.Format = m_cKeywordFormat;
			m_vecHighlightingRules.append(sRule);
		}

		m_cSingleLineCommentFormat.setForeground(Qt::darkGray);
		m_cSingleLineCommentFormat.setFontItalic(true);
		if(luaMode) {
			sRule.Pattern = QRegExp("--[^[\n]*");
			sRule.Format = m_cSingleLineCommentFormat;
			m_vecHighlightingRules.append(sRule);
		} else {
			sRule.Pattern = QRegExp("\\/\\/[^[\n]*");
			sRule.Format = m_cSingleLineCommentFormat;
			m_vecHighlightingRules.append(sRule);
		}

		m_cMultiLineCommentFormat.setForeground(Qt::darkGray);
		m_cMultiLineCommentFormat.setFontItalic(true);
		m_cCommentStartExpression = QRegExp("--\\[\\[");
		m_cCommentEndExpression = QRegExp("\\]\\]");
		m_cCommentStartExpression2 = QRegExp("\\/\\*");
		m_cCommentEndExpression2 = QRegExp("\\*\\/");
		m_cQuotationFormat.setForeground(Qt::darkGreen);
		sRule.Pattern = QRegExp("\".*\"");
		sRule.Format = m_cQuotationFormat;
		m_vecHighlightingRules.append(sRule);
	}

	/****************************************/
	/****************************************/

	void CQTOpenGLLuaSyntaxHighlighter::highlightBlock(const QString& str_text) {
		/*
	   * Apply normal rules
	   */
		foreach (const SHighlightingRule& sRule, m_vecHighlightingRules) {
			QRegExp cExpression(sRule.Pattern);
			int i = cExpression.indexIn(str_text);
			while(i >= 0) {
				int nLength = cExpression.matchedLength();
				setFormat(i, nLength, sRule.Format);
				i = cExpression.indexIn(str_text, i + nLength);
			}
		}
		/*
	   * Apply multi-line comment rules
	   */
		if(luaMode) {
			setCurrentBlockState(NOT_MULTILINE_COMMENT);
			int nStartIndex = 0;
			if (previousBlockState() != MULTILINE_COMMENT) {
				nStartIndex = m_cCommentStartExpression.indexIn(str_text);
			}
			while(nStartIndex >= 0) {
				int nEndIndex = m_cCommentEndExpression.indexIn(str_text, nStartIndex);
				int nCommentLength;
				if (nEndIndex == -1) {
					setCurrentBlockState(MULTILINE_COMMENT);
					nCommentLength = str_text.length() - nStartIndex;
				} else {
					nCommentLength = nEndIndex - nStartIndex
									 + m_cCommentEndExpression.matchedLength();
				}
				setFormat(nStartIndex, nCommentLength, m_cMultiLineCommentFormat);
				nStartIndex = m_cCommentStartExpression.indexIn(str_text, nStartIndex + nCommentLength);
			}
		} else {
			setCurrentBlockState(NOT_MULTILINE_COMMENT);
			int nStartIndex = 0;
			if (previousBlockState() != MULTILINE_COMMENT) {
				nStartIndex = m_cCommentStartExpression2.indexIn(str_text);
			}
			while(nStartIndex >= 0) {
				int nEndIndex = m_cCommentEndExpression2.indexIn(str_text, nStartIndex);
				int nCommentLength;
				if (nEndIndex == -1) {
					setCurrentBlockState(MULTILINE_COMMENT);
					nCommentLength = str_text.length() - nStartIndex;
				} else {
					nCommentLength = nEndIndex - nStartIndex
									 + m_cCommentEndExpression2.matchedLength();
				}
				setFormat(nStartIndex, nCommentLength, m_cMultiLineCommentFormat);
				nStartIndex = m_cCommentStartExpression2.indexIn(str_text, nStartIndex + nCommentLength);
			}
		}
	}

	/****************************************/
	/****************************************/

}
