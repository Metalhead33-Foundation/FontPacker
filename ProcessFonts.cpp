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
#include <QOpenGLTexture>
#include <glm/glm.hpp>
#include <QOpenGLExtraFunctions>
#include <iostream>

#if defined (_MSC_VER)
#define STD140 __declspec(align(16))
#elif defined(__GNUC__) || defined(__clang__)
#define STD140 __attribute__((aligned(16)))
#else
#define STD140
#endif

#ifdef HIRES
const unsigned INTENDED_SIZE = 4096;
const unsigned PADDING = 300;
#else
const unsigned INTENDED_SIZE = 1024;
const unsigned PADDING = 100;
#endif


void processFonts(const QVariantMap& args)
{
	QTextStream errStrm(stderr);
	SDFGenerationArguments args2;
	args2.fromArgs(args);
	unsigned to_scale = args2.intendedSize - args2.padding;
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
	auto minChar = args.value(QStringLiteral("mincharcode"),0).toUInt();
	auto maxChar = args.value(QStringLiteral("mincharcode"),0xE007F).toUInt();
	switch(args2.mode) {
		case SOFTWARE: break;
		case OPENGL_COMPUTE: {
			QSurfaceFormat fmt;
			fmt.setDepthBufferSize(24);
			if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
				qDebug("Requesting OpenGL 4.3 core context");
				fmt.setVersion(4, 3);
				fmt.setProfile(QSurfaceFormat::CoreProfile);
			} else {
				qDebug("Requesting OpenGL ES 3.2 context");
				fmt.setVersion(3, 2);
			}
			args2.glHelpers = std::make_unique<GlHelpers>();
			args2.glShader = std::make_unique<QOpenGLShaderProgram>();
			if(!args2.glShader->create()) throw std::runtime_error("Failed to create shader!");
			QFile res(args2.distType == DistanceType::Eucledian ? ":/shader1.glsl" : ":/shader2.glsl");
			if(res.open(QFile::ReadOnly)) {
				QByteArray shdrArr = res.readAll();
				if(!args2.glShader->addCacheableShaderFromSourceCode(QOpenGLShader::Compute,shdrArr)) {
					errStrm << args2.glShader->log() << '\n';
					errStrm.flush();
					throw std::runtime_error("Failed to add shader!");
				}
				if(!args2.glShader->link()) {
					errStrm << args2.glShader->log() << '\n';
					errStrm.flush();
					throw std::runtime_error("Failed to compile OpenGL shader!");
				}
			}
			break;
		}
		case OPENCL: {
			break;
		}
		default: break;
	}
	for(uint32_t charcode = minChar; charcode < maxChar; ++charcode) {
		auto glyph_index = FT_Get_Char_Index( face, charcode );
		if(glyph_index) {
			++validGylph;
			error = FT_Load_Glyph(face,glyph_index,FT_LOAD_NO_BITMAP);
			if ( error ) throw std::runtime_error("Failed to load glyph.");
			StoredCharacter strChar;
			strChar.fromFreeTypeGlyph(face->glyph, args2);
		}
	}
	FT_Done_Face(face);
	FT_Done_FreeType( library );
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
	float f;
	bool isInside;
};

