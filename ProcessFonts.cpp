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
#include <QBitArray>
#include <glm/glm.hpp>
#include <iostream>

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
			SDFGenerationArguments args2;
			args2.fromArgs(args);
			StoredCharacter strChar;
			strChar.fromFreeTypeGlyph(face->glyph, args2);
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

struct TmpStoredDist {
	double f;
	bool isInside;
};

QImage produceSdf(const QBitArray& source, unsigned width, unsigned height, const SDFGenerationArguments& args) {
	QImage sdf(width,height,QImage::Format_Grayscale8);
	//std::vector<float> tmpFloat(with * height);
	//double maxDist = sqrt(static_cast<double>(source.width()) * static_cast<double>(source.height()));
	const double maxDist = (args.distType == DistanceType::Eucledian)
							   ? (half_samples_to_check * std::sqrt(2.0))
							   : static_cast<double>(max_samples_to_check);

	const auto distanceCalculator = (args.distType == DistanceType::Eucledian)
										?
										( [](const glm::ivec2& a, const glm::ivec2& b) {
											  return glm::distance(glm::dvec2(a),glm::dvec2(b));
										} )
										:
										( [](const glm::ivec2& a, const glm::ivec2& b) {
											  return static_cast<double>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
										  } );

	std::vector<TmpStoredDist> storedDists(width * height);

	auto calculateSdfForPixel = [&source,&distanceCalculator,maxDist,width,height](unsigned x, unsigned y, bool isInside) {
		const unsigned min_offset_x = static_cast<unsigned>(std::max( static_cast<int>(x)-static_cast<int>(half_samples_to_check), 0 ));
		const unsigned max_offset_x = static_cast<unsigned>(std::min( static_cast<int>(x)+static_cast<int>(half_samples_to_check), static_cast<int>(width) ));
		const unsigned min_offset_y = static_cast<unsigned>(std::max( static_cast<int>(y)-static_cast<int>(half_samples_to_check), 0 ));
		const unsigned max_offset_y = static_cast<unsigned>(std::min( static_cast<int>(y)+static_cast<int>(half_samples_to_check), static_cast<int>(height) ));
		double minDistance = maxDist;
		for(unsigned offset_y = min_offset_y; offset_y < max_offset_y; ++offset_y ) {
			const unsigned row_start = offset_y * width;
			for(unsigned offset_x = min_offset_x; offset_x < max_offset_x; ++offset_x ) {
				bool isEdge = source.testBit(row_start+offset_x) != isInside;
				if(isEdge) {
					double dist = distanceCalculator(glm::ivec2(x,y),glm::ivec2(offset_x,offset_y));
					if(dist <= minDistance) minDistance = dist;
				}
			}
		}
		return minDistance;
	};
	#pragma omp parallel for collapse(2)
	for(int y = 0; y < height;++y) {
		TmpStoredDist* out_row_start = &storedDists[y * width];
		const unsigned in_row_start = y * width;
		for(int x = 0; x < width; ++x) {
			const bool isInside = source.testBit(in_row_start+x);
			out_row_start[x].isInside = isInside;
			out_row_start[x].f = calculateSdfForPixel(x,y,isInside);
		}
	}

	double maxDistIn = 0.0;
	double maxDistOut = 0.0;
	for(const auto& it : storedDists) {
		if(it.isInside) {
			maxDistIn = std::max(maxDistIn,it.f);
		} else {
			maxDistOut = std::max(maxDistOut,it.f);
		}
	}
	std::cout << maxDistIn << ' ' << maxDistOut << '\n';
	for(auto& it : storedDists) {
		if(it.isInside) {
			it.f /= maxDistIn;
		} else {
			it.f /= maxDistOut;
		}
	}

	#pragma omp parallel for collapse(2)
	for(int y = 0; y < height;++y) {
		uchar* row = sdf.scanLine(y);
		const TmpStoredDist* in_row_start = &storedDists[y * width];
		for(int x = 0; x < width; ++x) {
			const TmpStoredDist& in = in_row_start[x];
			double sdfValue = (in.isInside ? 0.5 + (in.f / 2.0) : 0.5 - (in.f / 2.0));
			row[x] = static_cast<uint8_t>(sdfValue * 255.0);
		}
	}
	return sdf;
}

