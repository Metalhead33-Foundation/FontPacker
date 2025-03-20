#ifndef SDFGENERATIONCONTEXT_HPP
#define SDFGENERATIONCONTEXT_HPP
#include <QImage>
#include "SDFGenerationArguments.hpp"
#include "PreprocessedFontFace.hpp"
#include "StoredCharacter.hpp"

struct FT_Bitmap_;
class SdfGenerationContext
{
public:
	SdfGenerationContext();
	virtual QImage produceSdf(const QImage& source, const SDFGenerationArguments& args) = 0;
	void processGlyph(StoredCharacter& output, FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args);
	void processFont(PreprocessedFontFace& output, const SDFGenerationArguments& args);
	static QImage FTBitmap2QImage(const FT_Bitmap_& bitmap, unsigned int intended_width, unsigned int intended_height);
	static QBitArray producePaddedVariant1bit(const QImage& glyph, unsigned int padding);
	static QImage producePaddedVariantOfImage(const QImage& glyph, unsigned int padding);
	static double convert26_6ToDouble(int32_t fixedPointValue);
	static double convert16_16ToDouble(int32_t fixedPointValue);
};

#endif // SDFGENERATIONCONTEXT_HPP
