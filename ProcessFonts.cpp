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
#include "ConstStrings.hpp"

#if defined (_MSC_VER)
#define STD140 __declspec(align(16))
#elif defined(__GNUC__) || defined(__clang__)
#define STD140 __attribute__((aligned(16)))
#else
#define STD140
#endif

#ifdef HIRES
const unsigned INTERNAL_RENDER_SIZE = 4096;
const unsigned PADDING = 400;
#else
const unsigned INTERNAL_RENDER_SIZE = 1024;
const unsigned PADDING = 100;
#endif
const unsigned INTENDED_SIZE = 32;

void PreprocessedFontFace::processFonts(const QVariantMap& args)
{
	SDFGenerationArguments args2;
	args2.fromArgs(args);
	processFonts(args2);
}

void PreprocessedFontFace::processFonts(SDFGenerationArguments& args)
{
	QTextStream errStrm(stderr);
	this->bitmap_size = args.intendedSize;
	this->bitmap_padding = args.padding;
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
	switch(args.mode) {
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
			args.glHelpers = std::make_unique<GlHelpers>();
			args.glShader = std::make_unique<QOpenGLShaderProgram>();
			if(!args.glShader->create()) throw std::runtime_error("Failed to create shader!");
			QFile res(args.distType == DistanceType::Euclidean ? ":/shader1.glsl" : ":/shader2.glsl");
			if(res.open(QFile::ReadOnly)) {
				QByteArray shdrArr = res.readAll();
				if(!args.glShader->addCacheableShaderFromSourceCode(QOpenGLShader::Compute,shdrArr)) {
					errStrm << args.glShader->log() << '\n';
					errStrm.flush();
					throw std::runtime_error("Failed to add shader!");
				}
				if(!args.glShader->link()) {
					errStrm << args.glShader->log() << '\n';
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
			error = FT_Load_Glyph(face,glyph_index,FT_LOAD_NO_BITMAP);
			if ( error ) throw std::runtime_error("Failed to load glyph.");
			StoredCharacter strdChr;
			strdChr.fromFreeTypeGlyph(face->glyph, args);
			storedCharacters.insert(glyph_index, strdChr);
		}
	}
	FT_Done_Face(face);
	FT_Done_FreeType( library );
}

void PreprocessedFontFace::toCbor(QCborMap& cbor) const
{
	cbor.insert(TYPE_KEY, static_cast<unsigned>(type));
	cbor.insert(DIST_KEY, static_cast<unsigned>(distType));
	cbor.insert(BITMAP_SIZE_KEY, bitmap_size);
	cbor.insert(PADDING_KEY, bitmap_padding);
	QCborMap tmpMap;
	for(auto it = std::begin(storedCharacters); it != std::end(storedCharacters); ++it) {
		tmpMap.insert(it.key(),it.value().toCbor());
	}
	cbor.insert(GLYPHS_KEY, tmpMap);
}

QCborMap PreprocessedFontFace::toCbor() const
{
	QCborMap tmp;
	toCbor(tmp);
	return tmp;
}

void PreprocessedFontFace::fromCbor(const QCborMap& cbor)
{
	this->type = static_cast<SDFType>(cbor[TYPE_KEY].toInteger());
	this->distType = static_cast<DistanceType>(cbor[DIST_KEY].toInteger());
	this->bitmap_size = cbor[BITMAP_SIZE_KEY].toInteger();
	this->bitmap_padding = cbor[PADDING_KEY].toInteger();
	QCborMap tmpMap = cbor[GLYPHS_KEY].toMap();
	for(auto it = std::begin(tmpMap); it != std::end(tmpMap); ++it) {
		unsigned charCode = it.key().toInteger();
		StoredCharacter storedChar;
		storedChar.fromCbor(it.value().toMap());
		storedCharacters.insert(charCode, storedChar);
	}
}

void PreprocessedFontFace::toData(QDataStream& dataStream) const
{
/*
	SDFType type;
	DistanceType distType;
	uint32_t bitmap_size;
	uint32_t bitmap_padding;
	QMap<uint32_t,StoredCharacter> storedCharacters;
*/
	dataStream << static_cast<uint8_t>(type) << static_cast<uint8_t>(distType) << bitmap_size << bitmap_padding << static_cast<uint32_t>(storedCharacters.size());
	// Write table of contents
	QMap<uint32_t,uint32_t> offsets;
	auto currOffset = dataStream.device()->pos();
	for(qsizetype i = 0; i < storedCharacters.size(); ++i) {
		dataStream << uint32_t(0) << uint32_t(0);
	}
	for(auto it = std::begin(storedCharacters); it != std::end(storedCharacters); ++it) {
		auto glyphOffset = dataStream.device()->pos();
		offsets.insert(it.key(), glyphOffset);
		it->toData(dataStream);
	}
	dataStream.device()->seek(currOffset);
	for(auto it = std::begin(offsets); it != std::end(offsets); ++it) {
		dataStream << it.key() << it.value();
	}
}

void PreprocessedFontFace::fromData(QDataStream& dataStream)
{
	uint8_t tmpType, tmpDist;
	uint32_t charCount;
	dataStream >> tmpType >> tmpDist >> bitmap_size >> bitmap_padding >> charCount;
	this->type = static_cast<SDFType>(tmpType);
	this->distType = static_cast<DistanceType>(tmpDist);
	QMap<uint32_t,uint32_t> offsets;
	for(uint32_t i = 0; i < charCount; ++i) {
		uint32_t k,v;
		dataStream >> k >> v;
		offsets.insert(k,v);
	}
	for(auto it = std::begin(offsets); it != std::end(offsets); ++it) {
		dataStream.device()->seek(it.value());
		StoredCharacter tmpChar;
		tmpChar.fromData(dataStream);
		storedCharacters.insert(it.key(),tmpChar);
	}
}

void PreprocessedFontFace::outToFolder(const QString& pattern) const
{
	for(auto it = std::begin(storedCharacters); it != std::end(storedCharacters); ++it) {
		QFile fil(pattern.arg(it.key()));
		if(fil.open(QFile::WriteOnly)) {
			fil.write(it.value().sdf);
		}
	}
}

void StoredCharacter::toCbor(QCborMap& cbor) const
{
	cbor.insert(VALID_KEY,valid);
	cbor.insert(WIDTH_KEY,width);
	cbor.insert(HEIGHT_KEY,height);
	cbor.insert(BEARING_X_KEY,bearing_x);
	cbor.insert(BEARING_Y_KEY,bearing_y);
	cbor.insert(ADVANCE_X_KEY,advance_x);
	cbor.insert(ADVANCE_Y_KEY,advance_y);
	cbor.insert(SDF_KEY,sdf);
}

QCborMap StoredCharacter::toCbor() const
{
	QCborMap cbor;
	toCbor(cbor);
	return cbor;
}

void StoredCharacter::fromCbor(const QCborMap& cbor)
{
	this->valid = cbor[VALID_KEY].toBool(false);
	this->width = cbor[WIDTH_KEY].toInteger(0);
	this->height = cbor[HEIGHT_KEY].toInteger(0);
	this->bearing_x = cbor[BEARING_X_KEY].toInteger(0);
	this->bearing_y = cbor[BEARING_Y_KEY].toInteger(0);
	this->advance_x = cbor[ADVANCE_X_KEY].toInteger(0);
	this->advance_y = cbor[ADVANCE_Y_KEY].toInteger(0);
	this->sdf = cbor[SDF_KEY].toByteArray();
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
	const unsigned half_samples_to_check_x = args.samples_to_check_x ? args.samples_to_check_x / 2 : args.padding;
	const unsigned half_samples_to_check_y = args.samples_to_check_y ? args.samples_to_check_y / 2 : args.padding;

	const float maxDist = (args.distType == DistanceType::Euclidean)
							  ? (std::sqrt(static_cast<float>(half_samples_to_check_x) * static_cast<float>(half_samples_to_check_y)))
							   : static_cast<float>(half_samples_to_check_x+half_samples_to_check_y);

	const auto distanceCalculator = (args.distType == DistanceType::Euclidean)
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
QImage produceSdfGL(const QImage& source, const SDFGenerationArguments& args) {
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
	uniform.width = args.samples_to_check_x ? args.samples_to_check_x / 2 : args.padding;
	uniform.height = args.samples_to_check_y ? args.samples_to_check_y / 2 : args.padding;
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

QImage producePaddedVariant(const QImage& glyph, unsigned padding) {
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
QBitArray producePaddedVariant1bit(const QImage& glyph, unsigned padding) {
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

QImage fromFBitmap(const FT_Bitmap& bitmap, unsigned intended_width, unsigned intended_height) {
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

void StoredCharacter::fromFreeTypeGlyph(FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args)
{
	auto error = FT_Render_Glyph( glyphSlot, FT_RENDER_MODE_NORMAL);
	if ( error ) {
		valid = false;
		return;
	}
	if(glyphSlot->bitmap.rows <= 1 || glyphSlot->bitmap.width <= 1) {
		valid = false;
		return;
	}
	valid = true;

	const unsigned padding = args.padding;
	this->width = glyphSlot->bitmap.width;
	this->height = glyphSlot->bitmap.rows;
	this->bearing_x = glyphSlot->bitmap_left;
	this->bearing_x = glyphSlot->bitmap_top;
	this->advance_x = glyphSlot->advance.x;
	this->advance_y = glyphSlot->advance.y;

/*
	unsigned width; // Intended width: actual size is 32x32 or whatever else set in globally in the font
	unsigned height; // Intended height: actual size is 32x32 or whatever else set in globally in the font
	int bearing_x;
	int bearing_y;
	unsigned advance;
	QByteArray sdf;
*/

	QBuffer buff(&this->sdf);
	QImage oldImg = fromFBitmap(glyphSlot->bitmap, args.internalProcessSize - (args.padding*2), args.internalProcessSize - (args.padding*2));

	QImage img;
	switch(args.mode) {
		case SOFTWARE: {
			QBitArray newBits = producePaddedVariant1bit(oldImg, padding);
			img = produceSdfSoft(newBits, args.internalProcessSize, args.internalProcessSize, args);
			break;
		}
		case OPENGL_COMPUTE: {
			oldImg = producePaddedVariant(oldImg, padding);
			img = produceSdfGL(oldImg,args);
			break;
		}
		case OPENCL: {
			throw std::runtime_error("Unsupported mode!");
		}
		default: {
			throw std::runtime_error("Unsupported mode!");
		}
	}
	if(args.intendedSize) img = img.scaled(args.intendedSize,args.intendedSize,Qt::IgnoreAspectRatio,Qt::SmoothTransformation);
	buff.open(QIODevice::WriteOnly);
	if(!img.save(&buff,"PNG",-1)) throw std::runtime_error("Failed to save image!");
	buff.close();
}

void StoredCharacter::toData(QDataStream& dataStream) const
{
	/*
	bool valid;
	unsigned width; // Intended width: actual size is 32x32 or whatever else set in globally in the font
	unsigned height; // Intended height: actual size is 32x32 or whatever else set in globally in the font
	int bearing_x;
	int bearing_y;
	unsigned advance_x;
	unsigned advance_y;
	QByteArray sdf;
	*/
	dataStream << valid;
	if(valid) {
		dataStream << width << height << bearing_x << bearing_y << advance_x << advance_y << static_cast<uint32_t>(sdf.length());
		dataStream.writeRawData(sdf.data(),sdf.length());
	}
}

void StoredCharacter::fromData(QDataStream& dataStream)
{
	dataStream >> valid;
	if(valid) {
		uint32_t sdf_len;
		dataStream >> width >> height >> bearing_x >> bearing_y >> advance_x >> advance_y >> sdf_len;
		QByteArray raw(sdf_len, 0);
		dataStream.readRawData(raw.data(),sdf_len);
		this->sdf = raw;
	}
}

/*
	unsigned internalProcessSize;
	unsigned padding;
	unsigned samples_to_check_x;
	unsigned samples_to_check_y;
*/

void SDFGenerationArguments::fromArgs(const QVariantMap& args)
{
	this->internalProcessSize = args.value(INTERNAL_PROCESS_SIZE_KEY, INTERNAL_RENDER_SIZE).toUInt();
	this->intendedSize = args.value(INTENDED_SIZE_KEY, 0).toUInt();
	this->padding = args.value(PADDING_KEY, PADDING).toUInt();
	this->samples_to_check_x = args.value(SAMPLES_TO_CHECK_X_KEY, 0).toUInt();
	this->samples_to_check_y = args.value(SAMPLES_TO_CHECK_Y_KEY, 0).toUInt();
	this->char_min = args.value(CHAR_MIN_KEY, 0).toUInt();
	this->char_max = args.value(CHAR_MAX_KEY, 0xE007F).toUInt();
	this->font_path = args.value(IN_FONT_KEY, DEFAULT_FONT_PATH).toString();
	// Mode
	{
		QVariant genMod = args.value(MODE_KEY,SOFTWARE_MODE_KEY);
		switch (genMod.typeId() ) {
			case QMetaType::QString: {
				QString genModS = genMod.toString();
				if(!genModS.compare(SOFTWARE_MODE_KEY,Qt::CaseInsensitive)) this->mode = SDfGenerationMode::SOFTWARE;
				else if(!genModS.compare(OPENGL_MODE_KEY,Qt::CaseInsensitive)) this->mode = SDfGenerationMode::OPENGL_COMPUTE;
				else if(!genModS.compare(OPENCL_MODE_KEY,Qt::CaseInsensitive)) this->mode = SDfGenerationMode::OPENCL;
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
		QVariant sdfType = args.value(TYPE_KEY,SDF_MODE_KEY);
		switch (sdfType.typeId() ) {
			case QMetaType::QString: {
				QString sdfTypeS = sdfType.toString();
				if(!sdfTypeS.compare(SDF_MODE_KEY,Qt::CaseInsensitive)) this->type = SDFType::SDF;
				else if(!sdfTypeS.compare(MSDFA_MODE_KEY,Qt::CaseInsensitive)) this->type = SDFType::MSDFA;
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
		QVariant distType = args.value(DIST_KEY,MANHATTAN_MODE_KEY);
		switch (distType.typeId() ) {
			case QMetaType::QString: {
				QString distTypeS = distType.toString();
				if(!distTypeS.compare(MANHATTAN_MODE_KEY,Qt::CaseInsensitive)) this->distType = DistanceType::Manhattan;
				else if(!distTypeS.compare(EUCLIDEAN_MODE_KEY,Qt::CaseInsensitive)) this->distType = DistanceType::Euclidean;
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

QDataStream& operator<<(QDataStream& stream, const StoredCharacter& storedCharacter) {
	storedCharacter.toData(stream);
	return stream;
}

QDataStream& operator>>(QDataStream& stream, StoredCharacter& storedCharacter) {
	storedCharacter.fromData(stream);
	return stream;
}

QDataStream& operator<<(QDataStream& stream, const PreprocessedFontFace& processedFontFace) {
	processedFontFace.toData(stream);
	return stream;
}

QDataStream& operator>>(QDataStream& stream, PreprocessedFontFace& processedFontFace) {
	processedFontFace.fromData(stream);
	return stream;
}
