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
#include <cstring>
#include <cstdlib>
#include <vector>
#include <cmath>
#include <QBitArray>
#include <QCborArray>

void PreprocessedFontFace::toCbor(QCborMap& cbor) const
{
	cbor.insert(FONT_NAME_KEY,fontFamilyName);
	cbor.insert(TYPE_KEY, static_cast<unsigned>(type));
	cbor.insert(DIST_KEY, static_cast<unsigned>(distType));
	cbor.insert(BITMAP_SIZE_KEY, bitmap_size);
	cbor.insert(BITMAP_LOGICAL_SIZE_KEY, bitmap_logical_size);
	cbor.insert(PADDING_KEY, bitmap_padding);
	cbor.insert(HAS_VERT_KEY, hasVert);
	cbor.insert(JPEG_KEY, jpeg);
	QCborMap tmpMap;
	for(auto it = std::begin(storedCharacters); it != std::end(storedCharacters); ++it) {
		tmpMap.insert(it.key(),it.value().toCbor());
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
	this->fontFamilyName = cbor[FONT_NAME_KEY].toString();
	this->type = static_cast<SDFType>(cbor[TYPE_KEY].toInteger());
	this->distType = static_cast<DistanceType>(cbor[DIST_KEY].toInteger());
	this->bitmap_size = cbor[BITMAP_SIZE_KEY].toInteger();
	this->bitmap_logical_size = cbor[BITMAP_LOGICAL_SIZE_KEY].toInteger();
	this->bitmap_padding = cbor[PADDING_KEY].toInteger();
	this->hasVert = cbor[HAS_VERT_KEY].toBool();
	this->jpeg = cbor[JPEG_KEY].toBool();
	QCborMap tmpMap = cbor[GLYPHS_KEY].toMap();
	for(auto it = std::begin(tmpMap); it != std::end(tmpMap); ++it) {
		unsigned charCode = it.key().toInteger();
		StoredCharacter storedChar;
		storedChar.fromCbor(it.value().toMap());
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
}

void PreprocessedFontFace::toData(QDataStream& dataStream) const
{
	QByteArray utf8str = this->fontFamilyName.toUtf8();
	dataStream << static_cast<uint32_t>(utf8str.size());
	dataStream.writeRawData(utf8str.data(),utf8str.length());
	dataStream << static_cast<uint8_t>(type) << static_cast<uint8_t>(distType) << bitmap_size << bitmap_logical_size << bitmap_padding << hasVert
			   << jpeg << static_cast<uint32_t>(storedCharacters.size());
	// Write table of contents
	QMap<uint32_t,uint32_t> offsets;
	auto currOffset = dataStream.device()->pos();
	for(qsizetype i = 0; i < storedCharacters.size(); ++i) {
		dataStream << uint32_t(0) << uint32_t(0);
	}
	dataStream << kerning;
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
	{
		uint32_t familyNameSize;
		dataStream >> familyNameSize;
		QByteArray familyNameUtf8(familyNameSize, 0);
		dataStream.readRawData(familyNameUtf8.data(),familyNameSize);
		this->fontFamilyName = QString::fromUtf8(familyNameUtf8);
	}
	uint8_t tmpType, tmpDist;
	uint32_t charCount;
	dataStream >> tmpType >> tmpDist >> bitmap_size >> bitmap_logical_size >> bitmap_padding >> hasVert >> jpeg >> charCount;
	this->type = static_cast<SDFType>(tmpType);
	this->distType = static_cast<DistanceType>(tmpDist);
	QMap<uint32_t,uint32_t> offsets;
	for(uint32_t i = 0; i < charCount; ++i) {
		uint32_t k,v;
		dataStream >> k >> v;
		offsets.insert(k,v);
	}
	dataStream >> kerning;
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
