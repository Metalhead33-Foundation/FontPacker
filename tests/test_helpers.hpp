#ifndef FONTPACKER_TEST_HELPERS_HPP
#define FONTPACKER_TEST_HELPERS_HPP

#include "PreprocessedFontFace.hpp"
#include "StoredCharacter.hpp"
#include "StoredVectorImage.hpp"

#include <QBuffer>
#include <QByteArray>
#include <QDataStream>

namespace test {

inline StoredCharacter makeGlyph(float advanceX = 12.75f, float advanceY = 0.25f)
{
	StoredCharacter glyph{};
	glyph.valid = true;
	glyph.width = 10;
	glyph.height = 12;
	glyph.bearing_x = -2;
	glyph.bearing_y = 9;
	glyph.advance_x = advanceX;
	glyph.advance_y = advanceY;
	glyph.metricWidth = 10.0f;
	glyph.metricHeight = 12.0f;
	glyph.horiBearingX = 0.5f;
	glyph.horiBearingY = 12.0f;
	glyph.horiAdvance = advanceX;
	glyph.vertBearingX = 1.0f;
	glyph.vertBearingY = 2.0f;
	glyph.vertAdvance = 13.0f;
	glyph.sdf = QByteArray::fromHex("010203");
	return glyph;
}

inline PreprocessedFontFace makeFace(uint32_t version = 3, const QString& family = QStringLiteral("Test"))
{
	PreprocessedFontFace face;
	face.version = version;
	face.fontFamilyName = family;
	face.type = SDF;
	face.distType = Euclidean;
	face.bitmap_size = 16;
	face.bitmap_logical_size = 64;
	face.bitmap_padding = 8;
	face.hasVert = false;
	face.setImageFormat(QByteArrayLiteral("PNG"));
	face.ascender = 52.0f;
	face.descender = -14.0f;
	face.faceHeight = 65.0f;
	face.maxAdvance = 105.0f;
	face.unitsPerEm = 2048;
	face.distanceRangeX = 2.5f;
	face.distanceRangeY = 2.0f;
	face.encodingFlags = StoredVectorImage::EncodingInverted | StoredVectorImage::EncodingGammaCorrected;
	face.midpointAdjustment = 1.25f;
	face.storedCharacters.insert(0x41, makeGlyph());
	PerCharacterKerning pairs;
	pairs.insert(0x42, {1.5f, -0.25f});
	face.kerning.insert(0x41, pairs);
	return face;
}

inline QByteArray fontToBinary(const PreprocessedFontFace& face)
{
	QByteArray bytes;
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::WriteOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);
	face.toData(stream);
	buffer.close();
	return bytes;
}

inline PreprocessedFontFace fontFromBinary(QByteArray bytes)
{
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::ReadOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);
	PreprocessedFontFace face;
	face.fromData(stream);
	return face;
}

inline QByteArray glyphToBinary(const StoredCharacter& glyph, uint32_t version)
{
	QByteArray bytes;
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::WriteOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);
	glyph.toData(stream, version);
	buffer.close();
	return bytes;
}

inline StoredCharacter glyphFromBinary(QByteArray bytes, uint32_t version)
{
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::ReadOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);
	StoredCharacter glyph;
	glyph.fromData(stream, version);
	return glyph;
}

inline QByteArray vectorToBinary(const StoredVectorImage& image)
{
	QByteArray bytes;
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::WriteOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);
	image.toData(stream);
	buffer.close();
	return bytes;
}

inline StoredVectorImage vectorFromBinary(QByteArray bytes)
{
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::ReadOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);
	StoredVectorImage image;
	image.fromData(stream);
	return image;
}

}

#endif