void StoredCharacter::fromFreeTypeGlyph(FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args)
{
	auto error = FT_Render_Glyph( glyphSlot, FT_RENDER_MODE_NORMAL);
	if ( error ) throw std::runtime_error("Failed to render glyph.");
	if(glyphSlot->bitmap.rows <= 1 || glyphSlot->bitmap.width <= 1) return;

	const unsigned width_org = glyphSlot->bitmap.width;
	const unsigned height_org = glyphSlot->bitmap.rows;
	const unsigned width_padded = width_org + (padding*2);
	const unsigned height_padded = height_org + (padding*2);

	QTextStream strm(stdout);
	strm << "Width: " << glyphSlot->bitmap.width << '\n';
	strm << "Rows: " << glyphSlot->bitmap.rows << '\n';

	//QByteArray tmpArrA = QByteArray::fromRawData(reinterpret_cast<char*>(glyphSlot->bitmap.buffer),glyphSlot->bitmap.width * glyphSlot->bitmap.rows);
	QBitArray newBits(width_padded * height_padded,false);
	for(unsigned y = padding; y < height_padded-padding; ++y) {
		const unsigned row_start = y*width_padded;
		const unsigned row_start_org = (y-padding)*width_org;
		const uchar* inRow = &glyphSlot->bitmap.buffer[(y-padding)*width_org];
		for(unsigned x = padding; x < width_padded-padding;++x) {
			const unsigned row_i = row_start + x;
			const unsigned row_i_og = row_start_org + x - padding;
			newBits.setBit(row_i,inRow[x-padding] >= 128);
		}
	}

	QImage img = produceSdf(newBits, width_padded, height_padded, args);
	//img = img.scaled(32,32,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
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

const QString mode_str = QStringLiteral("mode");
const QString type_str = QStringLiteral("type");
const QString dist_str = QStringLiteral("dist");

const QString software_mode_str = QStringLiteral("Software");
const QString opengl_mode_str = QStringLiteral("OpenGL");
const QString opencl_mode_str = QStringLiteral("OpenCL");
const QString sdf_mode_str = QStringLiteral("SDF");
const QString msdfa_mode_str = QStringLiteral("MSDFA");
const QString manhattan_mode_str = QStringLiteral("Manhattan");
const QString eucledian_mode_str = QStringLiteral("Eucledian");

void SDFGenerationArguments::fromArgs(const QVariantMap& args)
{
	// Mode
	{
		QVariant genMod = args.value(mode_str,software_mode_str);
		switch (genMod.typeId() ) {
			case QMetaType::QString: {
				QString genModS = genMod.toString();
				if(!genModS.compare(software_mode_str,Qt::CaseInsensitive)) this->mode = SDfGenerationMode::SOFTWARE;
				else if(!genModS.compare(opengl_mode_str,Qt::CaseInsensitive)) this->mode = SDfGenerationMode::OPENGL_COMPUTE;
				else if(!genModS.compare(opencl_mode_str,Qt::CaseInsensitive)) this->mode = SDfGenerationMode::OPENCL;
				else this->mode = SDfGenerationMode::SOFTWARE;
				break;
			}
			case QMetaType::Int: {
				this->mode = static_cast<SDfGenerationMode>(genMod.toInt());
				break;
			}
			case QMetaType::UInt: {
				this->mode = static_cast<SDfGenerationMode>(genMod.toUInt());
				break;
			}
			default: {
				this->mode = SDfGenerationMode::SOFTWARE;
				break;
			}
		}
	}


	// Type
	{
		QVariant sdfType = args.value(type_str,sdf_mode_str);
		switch (sdfType.typeId() ) {
			case QMetaType::QString: {
				QString sdfTypeS = sdfType.toString();
				if(!sdfTypeS.compare(sdf_mode_str,Qt::CaseInsensitive)) this->type = SDFType::SDF;
				else if(!sdfTypeS.compare(msdfa_mode_str,Qt::CaseInsensitive)) this->type = SDFType::MSDFA;
				else this->type = SDFType::SDF;
				break;
			}
			case QMetaType::Int: {
				this->type = static_cast<SDFType>(sdfType.toInt());
				break;
			}
			case QMetaType::UInt: {
				this->type = static_cast<SDFType>(sdfType.toUInt());
				break;
			}
			default: {
				this->type = SDFType::SDF;
				break;
			}
		}
	}

	// Dist
	{
		QVariant distType = args.value(dist_str,manhattan_mode_str);
		switch (distType.typeId() ) {
			case QMetaType::QString: {
				QString distTypeS = distType.toString();
				if(!distTypeS.compare(manhattan_mode_str,Qt::CaseInsensitive)) this->distType = DistanceType::Manhattan;
				else if(!distTypeS.compare(eucledian_mode_str,Qt::CaseInsensitive)) this->distType = DistanceType::Eucledian;
				else this->distType = DistanceType::Manhattan;
				break;
			}
			case QMetaType::Int: {
				this->distType = static_cast<DistanceType>(distType.toInt());
				break;
			}
			case QMetaType::UInt: {
				this->distType = static_cast<DistanceType>(distType.toUInt());
				break;
			}
			default: {
				this->distType = DistanceType::Manhattan;;
				break;
			}
		}
	}
}
