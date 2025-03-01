#include "ProcessFonts.hpp"
#include <stdexcept>
#include <QTextStream>
#include <ft2build.h>
#include <cstdint>
#include FT_FREETYPE_H
#include <QImage>
#include <QFile>
#include <QBuffer>
#include <cstring>
#include <cstdlib>
#include <memory>
#include <vector>
#include <cmath>
#include <glm/glm.hpp>

#ifdef HIRES
const unsigned intendedSize = 4096;
const unsigned padding = 300;
const unsigned max_samples_to_check = 128;
const unsigned half_samples_to_check = max_samples_to_check / 2;
const unsigned to_scale = intendedSize - padding;
#else
const unsigned intendedSize = 1024;
const unsigned padding = 100;
const unsigned max_samples_to_check = 64;
const unsigned half_samples_to_check = max_samples_to_check / 2;
const unsigned to_scale = intendedSize - padding;
#endif


void processFonts(const QVariantMap& args)
{
	int validGylph = 0;
	FT_Library library;
	auto error = FT_Init_FreeType( &library );
	if ( error ) {
		throw std::runtime_error("An error occurred during library initialization!");
	}
	FT_Face face;
	auto fpath = args.value(QStringLiteral("font"),QStringLiteral("/usr/share/fonts/truetype/arial.ttf")).toString().toStdString();
	error = FT_New_Face( library, fpath.c_str(), 0, &face );
	if ( error == FT_Err_Unknown_File_Format )
	{
		throw std::runtime_error("The font file could be opened and read, but it appears that its font format is unsupported.");
	}
	else if ( error )
	{
		throw std::runtime_error("Font file could not be read! Does it even exist?");
	}
	/*error = FT_Set_Char_Size(face,0,16*64,300,300);
	if ( error ) {
		throw std::runtime_error("Failed to set character sizes.");
	}*/
	error = FT_Set_Pixel_Sizes(face,to_scale,to_scale);
	if ( error ) {
		throw std::runtime_error("Failed to set character sizes.");
	}
	FT_Set_Transform(face,nullptr,nullptr);
	if ( error ) {
		throw std::runtime_error("Failed to set transform.");
	}
	auto minChar = args.value(QStringLiteral("minCharcode"),0).toUInt();
	auto maxChar = args.value(QStringLiteral("minCharcode"),0xE007F).toUInt();
	for(uint32_t charcode = minChar; charcode < maxChar; ++charcode) {
		auto glyph_index = FT_Get_Char_Index( face, charcode );
		if(glyph_index) {
			++validGylph;
			error = FT_Load_Glyph(face,glyph_index,FT_LOAD_NO_BITMAP);
			if ( error ) throw std::runtime_error("Failed to load glyph.");
			StoredCharacter strChar;
			strChar.fromFreeTypeGlyph(face->glyph);
		}
	}
	QTextStream strm(stdout);
	strm << validGylph << '\n';
}

const QString str_valid = QStringLiteral("valid");
const QString str_width = QStringLiteral("width");
const QString str_height = QStringLiteral("height");
const QString str_bearing_x = QStringLiteral("bearing_x");
const QString str_bearing_y = QStringLiteral("bearing_y");
const QString str_advance = QStringLiteral("advance");
const QString str_sdf = QStringLiteral("sdf");

void StoredCharacter::toCbor(QCborMap& cbor) const
{
	cbor.insert(str_valid,valid);
	cbor.insert(str_width,width);
	cbor.insert(str_height,height);
	cbor.insert(str_bearing_x,bearing_x);
	cbor.insert(str_bearing_y,bearing_y);
	cbor.insert(str_advance,advance);
	cbor.insert(str_sdf,sdf);
}

QCborMap StoredCharacter::toCbor() const
{
	QCborMap cbor;
	toCbor(cbor);
	return cbor;
}

void StoredCharacter::fromCbor(const QCborMap& cbor)
{
	this->valid = cbor[str_valid].toBool(false);
	this->width = cbor[str_width].toInteger(0);
	this->height = cbor[str_height].toInteger(0);
	this->bearing_x = cbor[str_bearing_x].toInteger(0);
	this->bearing_y = cbor[str_bearing_y].toInteger(0);
	this->advance = cbor[str_advance].toInteger(0);
	this->sdf = cbor[str_sdf].toByteArray();
}

template <typename T> struct aligned_arr {
private:
	aligned_arr(const aligned_arr& cpy) = delete;
	aligned_arr& operator=(const aligned_arr& cpy) = delete;
public:
	T* ptr;
	aligned_arr( size_t alignment, size_t size ) {
		ptr = static_cast<T*>(aligned_alloc(alignment, size * sizeof(T)));
	}
	~aligned_arr() {
		if(ptr) free(ptr);
	}
	aligned_arr(aligned_arr&& mov) {
		this->ptr = mov.ptr;
		mov.ptr = nullptr;
	}
	aligned_arr& operator=(aligned_arr&& mov) {
		this->ptr = mov.ptr;
		mov.ptr = nullptr;
		return *this;
	}
};


