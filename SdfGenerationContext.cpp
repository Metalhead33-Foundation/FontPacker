#include "RGBA8888.hpp"
#include "SdfGenerationContext.hpp"
#include <stdexcept>
#include <QTextStream>
#include <cstdint>
#include <QBuffer>
#include <QBitArray>

/*
	static int Outline_MoveToFunc(const FT_Vector* to, void* user);
	static int Outline_LineToFunc(const FT_Vector* to, void* user );
	static int Outline_ConicToFunc(const FT_Vector* control, const FT_Vector*  to, void* user);
	static int Outline_CubicToFunc(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user);

	FT_Outline_MoveToFunc   move_to;
	FT_Outline_LineToFunc   line_to;
	FT_Outline_ConicToFunc  conic_to;
	FT_Outline_CubicToFunc  cubic_to;

	int                     shift;
	FT_Pos                  delta;
*/
FT_Outline_Funcs outlineFuncs {
	.move_to = SdfGenerationContext::Outline_MoveToFunc,
	.line_to = SdfGenerationContext::Outline_LineToFunc,
	.conic_to = SdfGenerationContext::Outline_ConicToFunc,
	.cubic_to = SdfGenerationContext::Outline_CubicToFunc,
	.shift = 0,
	.delta = 0
};



static uint32_t nextPowerOf2(uint32_t n) {
	if (n == 0) return 1;
	int k = 31 - __builtin_clz(n);  // Highest bit position
	if (n & (n - 1)) return 1U << (k + 1);  // Not a power of 2, shift up
	return 1U << k;  // Already a power of 2
}

double SdfGenerationContext::convert26_6ToDouble(int32_t fixedPointValue) {
	// Extract the integer part by shifting right by 6 bits
	int32_t integerPart = fixedPointValue >> 6;

	// Extract the fractional part by masking the lower 6 bits
	int32_t fractionalPart = fixedPointValue & 0x3F; // 0x3F is 00111111 in binary

	// Convert the fractional part to a double
	double fractionalValue = static_cast<double>(fractionalPart) / 64.0;

	// Combine the integer and fractional parts
	double result = static_cast<double>(integerPart) + fractionalValue;

	return result;
}
double SdfGenerationContext::convert16_16ToDouble(int32_t fixedPointValue) {
	// Extract the integer part by shifting right by 16 bits
	int32_t integerPart = fixedPointValue >> 16;

	// Extract the fractional part by masking the lower 16 bits
	int32_t fractionalPart = fixedPointValue & 0xFFFF;

	// Convert the fractional part to a double
	double fractionalValue = static_cast<double>(fractionalPart) / 65536.0;

	// Combine the integer and fractional parts
	double result = static_cast<double>(integerPart) + fractionalValue;

	return result;
}

int SdfGenerationContext::Outline_MoveToFunc(const FT_Vector* to, void* user)
{
	SdfGenerationContext* ctx = static_cast<SdfGenerationContext*>(user);
	glm::fvec2 toConverted(convert26_6ToDouble(to->x), convert26_6ToDouble(to->y));
	return ctx->decompositionContext.moveTo(toConverted);
}

int SdfGenerationContext::Outline_LineToFunc(const FT_Vector* to, void* user)
{
	SdfGenerationContext* ctx = static_cast<SdfGenerationContext*>(user);
	glm::fvec2 toConverted(convert26_6ToDouble(to->x), convert26_6ToDouble(to->y));
	return ctx->decompositionContext.lineTo(toConverted);
}

int SdfGenerationContext::Outline_ConicToFunc(const FT_Vector* control, const FT_Vector* to, void* user)
{
	SdfGenerationContext* ctx = static_cast<SdfGenerationContext*>(user);
	glm::fvec2 controlConverted(convert26_6ToDouble(control->x), convert26_6ToDouble(control->y));
	glm::fvec2 toConverted(convert26_6ToDouble(to->x), convert26_6ToDouble(to->y));
	return ctx->decompositionContext.conicTo(controlConverted, toConverted);
}

