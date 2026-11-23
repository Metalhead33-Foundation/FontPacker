/**
 * @file PreprocessedFontFace.cpp
 * @brief Implementation of PreprocessedFontFace serialization and deserialization.
 * 
 * Implements conversion between PreprocessedFontFace structures and:
 * - CBOR format (for JSON-like serialization)
 * - Binary QDataStream format (see BINARY_FORMAT.md)
 * 
 * Also implements kerning map serialization operators.
 */

#include "PreprocessedFontFace.hpp"
#include "StoredCharacter.hpp"
#include "ConstStrings.hpp"
#include <harfbuzz/hb-ft.h>
#include <QTextStream>
#include <ft2build.h>
#include <cstdint>
#include FT_FREETYPE_H
#include <QImage>
#include <QFile>
#include <QBuffer>
#include <algorithm>
#include <cstring>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <stdexcept>
#include <QBitArray>
#include <QCborArray>
#include "QDataStreamHelper.hpp"

namespace {

void validatePreprocessedFontFaceVersion(uint32_t version)
{
	if(version == 0 || version > PreprocessedFontFace::CURRENT_VERSION) {
		throw std::runtime_error("Unsupported PreprocessedFontFace format version.");
	}
}

void validatePreprocessedFontFaceMagic(QDataStream& dataStream)
{
	std::array<char,PreprocessedFontFace::BINARY_MAGIC.size()> magic{};
	if(dataStream.readRawData(magic.data(), magic.size()) != static_cast<int>(magic.size()) || magic != PreprocessedFontFace::BINARY_MAGIC) {
		throw std::runtime_error("Invalid PreprocessedFontFace binary magic.");
	}
}

void validatePreprocessedFontFaceCborType(const QCborMap& cbor)
{
	if(cbor.contains(CBOR_CONTAINER_TYPE_KEY) && cbor[CBOR_CONTAINER_TYPE_KEY].toString() != CBOR_PREPROCESSED_FONT_FACE_TYPE) {
		throw std::runtime_error("Invalid PreprocessedFontFace CBOR container type.");
	}
}

}

void PreprocessedFontFace::toCbor(QCborMap& cbor) const
{
	cbor.insert(CBOR_CONTAINER_TYPE_KEY, CBOR_PREPROCESSED_FONT_FACE_TYPE);
	cbor.insert(VERSION_KEY, version);
	cbor.insert(FONT_NAME_KEY,fontFamilyName);
	cbor.insert(TYPE_KEY, static_cast<unsigned>(type));
	cbor.insert(DIST_KEY, static_cast<unsigned>(distType));
	cbor.insert(BITMAP_SIZE_KEY, bitmap_size ? bitmap_size : bitmap_logical_size);
	cbor.insert(BITMAP_LOGICAL_SIZE_KEY, bitmap_logical_size);
	cbor.insert(PADDING_KEY, bitmap_padding);
	cbor.insert(HAS_VERT_KEY, hasVert);
	cbor.insert(IMAGE_FORMAT_KEY, QString::fromLatin1(imageFormatBytes()));
	cbor.insert(ASCENDER_KEY, ascender);
	cbor.insert(DESCENDER_KEY, descender);
	cbor.insert(FACE_HEIGHT_KEY, faceHeight);
	cbor.insert(MAX_ADVANCE_KEY, maxAdvance);
	cbor.insert(UNITS_PER_EM_KEY, unitsPerEm);
	if(version >= 2) {
		cbor.insert(DISTANCE_RANGE_X_KEY, distanceRangeX);
		cbor.insert(DISTANCE_RANGE_Y_KEY, distanceRangeY);
		cbor.insert(ENCODING_FLAGS_KEY, encodingFlags);
		cbor.insert(CBOR_MIDPOINT_ADJUSTMENT_KEY, midpointAdjustment);
	}
	const bool legacySvgAdvances = (version == 1 && fontFamilyName == QStringLiteral("SVG"));
	QCborMap tmpMap;
	for(auto it = std::begin(storedCharacters); it != std::end(storedCharacters); ++it) {
		QCborMap glyphMap;
		it.value().toCbor(glyphMap, version, legacySvgAdvances);
		tmpMap.insert(it.key(),glyphMap);
	}
	cbor.insert(GLYPHS_KEY, tmpMap);
	QCborMap kerningMap;
	for(auto it = std::begin(kerning) ; it != std::end(kerning) ; ++it ) {
		QCborMap kerningMap2;
		for(auto zt = std::begin(it.value()) ; zt != std::end(it.value()) ; ++zt ) {
			QCborArray arr;
			arr.push_back(zt.value().first);
			arr.push_back(zt.value().second);
			kerningMap2.insert(zt.key(),arr);
		}
		kerningMap.insert(it.key(),kerningMap2);
	}
	cbor.insert(KERNING_KEY, kerningMap);
}