QImage produceSdf(const QImage& source) {
	QImage sdf(source.width(),source.height(),QImage::Format_Grayscale8);
	std::vector<float> tmpFloat(source.width() * source.height());
	//double maxDist = sqrt(static_cast<double>(source.width()) * static_cast<double>(source.height()));
	const double maxDist = static_cast<double>(max_samples_to_check);
	auto calculateSdfForPixel = [&source,maxDist](unsigned x, unsigned y, bool isInside) {
		const unsigned min_offset_x = static_cast<unsigned>(std::max( static_cast<int>(x)-static_cast<int>(half_samples_to_check), 0 ));
		const unsigned max_offset_x = static_cast<unsigned>(std::min( static_cast<int>(x)+static_cast<int>(half_samples_to_check), source.width() ));
		const unsigned min_offset_y = static_cast<unsigned>(std::max( static_cast<int>(y)-static_cast<int>(half_samples_to_check), 0 ));
		const unsigned max_offset_y = static_cast<unsigned>(std::min( static_cast<int>(y)+static_cast<int>(half_samples_to_check), source.height() ));
		double minDistance = maxDist;
		for(unsigned offset_y = min_offset_y; offset_y < max_offset_y; ++offset_y ) {
			const uchar* src = source.scanLine(offset_y);
			for(unsigned offset_x = min_offset_x; offset_x < max_offset_x; ++offset_x ) {
				bool isEdge = (src[offset_x] >= 128) != isInside;
				if(isEdge) {
					double dist = glm::distance(glm::dvec2(offset_x,offset_y),glm::dvec2(x,y));
					if(dist <= minDistance) minDistance = dist;
				}
			}
		}
		return minDistance / maxDist;
	};
	int height = sdf.height();
	int width = sdf.width();

	#pragma omp parallel for collapse(2)
	for(int y = 0; y < height;++y) {
		uchar* row = sdf.scanLine(y);
		const uchar* srcRow = source.scanLine(y);
		for(int x = 0; x < width; ++x) {
			const bool isInside = srcRow[x] >= 128;
			double sdfValue = calculateSdfForPixel(x,y,isInside);
			sdfValue = 1.0 - (isInside ? 0.5 - (sdfValue / 2.0) : 0.5 + (sdfValue / 2.0));
			row[x] = static_cast<uint8_t>(sdfValue * 128.0);
		}
	}
	return sdf;
}

void StoredCharacter::fromFreeTypeGlyph(FT_GlyphSlot glyphSlot, SDfGenerationMode genMode, SDFType type)
{
	auto error = FT_Render_Glyph( glyphSlot, FT_RENDER_MODE_NORMAL);
	if ( error ) throw std::runtime_error("Failed to render glyph.");

	const unsigned width_org = glyphSlot->bitmap.width;
	const unsigned height_org = glyphSlot->bitmap.rows;
	const unsigned width_padded = width_org + (padding*2);
	const unsigned height_padded = height_org + (padding*2);

	QTextStream strm(stdout);
	strm << "Width: " << glyphSlot->bitmap.width << '\n';
	strm << "Rows: " << glyphSlot->bitmap.rows << '\n';
	QByteArray tmpArrA = QByteArray::fromRawData(reinterpret_cast<char*>(glyphSlot->bitmap.buffer),glyphSlot->bitmap.width * glyphSlot->bitmap.rows);
	//QImage img(glyphSlot->bitmap.buffer,glyphSlot->bitmap.width,glyphSlot->bitmap.rows,QImage::Format_Grayscale8);

	QImage img(width_padded,height_padded,QImage::Format_Grayscale8);
	img.fill(0);
	//uchar* bits = img.bits();
	for(unsigned y = padding; y < height_padded-padding;++y) {
		//uchar* row = &bits[y*width_padded];
		uchar* row = img.scanLine(y);
		//const char* inRow = &tmpArrA[(y-padding)*width_org];
		const uchar* inRow = &glyphSlot->bitmap.buffer[(y-padding)*width_org];
		memcpy(&row[padding],inRow,width_org);
	}
	img = produceSdf(img);
	img = img.scaled(32,32,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
	QByteArray tmpArr;
	QBuffer buff(&tmpArr);
	buff.open(QIODevice::WriteOnly);
	img.save(&buff,"PNG",-1);
	buff.close();
	QFile f(QStringLiteral("/tmp/fonttmp/%1.png").arg(glyphSlot->glyph_index));
	if(f.open(QFile::WriteOnly)) {
		f.write(tmpArr);
		f.flush();
		f.close();
	}
}