int SdfGenerationContext::Outline_CubicToFunc(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user)
{
	SdfGenerationContext* ctx = static_cast<SdfGenerationContext*>(user);
	glm::fvec2 control1Converted(convert26_6ToDouble(control1->x), convert26_6ToDouble(control1->y));
	glm::fvec2 control2Converted(convert26_6ToDouble(control2->x), convert26_6ToDouble(control2->y));
	glm::fvec2 toConverted(convert26_6ToDouble(to->x), convert26_6ToDouble(to->y));
	return ctx->decompositionContext.cubicTo(control1Converted, control2Converted, toConverted);
}

SdfGenerationContext::SdfGenerationContext() {
	auto error = FT_Init_FreeType( &library );
	if ( error ) {
		throw std::runtime_error("An error occurred during library initialization!");
	}
}

SdfGenerationContext::~SdfGenerationContext()
{
	FT_Done_FreeType( library );
}

uint8_t round4Num(uint8_t r, uint8_t g, uint8_t b, uint8_t a) {
	uint32_t sum = static_cast<uint32_t>(r) + static_cast<uint32_t>(g) + static_cast<uint32_t>(b) + static_cast<uint32_t>(a);
	return static_cast<uint8_t>( (sum + 2) / 4 );
}

QImage SdfGenerationContext::downsampleImageByAveraging(const QImage& src)
{
	QImage toReturn(src.width() / 2, src.height() / 2, src.format());
	if(src.format() == QImage::Format_Grayscale8) {
		for( int y = 0; y < toReturn.height(); ++y ) {
			uchar* outputScanline = toReturn.scanLine(y);
			const uchar* inputScanline0 = src.scanLine(y*2);
			const uchar* inputScanline1 = src.scanLine( (y*2)+1 );
			for ( int x = 0; x < toReturn.width() ; ++x ) {  // Fixed: ++x instead of ++y
				uchar& output = outputScanline[x];
				uchar in00 = inputScanline0[x*2];
				uchar in01 = inputScanline0[(x*2)+1];
				uchar in10 = inputScanline1[x*2];
				uchar in11 = inputScanline1[(x*2)+1];
				output = round4Num(in00,in01,in10,in11);
			}
		}
	} else if (src.format() == QImage::Format_RGBA8888) {
		for( int y = 0; y < toReturn.height(); ++y ) {
			RGBA8888* outputScanline = reinterpret_cast<RGBA8888*>(toReturn.scanLine(y));
			const RGBA8888* inputScanline0 = reinterpret_cast<const RGBA8888*>(src.scanLine(y*2));
			const RGBA8888* inputScanline1 = reinterpret_cast<const RGBA8888*>(src.scanLine((y*2)+1));

			for ( int x = 0; x < toReturn.width() ; ++x ) {
				const RGBA8888& in00 = inputScanline0[x*2];
				const RGBA8888& in01 = inputScanline0[(x*2)+1];
				const RGBA8888& in10 = inputScanline1[x*2];
				const RGBA8888& in11 = inputScanline1[(x*2)+1];

				// Average components
				RGBA8888 top = in00.averageWith(in01);
				RGBA8888 bottom = in10.averageWith(in11);
				outputScanline[x] = top.averageWith(bottom);
			}
		}
	}
	return toReturn;
}

QImage SdfGenerationContext::dowsanmpleImageByMaxing(const QImage& src)
{
	QImage toReturn(src.width() / 2, src.height() / 2, src.format());
	if(src.format() == QImage::Format_Grayscale8) {
		for( int y = 0; y < toReturn.height(); ++y ) {
			uchar* outputScanline = toReturn.scanLine(y);
			const uchar* inputScanline0 = src.scanLine(y*2);
			const uchar* inputScanline1 = src.scanLine( (y*2)+1 );
			for ( int x = 0; x < toReturn.width() ; ++x ) {  // Fixed: ++x instead of ++y
				uchar& output = outputScanline[x];
				uchar in00 = inputScanline0[x*2];
				uchar in01 = inputScanline0[(x*2)+1];
				uchar in10 = inputScanline1[x*2];
				uchar in11 = inputScanline1[(x*2)+1];
				output = std::max( std::max(in00,in01), std::max(in10,in11) );
			}
		}
	} else if (src.format() == QImage::Format_RGBA8888) {
		for( int y = 0; y < toReturn.height(); ++y ) {
			RGBA8888* outputScanline = reinterpret_cast<RGBA8888*>(toReturn.scanLine(y));
			const RGBA8888* inputScanline0 = reinterpret_cast<const RGBA8888*>(src.scanLine(y*2));
			const RGBA8888* inputScanline1 = reinterpret_cast<const RGBA8888*>(src.scanLine((y*2)+1));

			for ( int x = 0; x < toReturn.width() ; ++x ) {
				const RGBA8888& in00 = inputScanline0[x*2];
				const RGBA8888& in01 = inputScanline0[(x*2)+1];
				const RGBA8888& in10 = inputScanline1[x*2];
				const RGBA8888& in11 = inputScanline1[(x*2)+1];

				// Max components
				RGBA8888 top = in00.maxWith(in01);
				RGBA8888 bottom = in10.maxWith(in11);
				outputScanline[x] = top.maxWith(bottom);
			}
		}
	}
	return toReturn;
}