QCborMap PreprocessedFontFace::toCbor() const
{
	QCborMap tmp;
	toCbor(tmp);
	return tmp;
}

void PreprocessedFontFace::fromCbor(const QCborMap& cbor)
{
	validatePreprocessedFontFaceCborType(cbor);
	if(cbor.contains(VERSION_KEY)) {
		this->version = static_cast<uint32_t>(cbor[VERSION_KEY].toInteger(1));
	} else {
		// Old CBOR files had no version; the distance-range key identifies v2.
		this->version = cbor.contains(DISTANCE_RANGE_X_KEY) ? 2u : 1u;
	}
	validatePreprocessedFontFaceVersion(version);
	this->fontFamilyName = cbor[FONT_NAME_KEY].toString();
	this->type = static_cast<SDFType>(cbor[TYPE_KEY].toInteger());
	this->distType = static_cast<DistanceType>(cbor[DIST_KEY].toInteger());
	this->bitmap_size = cbor[BITMAP_SIZE_KEY].toInteger();
	this->bitmap_logical_size = cbor[BITMAP_LOGICAL_SIZE_KEY].toInteger();
	this->bitmap_padding = cbor[PADDING_KEY].toInteger();
	this->hasVert = cbor[HAS_VERT_KEY].toBool();
	setImageFormat(cbor[IMAGE_FORMAT_KEY].toString(QStringLiteral("PNG")).toLatin1());
	this->ascender = cbor[ASCENDER_KEY].toDouble();
	this->descender = cbor[DESCENDER_KEY].toDouble();
	this->faceHeight = cbor[FACE_HEIGHT_KEY].toDouble();
	this->maxAdvance = cbor[MAX_ADVANCE_KEY].toDouble();
	this->unitsPerEm = cbor[UNITS_PER_EM_KEY].toInteger();
	this->distanceRangeX = static_cast<float>(cbor[DISTANCE_RANGE_X_KEY].toDouble(0.0));
	this->distanceRangeY = static_cast<float>(cbor[DISTANCE_RANGE_Y_KEY].toDouble(0.0));
	this->encodingFlags = static_cast<uint32_t>(cbor[ENCODING_FLAGS_KEY].toInteger(0));
	this->midpointAdjustment = static_cast<float>(cbor[CBOR_MIDPOINT_ADJUSTMENT_KEY].toDouble(1.0));
	QCborMap tmpMap = cbor[GLYPHS_KEY].toMap();
	const bool legacySvgAdvances = (version == 1 && fontFamilyName == QStringLiteral("SVG"));
	for(auto it = std::begin(tmpMap); it != std::end(tmpMap); ++it) {
		unsigned charCode = it.key().toInteger();
		StoredCharacter storedChar;
		storedChar.fromCbor(it.value().toMap(), version, legacySvgAdvances);
		storedCharacters.insert(charCode, storedChar);
	}
	QCborMap kerningMap = cbor[KERNING_KEY].toMap();
	for(auto it = std::begin(kerningMap) ; it != std::end(kerningMap) ; ++it ) {
		QCborMap kerningMap2 = it.value().toMap();
		PerCharacterKerning perCharKern;
		for(auto zt = std::begin(kerningMap2) ; zt != std::end(kerningMap2) ; ++zt ) {
			QCborArray arr = zt->toArray();
			Vec2f tmpVec = { arr[0].toDouble(), arr[1].toDouble() };
			perCharKern.insert(zt.key().toInteger(),tmpVec);
		}
		kerning.insert(it.key().toInteger(),perCharKern);
	}
	if(!cbor.contains(DISTANCE_RANGE_X_KEY) && bitmap_logical_size) {
		// Old CBOR files may omit both the fields and an explicit version.
		const float storedSize = static_cast<float>(bitmap_size ? bitmap_size : bitmap_logical_size);
		distanceRangeX = static_cast<float>(bitmap_padding) * storedSize / static_cast<float>(bitmap_logical_size);
		distanceRangeY = distanceRangeX;
	}
	if(version < 2 && bitmap_logical_size) {
		// Version 1 did not store ranges; reconstruct the nominal padding-based value.
		const float storedSize = static_cast<float>(bitmap_size ? bitmap_size : bitmap_logical_size);
		distanceRangeX = static_cast<float>(bitmap_padding) * storedSize / static_cast<float>(bitmap_logical_size);
		distanceRangeY = distanceRangeX;
	}
}