QImage produceSdfSoft(const QBitArray& source, unsigned width, unsigned height, const SDFGenerationArguments& args) {
	QImage sdf(width,height,QImage::Format_Grayscale8);
	//std::vector<float> tmpFloat(with * height);
	//double maxDist = sqrt(static_cast<double>(source.width()) * static_cast<double>(source.height()));
	const unsigned half_samples_to_check_x = args.samples_to_check_x ? args.samples_to_check_x / 2 : width / 2 ;
	const unsigned half_samples_to_check_y = args.samples_to_check_y ? args.samples_to_check_y / 2 : height / 2 ;

	const float maxDist = (args.distType == DistanceType::Eucledian)
							  ? (std::sqrt(static_cast<float>(half_samples_to_check_x) * static_cast<float>(half_samples_to_check_y)))
							   : static_cast<float>(half_samples_to_check_x+half_samples_to_check_y);

	const auto distanceCalculator = (args.distType == DistanceType::Eucledian)
										?
										( [](const glm::ivec2& a, const glm::ivec2& b) {
											  return glm::distance(glm::fvec2(a),glm::fvec2(b));
										} )
										:
										( [](const glm::ivec2& a, const glm::ivec2& b) {
											  return static_cast<float>(std::abs(a.x - b.x) + std::abs(a.y - b.y));
										  } );

	std::vector<TmpStoredDist> storedDists(width * height);

	auto calculateSdfForPixel = [&source,&distanceCalculator,maxDist,half_samples_to_check_x,half_samples_to_check_y,width,height](unsigned x, unsigned y, bool isInside) {
		const unsigned min_offset_x = static_cast<unsigned>(std::max( static_cast<int>(x)-static_cast<int>(half_samples_to_check_x), 0 ));
		const unsigned max_offset_x = static_cast<unsigned>(std::min( static_cast<int>(x)+static_cast<int>(half_samples_to_check_x), static_cast<int>(width) ));
		const unsigned min_offset_y = static_cast<unsigned>(std::max( static_cast<int>(y)-static_cast<int>(half_samples_to_check_y), 0 ));
		const unsigned max_offset_y = static_cast<unsigned>(std::min( static_cast<int>(y)+static_cast<int>(half_samples_to_check_y), static_cast<int>(height) ));
		float minDistance = maxDist;
		for(unsigned offset_y = min_offset_y; offset_y < max_offset_y; ++offset_y ) {
			const unsigned row_start = offset_y * width;
			for(unsigned offset_x = min_offset_x; offset_x < max_offset_x; ++offset_x ) {
				bool isEdge = source.testBit(row_start+offset_x) != isInside;
				if(isEdge) {
					float dist = distanceCalculator(glm::ivec2(x,y),glm::ivec2(offset_x,offset_y));
					if(dist <= minDistance) minDistance = dist;
				}
			}
		}
		return minDistance;
	};
	#pragma omp parallel for collapse(2)
	for(int y = 0; y < height;++y) {
		for(int x = 0; x < width; ++x) {
			TmpStoredDist* out_row_start = &storedDists[y * width];
			const unsigned in_row_start = y * width;
			const bool isInside = source.testBit(in_row_start+x);
			out_row_start[x].isInside = isInside;
			out_row_start[x].f = calculateSdfForPixel(x,y,isInside);
		}
	}

	float maxDistIn = std::numeric_limits<float>::epsilon();
	float maxDistOut = std::numeric_limits<float>::epsilon();
	for(const auto& it : storedDists) {
		if(it.isInside) {
			maxDistIn = std::max(maxDistIn,it.f);
		} else {
			maxDistOut = std::max(maxDistOut,it.f);
		}
	}
	// Add a small epsilon to avoid division by zero
	for(auto& it : storedDists) {
		if(it.isInside) {
			it.f /= maxDistIn;
			it.f = 0.5f + (it.f * 0.5f);
		} else {
			it.f /= maxDistOut;
			it.f = 0.5f - (it.f * 0.5f);
		}
	}

	for(int y = 0; y < height;++y) {
		uchar* row = sdf.scanLine(y);
		const TmpStoredDist* in_row_start = &storedDists[y * width];
		for(int x = 0; x < width; ++x) {
			const TmpStoredDist& in = in_row_start[x];
			//float sdfValue = (in.isInside ? 0.5f + (in.f / 2.0f) : 0.5f - (in.f / 2.0f));
			row[x] = static_cast<uint8_t>(in.f * 255.0f);
		}
	}
	return sdf;
}

struct STD140 UniformForCompute {
	int32_t width, height;
	int32_t padding[2];
};
QImage produceSdfGL(const QImage& source, unsigned width, unsigned height, const SDFGenerationArguments& args) {
	glPixelStorei( GL_PACK_ALIGNMENT, 1);
	glPixelStorei(  GL_UNPACK_ALIGNMENT, 1);
	QImage newimg(source.width(), source.height(), QImage::Format_Grayscale8);
	// Old Tex
	GlTexture oldTex(source);
	// New Tex
	GlTexture newTex(newimg.width(),newimg.height(), { GL_R32F, GL_RED, GL_FLOAT } );
	// New Tex 2
	GlTexture newTex2(newimg.width(),newimg.height(), { GL_R8, GL_RED, GL_UNSIGNED_BYTE } );
	GlStorageBuffer uniformBuffer(args.glHelpers->glFuncs, args.glHelpers->extraFuncs);
	UniformForCompute uniform;
	uniform.width = args.samples_to_check_x ? args.samples_to_check_x / 2 : width / 2;
	uniform.height = args.samples_to_check_y ? args.samples_to_check_y / 2 : height / 2;
	uniformBuffer.initializeFrom(uniform);
	args.glHelpers->glFuncs->glUseProgram(args.glShader->programId());
	int fontUniform = args.glShader->uniformLocation("fontTexture");
	int sdfUniform1 = args.glShader->uniformLocation("rawSdfTexture");
	int sdfUniform2 = args.glShader->uniformLocation("isInsideTex");
	int dimensionsUniform = args.glShader->uniformLocation("Dimensions");
	oldTex.bindAsImage(args.glHelpers->extraFuncs, 0, GL_READ_ONLY);
	args.glHelpers->glFuncs->glUniform1i(fontUniform,0);
	newTex.bindAsImage(args.glHelpers->extraFuncs, 1, GL_WRITE_ONLY);
	args.glHelpers->glFuncs->glUniform1i(sdfUniform1,1);
	newTex2.bindAsImage(args.glHelpers->extraFuncs, 2, GL_WRITE_ONLY);
	args.glHelpers->glFuncs->glUniform1i(sdfUniform2,2);
	uniformBuffer.bindBase(3);
	args.glHelpers->extraFuncs->glUniformBlockBinding(args.glShader->programId(), dimensionsUniform, 3);
	args.glHelpers->extraFuncs->glDispatchCompute(newimg.width(),newimg.height(),1);
	args.glHelpers->extraFuncs->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	std::vector<uint8_t> areTheyInside = newTex2.getTextureAs<uint8_t>();
	std::vector<float> rawDistances = newTex.getTextureAs<float>();
	float maxDistIn = std::numeric_limits<float>::epsilon();
	float maxDistOut = std::numeric_limits<float>::epsilon();
	for(size_t i = 0; i < rawDistances.size(); ++i) {
		if(areTheyInside[i]) {
			maxDistIn = std::max(maxDistIn,rawDistances[i]);
		} else {
			maxDistOut = std::max(maxDistOut,rawDistances[i]);
		}
	}
	for(size_t i = 0; i < rawDistances.size(); ++i) {
		float& it = rawDistances[i];
		if(areTheyInside[i]) {
			it /= maxDistIn;
			it = 0.5f + (it * 0.5f);
		} else {
			it /= maxDistOut;
			it = 0.5f - (it * 0.5f);
		}
	}

	for(int y = 0; y < newimg.height(); ++y) {
		uchar* scanline = newimg.scanLine(y);
		const float* rawScanline = &rawDistances[newimg.width()*y];
		for(int x = 0; x < newimg.width(); ++x) {
			scanline[x] = static_cast<uint8_t>(rawScanline[x] * 255.f);
		}
	}
	return newimg;
}

