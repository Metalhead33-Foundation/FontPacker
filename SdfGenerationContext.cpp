#include "SdfGenerationContext.hpp"
#include <harfbuzz/hb-ft.h>
#include <stdexcept>
#include <QTextStream>
#include <ft2build.h>
#include <cstdint>
#include <QBuffer>
#include <QBitArray>
#include FT_FREETYPE_H

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

SdfGenerationContext::SdfGenerationContext() {}

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

void SdfGenerationContext::processGlyph(StoredCharacter& output, FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args)
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
	output.bearing_x = glyphSlot->bitmap_top;
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
	QImage img = produceSdf(oldImg, args);
	/*switch(args.mode) {
		case SOFTWARE: {
			QBitArray newBits = producePaddedVariant1bit(oldImg, padding);
			img = produceSdfSoft(newBits, args.internalProcessSize, args.internalProcessSize, args);
			break;
		}
		case OPENGL_COMPUTE: {
			oldImg = producePaddedVariantOfImage(oldImg, padding);
			img = produceSdfGL(oldImg,args);
			break;
		}
		case OPENCL: {
			throw std::runtime_error("Unsupported mode!");
		}
		default: {
			throw std::runtime_error("Unsupported mode!");
		}
	}*/
	if(args.intendedSize) img = img.scaled(args.intendedSize,args.intendedSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
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
	FT_Library library;
	auto error = FT_Init_FreeType( &library );
	if ( error ) {
		throw std::runtime_error("An error occurred during library initialization!");
	}
	FT_Face face;
	auto fpath = args.font_path.toStdString();
	error = FT_New_Face( library, fpath.c_str(), 0, &face );
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
	/*switch(args.mode) {
		case SOFTWARE: break;
		case OPENGL_COMPUTE: {
			break;
		}
		case OPENCL: {
			break;
		}
		default: break;
	}*/
	QMap<uint32_t,uint32_t> charcodeToGlyphIndex;
	QMap<uint32_t,uint32_t> glyphIndexToCharCode;
	for(uint32_t charcode = minChar; charcode < maxChar; ++charcode) {
		auto glyph_index = FT_Get_Char_Index( face, charcode );
		if(glyph_index) {
			charcodeToGlyphIndex.insert(charcode,glyph_index);
			glyphIndexToCharCode.insert(glyph_index,charcode);
			error = FT_Load_Glyph(face,glyph_index,FT_LOAD_NO_BITMAP);
			if ( error ) throw std::runtime_error("Failed to load glyph.");
			StoredCharacter strdChr;
			processGlyph(strdChr,face->glyph, args);
			//strdChr.fromFreeTypeGlyph(face->glyph, args);
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
	FT_Done_FreeType( library );
}