void PreprocessedFontFace::toData(QDataStream& dataStream) const
{
	dataStream.writeRawData(BINARY_MAGIC.data(), BINARY_MAGIC.size());
	dataStream << version;
	saveToDatastreamAsUtf8(dataStream, this->fontFamilyName);
	dataStream << static_cast<uint8_t>(type) << static_cast<uint8_t>(distType) << (bitmap_size ? bitmap_size : bitmap_logical_size) << bitmap_logical_size << bitmap_padding << hasVert;
	dataStream.writeRawData(imageFormat.data(), imageFormat.size());
	dataStream << ascender << descender << faceHeight << maxAdvance << unitsPerEm;
	if(version >= 2) {
		dataStream << distanceRangeX << distanceRangeY << encodingFlags << midpointAdjustment;
	}
	dataStream << static_cast<uint32_t>(storedCharacters.size());
	// Write table of contents
	const bool legacySvgAdvances = (version == 1 && fontFamilyName == QStringLiteral("SVG"));
	QMap<uint32_t,uint32_t> offsets;
	auto currOffset = dataStream.device()->pos();
	for(qsizetype i = 0; i < storedCharacters.size(); ++i) {
		dataStream << uint32_t(0) << uint32_t(0);
	}
	dataStream << kerning;
	for(auto it = std::begin(storedCharacters); it != std::end(storedCharacters); ++it) {
		auto glyphOffset = dataStream.device()->pos();
		offsets.insert(it.key(), glyphOffset);
		it->toData(dataStream, version, legacySvgAdvances);
	}
	dataStream.device()->seek(currOffset);
	for(auto it = std::begin(offsets); it != std::end(offsets); ++it) {
		dataStream << it.key() << it.value();
	}
}

void PreprocessedFontFace::fromData(QDataStream& dataStream)
{
	{
		validatePreprocessedFontFaceMagic(dataStream);
		dataStream >> version;
		validatePreprocessedFontFaceVersion(version);
		this->fontFamilyName = loadFromDatastreamAsUtf8(dataStream);
	}
	uint8_t tmpType, tmpDist;
	uint32_t charCount;
	dataStream >> tmpType >> tmpDist >> bitmap_size >> bitmap_logical_size >> bitmap_padding >> hasVert;
	dataStream.readRawData(imageFormat.data(), imageFormat.size());
	imageFormat.back() = '\0';
	dataStream >> ascender >> descender >> faceHeight >> maxAdvance >> unitsPerEm;
	if(version >= 2) {
		dataStream >> distanceRangeX >> distanceRangeY >> encodingFlags >> midpointAdjustment;
	}
	dataStream >> charCount;
	this->type = static_cast<SDFType>(tmpType);
	this->distType = static_cast<DistanceType>(tmpDist);
	QMap<uint32_t,uint32_t> offsets;
	const bool legacySvgAdvances = (version == 1 && fontFamilyName == QStringLiteral("SVG"));
	for(uint32_t i = 0; i < charCount; ++i) {
		uint32_t k,v;
		dataStream >> k >> v;
		offsets.insert(k,v);
	}
	dataStream >> kerning;
	for(auto it = std::begin(offsets); it != std::end(offsets); ++it) {
		dataStream.device()->seek(it.value());
		StoredCharacter tmpChar;
		tmpChar.fromData(dataStream, version, legacySvgAdvances);
		storedCharacters.insert(it.key(),tmpChar);
	}
	if(version < 2 && bitmap_logical_size) {
		// Version 1 did not store ranges; reconstruct the nominal padding-based value.
		const float storedSize = static_cast<float>(bitmap_size ? bitmap_size : bitmap_logical_size);
		distanceRangeX = static_cast<float>(bitmap_padding) * storedSize / static_cast<float>(bitmap_logical_size);
		distanceRangeY = distanceRangeX;
	}
}