QImage producePaddedVariant(const unsigned char* glyph, unsigned padding, unsigned width_org, unsigned height_org, unsigned width_padded, unsigned height_padded) {
	QImage img(width_padded,height_padded,QImage::Format_Grayscale8);
	img.fill(0);
	for(unsigned y = padding; y < height_padded-padding;++y) {
		uchar* row = img.scanLine(y);
		const uchar* inRow = &glyph[(y-padding)*width_org];
		memcpy(&row[padding],inRow,width_org);
	}
	return img;
}
QBitArray producePaddedVariant1bit(const unsigned char* glyph, unsigned padding, unsigned width_org, unsigned height_org, unsigned width_padded, unsigned height_padded) {
	QBitArray newBits(width_padded * height_padded,false);
	for(unsigned y = padding; y < height_padded-padding; ++y) {
		const unsigned row_start = y*width_padded;
		const uchar* inRow = &glyph[(y-padding)*width_org];
		for(unsigned x = padding; x < width_padded-padding;++x) {
			const unsigned row_i = row_start + x;
			newBits.setBit(row_i,inRow[x-padding] >= 128);
		}
	}
	return newBits;
}

void StoredCharacter::fromFreeTypeGlyph(FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args)
{
	auto error = FT_Render_Glyph( glyphSlot, FT_RENDER_MODE_NORMAL);
	if ( error ) throw std::runtime_error("Failed to render glyph.");
	if(glyphSlot->bitmap.rows <= 1 || glyphSlot->bitmap.width <= 1) return;

	const unsigned padding = args.padding;
	const unsigned width_org = glyphSlot->bitmap.width;
	const unsigned height_org = glyphSlot->bitmap.rows;
	const unsigned width_padded = width_org + (args.padding*2);
	const unsigned height_padded = height_org + (args.padding*2);

	QTextStream strm(stdout);
	strm << "Width: " << glyphSlot->bitmap.width << '\n';
	strm << "Rows: " << glyphSlot->bitmap.rows << '\n';
	QByteArray tmpArr;
	QBuffer buff(&tmpArr);
	QImage img;
	switch(args.mode) {
		case SOFTWARE: {
			QBitArray newBits = producePaddedVariant1bit(glyphSlot->bitmap.buffer, padding, width_org, height_org, width_padded, height_padded);
			img = produceSdfSoft(newBits, width_padded, height_padded, args);
			break;
		}
		case OPENGL_COMPUTE: {
			QImage oldImg = producePaddedVariant(glyphSlot->bitmap.buffer, padding, width_org, height_org, width_padded, height_padded)
			.scaled(args.intendedSize,args.intendedSize,Qt::AspectRatioMode::IgnoreAspectRatio,Qt::TransformationMode::SmoothTransformation);
			img = produceSdfGL(oldImg,oldImg.width(),oldImg.height(),args);
			break;
		}
		case OPENCL: {
			throw std::runtime_error("Unsupported mode!");
		}
		default: {
			throw std::runtime_error("Unsupported mode!");
		}
	}
	//img = img.scaled(32,32,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
	buff.open(QIODevice::WriteOnly);
	if(!img.save(&buff,"PNG",-1)) throw std::runtime_error("Failed to save image!");
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

/*
	unsigned intendedSize;
	unsigned padding;
	unsigned samples_to_check_x;
	unsigned samples_to_check_y;
*/

void SDFGenerationArguments::fromArgs(const QVariantMap& args)
{
	this->intendedSize = args.value(QStringLiteral("intendedsize"),INTENDED_SIZE).toUInt();
	this->padding = args.value(QStringLiteral("padding"),PADDING).toUInt();
	this->samples_to_check_x = args.value(QStringLiteral("samplestocheckx"),0).toUInt();
	this->samples_to_check_y = args.value(QStringLiteral("samplestochecky"),0).toUInt();
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
