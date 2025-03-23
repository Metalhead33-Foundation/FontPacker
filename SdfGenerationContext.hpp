#ifndef SDFGENERATIONCONTEXT_HPP
#define SDFGENERATIONCONTEXT_HPP
#include <QImage>
#include <glm/glm.hpp>
#include "SDFGenerationArguments.hpp"
#include "PreprocessedFontFace.hpp"
#include "StoredCharacter.hpp"
#include "FontOutlineDecompositionContext.hpp"
#include <harfbuzz/hb-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftoutln.h>

struct FT_Bitmap_;
class SdfGenerationContext
{
protected:
	FT_Library library;
	FontOutlineDecompositionContext decompositionContext;
public:
	SdfGenerationContext();
	virtual ~SdfGenerationContext();
	virtual QImage produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args) = 0;
	virtual QImage produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args) = 0;
	void processOutlineGlyph(StoredCharacter& output, FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args);
	void processBitmapGlyph(StoredCharacter& output, FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args);
	void processFont(PreprocessedFontFace& output, const SDFGenerationArguments& args);
	static QImage FTBitmap2QImage(const FT_Bitmap_& bitmap, unsigned int intended_width, unsigned int intended_height);
	static QBitArray producePaddedVariant1bit(const QImage& glyph, unsigned int padding);
	static QImage producePaddedVariantOfImage(const QImage& glyph, unsigned int padding);
	static double convert26_6ToDouble(int32_t fixedPointValue);
	static double convert16_16ToDouble(int32_t fixedPointValue);
	// Just for decomposing outlines
	static int Outline_MoveToFunc(const FT_Vector* to, void* user);
	static int Outline_LineToFunc(const FT_Vector* to, void* user );
	static int Outline_ConicToFunc(const FT_Vector* control, const FT_Vector*  to, void* user);
	static int Outline_CubicToFunc(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user);
};

#endif // SDFGENERATIONCONTEXT_HPP