void PreprocessedFontFace::setImageFormat(const QByteArray& format)
{
	imageFormat.fill('\0');
	QByteArray normalized = format.trimmed().toUpper();
	if(normalized.isEmpty()) normalized = QByteArrayLiteral("PNG");
	const qsizetype bytesToCopy = std::min<qsizetype>(normalized.size(), static_cast<qsizetype>(imageFormat.size() - 1));
	std::copy_n(normalized.constData(), bytesToCopy, imageFormat.data());
}

QByteArray PreprocessedFontFace::imageFormatBytes() const
{
	const auto terminator = std::find(imageFormat.begin(), imageFormat.end(), '\0');
	return QByteArray(imageFormat.data(), std::distance(imageFormat.begin(), terminator));
}

void PreprocessedFontFace::repack(const QByteArray& newImageFormat, uint32_t newIntendedSize)
{
	QByteArray codec = newImageFormat.trimmed().toUpper();
	if(codec.isEmpty()) codec = imageFormatBytes();
	if(codec.isEmpty()) codec = QByteArrayLiteral("PNG");
	if(codec.size() > 7) codec = codec.left(7);

	const uint32_t oldStoredSize = bitmap_size ? bitmap_size : bitmap_logical_size;
	const uint32_t targetStoredSize = newIntendedSize ? newIntendedSize : oldStoredSize;
	if(targetStoredSize == 0) {
		throw std::runtime_error("Cannot repack a font face with an unknown stored size.");
	}

	for(auto it = std::begin(storedCharacters); it != std::end(storedCharacters); ++it) {
		if(it.value().sdf.isEmpty()) continue;
		QImage img = QImage::fromData(it.value().sdf);
		if(img.isNull() || img.width() != img.height()) {
			throw std::runtime_error(QStringLiteral("Failed to decode the SDF image of code point %1.").arg(it.key()).toStdString());
		}
		if(newIntendedSize && img.width() != static_cast<int>(newIntendedSize)) {
			img = img.scaled(static_cast<int>(newIntendedSize), static_cast<int>(newIntendedSize),
							 Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
		}
		QByteArray bytes;
		QBuffer buff(&bytes);
		buff.open(QIODevice::WriteOnly);
		if(!img.save(&buff, codec.constData(), -1)) {
			throw std::runtime_error(QStringLiteral("Failed to encode the SDF image of code point %1 as %2.")
									 .arg(it.key()).arg(QString::fromLatin1(codec)).toStdString());
		}
		buff.close();
		it.value().sdf = bytes;
	}

	setImageFormat(codec);
	if(newIntendedSize && oldStoredSize && newIntendedSize != oldStoredSize) {
		const float ratio = static_cast<float>(newIntendedSize) / static_cast<float>(oldStoredSize);
		distanceRangeX *= ratio;
		distanceRangeY *= ratio;
	}
	bitmap_size = targetStoredSize;
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


QDataStream &operator<<(QDataStream &stream, const PerCharacterKerning &processedFontFace) {
	stream << static_cast<uint32_t>(processedFontFace.size());
	for( auto it = std::begin(processedFontFace) ; it != std::end(processedFontFace) ; ++it ) {
		stream << it.key() << it.value();
	}
	return stream;
}
QDataStream &operator>>(QDataStream &stream, PerCharacterKerning &processedFontFace) {
	uint32_t rawSize;
	stream >> rawSize;
	for(uint32_t i = 0; i < rawSize; ++i) {
		uint32_t k;
		Vec2f v;
		stream >> k >> v;
		processedFontFace.insert(k,v);
	}
	return stream;
}
QDataStream &operator<<(QDataStream &stream, const KerningMap &processedFontFace) {
	stream << static_cast<uint32_t>(processedFontFace.size());
	for( auto it = std::begin(processedFontFace) ; it != std::end(processedFontFace) ; ++it ) {
		stream << it.key() << it.value();
	}
	return stream;
}
QDataStream &operator>>(QDataStream &stream, KerningMap &processedFontFace) {
	uint32_t rawSize;
	stream >> rawSize;
	for(uint32_t i = 0; i < rawSize; ++i) {
		uint32_t k;
		PerCharacterKerning v;
		stream >> k >> v;
		processedFontFace.insert(k,v);
	}
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