void SdfGenerationContext::processOutlineGlyph(StoredCharacter& output, FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args)
{
	output.valid = true;
	decompositionContext.clear();
	output.width = glyphSlot->bitmap.width;
	output.height = glyphSlot->bitmap.rows;
	output.bearing_x = glyphSlot->bitmap_left;
	output.bearing_y = glyphSlot->bitmap_top;
	output.advance_x = glyphSlot->advance.x;
	output.advance_y = glyphSlot->advance.y;
	output.metricWidth = convert26_6ToDouble(glyphSlot->metrics.width);
	output.metricHeight = convert26_6ToDouble(glyphSlot->metrics.height);
	output.horiBearingX = convert26_6ToDouble(glyphSlot->metrics.horiBearingX);
	output.horiBearingY = convert26_6ToDouble(glyphSlot->metrics.horiBearingY);
	output.horiAdvance = convert26_6ToDouble(glyphSlot->metrics.horiAdvance);
	output.vertBearingX = convert26_6ToDouble(glyphSlot->metrics.vertBearingX);
	output.vertBearingY = convert26_6ToDouble(glyphSlot->metrics.vertBearingY);
	output.vertAdvance = convert26_6ToDouble(glyphSlot->metrics.vertAdvance);
	auto orientation = FT_Outline_Get_Orientation(&glyphSlot->outline);
	FT_Outline_Decompose(&glyphSlot->outline,&outlineFuncs,this);
	decompositionContext.closeShape();
	decompositionContext.makeShapeIdsSigend( orientation != FT_ORIENTATION_TRUETYPE);
	decompositionContext.orientContours();
	decompositionContext.translateToNewSize(args.internalProcessSize,args.internalProcessSize,args.padding,args.padding, output.metricWidth, output.metricHeight, output.horiBearingX, output.horiBearingY);
	if(args.msdfgenColouring) decompositionContext.assignColoursMsdfgen();
	else decompositionContext.assignColours();

	QBuffer buff(&output.sdf);
	QImage img = produceOutlineSdf(decompositionContext, args);

	if(args.intendedSize) {
			unsigned integerScale = args.internalProcessSize / args.intendedSize;
			unsigned powerOfTwoTargeet = nextPowerOf2(args.intendedSize);
			int steps = __builtin_clz(powerOfTwoTargeet) - __builtin_clz(args.internalProcessSize);
			if(args.maximizeInsteadOfAverage) {
				for(int i = 0; i < steps; ++i) {
					img = dowsanmpleImageByMaxing(img);
				}
			} else {
				for(int i = 0; i < steps; ++i) {
					img = downsampleImageByAveraging(img);
				}
			}
			if(img.width() != args.intendedSize) img = img.scaled(args.intendedSize,args.intendedSize,Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	buff.open(QIODevice::WriteOnly);
	if(!img.save(&buff, args.jpeg ? "JPG" : "PNG",-1)) throw std::runtime_error("Failed to save image!");
	buff.close();
}

QImage SdfGenerationContext::producePaddedVariantOfImage(const QImage& glyph, unsigned padding) {
	const unsigned width_padded = glyph.width() + (padding*2);
	const  unsigned height_padded = glyph.height() + (padding*2);
	QImage img(width_padded,height_padded,QImage::Format_Grayscale8);
	img.fill(0);
	for(unsigned y = padding; y < height_padded-padding;++y) {
		uchar* row = img.scanLine(y);
		const uchar* inRow = glyph.scanLine(y-padding);
		memcpy(&row[padding],inRow,glyph.width());
	}
	return img;
}
QBitArray SdfGenerationContext::producePaddedVariant1bit(const QImage& glyph, unsigned padding) {
	const unsigned width_padded = glyph.width() + (padding*2);
	const  unsigned height_padded = glyph.height() + (padding*2);
	QBitArray newBits(width_padded * height_padded,false);
	for(unsigned y = padding; y < height_padded-padding; ++y) {
		const unsigned row_start = y*width_padded;
		const uchar* inRow = glyph.scanLine(y-padding);
		for(unsigned x = padding; x < width_padded-padding;++x) {
			const unsigned row_i = row_start + x;
			newBits.setBit(row_i,inRow[x-padding] >= 128);
		}
	}
	return newBits;
}

QImage SdfGenerationContext::FTBitmap2QImage(const FT_Bitmap_& bitmap, unsigned intended_width, unsigned intended_height) {
	QImage toReturn(bitmap.width,bitmap.rows,QImage::Format_Grayscale8);
	for(unsigned y = 0; y < bitmap.rows; ++y) {
		uchar* row = toReturn.scanLine(y);
		const uchar* inRow = &bitmap.buffer[(y)*bitmap.width];
		memcpy(row,inRow,bitmap.width);
	}
	if(intended_width && intended_height) {
		toReturn = toReturn.scaled(intended_width, intended_height, Qt::AspectRatioMode::IgnoreAspectRatio, Qt::TransformationMode::SmoothTransformation);
	} else if(intended_width) {
		toReturn = toReturn.scaledToWidth(intended_width, Qt::TransformationMode::SmoothTransformation);
	} else if(intended_height) {
		toReturn = toReturn.scaledToHeight(intended_height, Qt::TransformationMode::SmoothTransformation);
	}
	return toReturn;
}

void SdfGenerationContext::processBitmapGlyph(StoredCharacter& output, FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args)
{
	auto error = FT_Render_Glyph( glyphSlot, FT_RENDER_MODE_NORMAL);
	if ( error ) {
		output.valid = false;
		return;
	}
	if(glyphSlot->bitmap.rows <= 1 || glyphSlot->bitmap.width <= 1) {
		output.valid = false;
		return;
	}
	output.valid = true;

	const unsigned padding = args.padding;
	output.width = glyphSlot->bitmap.width;
	output.height = glyphSlot->bitmap.rows;
	output.bearing_x = glyphSlot->bitmap_left;
	output.bearing_y = glyphSlot->bitmap_top;
	output.advance_x = glyphSlot->advance.x;
	output.advance_y = glyphSlot->advance.y;

	output.metricWidth = convert26_6ToDouble(glyphSlot->metrics.width);
	output.metricHeight = convert26_6ToDouble(glyphSlot->metrics.height);
	output.horiBearingX = convert26_6ToDouble(glyphSlot->metrics.horiBearingX);
	output.horiBearingY = convert26_6ToDouble(glyphSlot->metrics.horiBearingY);
	output.horiAdvance = convert26_6ToDouble(glyphSlot->metrics.horiAdvance);
	output.vertBearingX = convert26_6ToDouble(glyphSlot->metrics.vertBearingX);
	output.vertBearingY = convert26_6ToDouble(glyphSlot->metrics.vertBearingY);
	output.vertAdvance = convert26_6ToDouble(glyphSlot->metrics.vertAdvance);

	QBuffer buff(&output.sdf);
	QImage oldImg = FTBitmap2QImage(glyphSlot->bitmap, args.internalProcessSize - (args.padding*2), args.internalProcessSize - (args.padding*2));
	oldImg = producePaddedVariantOfImage(oldImg, padding);
	QImage img = produceBitmapSdf(oldImg, args);

	if(args.intendedSize) {
			unsigned integerScale = args.internalProcessSize / args.intendedSize;
			unsigned powerOfTwoTargeet = nextPowerOf2(args.intendedSize);
			int steps = __builtin_clz(powerOfTwoTargeet) - __builtin_clz(args.internalProcessSize);
			if(args.maximizeInsteadOfAverage) {
				for(int i = 0; i < steps; ++i) {
					img = dowsanmpleImageByMaxing(img);
				}
			} else {
				for(int i = 0; i < steps; ++i) {
					img = downsampleImageByAveraging(img);
				}
			}
			if(img.width() != args.intendedSize) img = img.scaled(args.intendedSize,args.intendedSize,Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
	}
	buff.open(QIODevice::WriteOnly);
	if(!img.save(&buff, args.jpeg ? "JPG" : "PNG",-1)) throw std::runtime_error("Failed to save image!");
	buff.close();
}

void SdfGenerationContext::processFont(PreprocessedFontFace& output, const SDFGenerationArguments& args)
{
	output.type = args.type;
	output.distType = args.distType;
	output.bitmap_size = args.intendedSize;
	output.bitmap_logical_size = args.internalProcessSize;
	output.bitmap_padding = args.padding;
	output.jpeg = args.jpeg;
	unsigned to_scale = args.internalProcessSize - args.padding;
	FT_Face face;
	auto fpath = args.font_path.toStdString();
	FT_Error error = FT_New_Face( library, fpath.c_str(), 0, &face );
	if ( error == FT_Err_Unknown_File_Format )
	{
		throw std::runtime_error("The font file could be opened and read, but it appears that its font format is unsupported.");
	}
	else if ( error )
	{
		throw std::runtime_error("Font file could not be read! Does it even exist?");
	}
	output.hasVert = FT_HAS_VERTICAL(face);
	output.fontFamilyName = QString::fromUtf8(face->family_name);
	error = FT_Set_Pixel_Sizes(face,to_scale,to_scale);
	if ( error ) {
		throw std::runtime_error("Failed to set character sizes.");
	}
	FT_Set_Transform(face,nullptr,nullptr);
	if ( error ) {
		throw std::runtime_error("Failed to set transform.");
	}
	auto minChar = args.char_min;
	auto maxChar = args.char_max;

	auto fonttype = FT_Get_FSType_Flags(face);

	QMap<uint32_t,uint32_t> charcodeToGlyphIndex;
	QMap<uint32_t,uint32_t> glyphIndexToCharCode;
	for(uint32_t charcode = minChar; charcode < maxChar; ++charcode) {
		auto glyph_index = FT_Get_Char_Index( face, charcode );
		if(glyph_index) {
			charcodeToGlyphIndex.insert(charcode,glyph_index);
			glyphIndexToCharCode.insert(glyph_index,charcode);
			error = FT_Load_Glyph(face,glyph_index,FT_LOAD_NO_BITMAP);
			if ( error ) throw std::runtime_error("Failed to load glyph.");
			StoredCharacter strdChr{};
			if( (face->glyph->outline.n_contours && face->glyph->outline.n_points) && !args.forceRaster ) {
				processOutlineGlyph(strdChr,face->glyph, args);
			} else {
				processBitmapGlyph(strdChr,face->glyph, args);
			}
			output.storedCharacters.insert(charcode, strdChr);
		}
	}
	if( FT_HAS_KERNING(face) )
	{
		FT_Vector kernVector;
		for( auto it = std::begin(charcodeToGlyphIndex); it != std::end(charcodeToGlyphIndex); ++it) {
			PerCharacterKerning tmpKern;
			for( auto zt = std::begin(charcodeToGlyphIndex); zt != std::end(charcodeToGlyphIndex); ++zt) {
				auto gotKerning = FT_Get_Kerning(face, it.value(), zt.value(), FT_KERNING_DEFAULT, &kernVector);
				if(!gotKerning && (kernVector.x || kernVector.y)) {
					Vec2f tmpVec;
					tmpVec.first = convert26_6ToDouble(kernVector.x);
					tmpVec.second = convert26_6ToDouble(kernVector.y);
					tmpKern.insert(zt.key(),tmpVec);
				}
			}
			if(tmpKern.size()) output.kerning.insert(it.key(),tmpKern);
		}
	}

	FT_Done_Face(face);
}
