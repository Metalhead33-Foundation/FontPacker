#include <catch2/catch_approx.hpp>
#include <catch2/catch_test_macros.hpp>

#include "ConstStrings.hpp"
#include "StoredCharacter.hpp"
#include "StoredVectorImage.hpp"
#include "test_helpers.hpp"

#include <QCborMap>
#include <QCborValue>
#include <QImage>
#include <QtEndian>

using Catch::Approx;

TEST_CASE("StoredCharacter v3 stores float advances in 61-byte records", "[binary][v3]")
{
	const StoredCharacter glyph = test::makeGlyph(12.75f, 0.25f);
	const QByteArray bytes = test::glyphToBinary(glyph, 3);

	REQUIRE(bytes.size() == 61 + glyph.sdf.size());
	const StoredCharacter roundTripped = test::glyphFromBinary(bytes, 3);
	CHECK(roundTripped.valid);
	CHECK(roundTripped.width == glyph.width);
	CHECK(roundTripped.height == glyph.height);
	CHECK(roundTripped.advance_x == Approx(12.75f));
	CHECK(roundTripped.advance_y == Approx(0.25f));
	CHECK(roundTripped.metricWidth == Approx(glyph.metricWidth));
	CHECK(roundTripped.sdf == glyph.sdf);
}

TEST_CASE("StoredCharacter v2 writes raw 26.6 advances but reads float pixels", "[binary][v2][compat]")
{
	const StoredCharacter glyph = test::makeGlyph(12.75f, 0.25f);
	const QByteArray bytes = test::glyphToBinary(glyph, 2);

	REQUIRE(bytes.size() == 61 + glyph.sdf.size());
	const quint32 rawX = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData() + 17));
	const quint32 rawY = qFromBigEndian<quint32>(reinterpret_cast<const uchar*>(bytes.constData() + 21));
	CHECK(rawX == 816); // 12.75 * 64
	CHECK(rawY == 16);  // 0.25 * 64

	const StoredCharacter roundTripped = test::glyphFromBinary(bytes, 2);
	CHECK(roundTripped.advance_x == Approx(12.75f));
	CHECK(roundTripped.advance_y == Approx(0.25f));
}

TEST_CASE("Invalid stored characters occupy exactly one byte", "[binary]")
{
	StoredCharacter invalid{};
	invalid.valid = false;
	const QByteArray bytes = test::glyphToBinary(invalid, 3);
	CHECK(bytes.size() == 1);

	const StoredCharacter roundTripped = test::glyphFromBinary(bytes, 3);
	CHECK_FALSE(roundTripped.valid);
}

TEST_CASE("PreprocessedFontFace v3 binary round-trip preserves header and glyphs", "[binary][v3]")
{
	const PreprocessedFontFace face = test::makeFace(3);
	const QByteArray bytes = test::fontToBinary(face);
	const PreprocessedFontFace out = test::fontFromBinary(bytes);

	CHECK(out.version == 3);
	CHECK(out.fontFamilyName == QStringLiteral("Test"));
	CHECK(out.bitmap_size == 16u);
	CHECK(out.bitmap_logical_size == 64u);
	CHECK(out.bitmap_padding == 8u);
	CHECK(out.distanceRangeX == Approx(2.5f));
	CHECK(out.distanceRangeY == Approx(2.0f));
	CHECK(out.encodingFlags == face.encodingFlags);
	CHECK(out.midpointAdjustment == Approx(1.25f));
	CHECK(out.imageFormatBytes() == QByteArrayLiteral("PNG"));
	REQUIRE(out.storedCharacters.size() == 1);
	CHECK(out.storedCharacters.value(0x41).advance_x == Approx(12.75f));
	CHECK(out.storedCharacters.value(0x41).sdf == QByteArray::fromHex("010203"));
	REQUIRE(out.kerning.contains(0x41));
	CHECK(out.kerning.value(0x41).value(0x42).first == Approx(1.5f));
	CHECK(out.kerning.value(0x41).value(0x42).second == Approx(-0.25f));
}

TEST_CASE("PreprocessedFontFace v2 round-trip keeps version and advance values", "[binary][v2][compat]")
{
	const PreprocessedFontFace face = test::makeFace(2);
	const PreprocessedFontFace out = test::fontFromBinary(test::fontToBinary(face));

	CHECK(out.version == 2);
	CHECK(out.storedCharacters.value(0x41).advance_x == Approx(12.75f));
	CHECK(out.storedCharacters.value(0x41).advance_y == Approx(0.25f));
}

TEST_CASE("Version 1 font faces keep raw 26.6 advances", "[binary][v1][compat]")
{
	PreprocessedFontFace face = test::makeFace(1, QStringLiteral("LegacyFont"));
	face.storedCharacters[0x41].advance_x = 10.5f;
	const PreprocessedFontFace out = test::fontFromBinary(test::fontToBinary(face));

	CHECK(out.version == 1);
	CHECK(out.storedCharacters.value(0x41).advance_x == Approx(10.5f));
}

TEST_CASE("Version 1 SVG faces use plain-pixel advances", "[binary][v1][svg][compat]")
{
	PreprocessedFontFace face = test::makeFace(1, QStringLiteral("SVG"));
	face.storedCharacters[0x41].advance_x = 100.0f;
	face.storedCharacters[0x41].advance_y = 50.0f;
	const PreprocessedFontFace out = test::fontFromBinary(test::fontToBinary(face));

	CHECK(out.version == 1);
	CHECK(out.storedCharacters.value(0x41).advance_x == Approx(100.0f));
	CHECK(out.storedCharacters.value(0x41).advance_y == Approx(50.0f));
}

TEST_CASE("Writing a zero bitmap_size resolves it to the logical size", "[binary][v3]")
{
	PreprocessedFontFace face = test::makeFace(3);
	face.bitmap_size = 0;
	face.bitmap_logical_size = 64;
	const PreprocessedFontFace out = test::fontFromBinary(test::fontToBinary(face));
	CHECK(out.bitmap_size == 64u);
}

TEST_CASE("PreprocessedFontFace CBOR v3 uses double advances and v2 uses integers", "[cbor][v3][v2]")
{
	PreprocessedFontFace v3 = test::makeFace(3);
	const QCborMap v3Map = v3.toCbor();
	const QCborMap v3Glyphs = v3Map.value(GLYPHS_KEY).toMap();
	const QCborMap v3Glyph = v3Glyphs.value(0x41).toMap();
	CHECK(v3Glyph.value(ADVANCE_X_KEY).isDouble());

	PreprocessedFontFace v3Out;
	v3Out.fromCbor(v3Map);
	CHECK(v3Out.version == 3);
	CHECK(v3Out.storedCharacters.value(0x41).advance_x == Approx(12.75f));

	PreprocessedFontFace v2 = test::makeFace(2);
	const QCborMap v2Map = v2.toCbor();
	const QCborMap v2Glyph = v2Map.value(GLYPHS_KEY).toMap().value(0x41).toMap();
	CHECK(v2Glyph.value(ADVANCE_X_KEY).isInteger());

	PreprocessedFontFace v2Out;
	v2Out.fromCbor(v2Map);
	CHECK(v2Out.version == 2);
	CHECK(v2Out.storedCharacters.value(0x41).advance_x == Approx(12.75f));
}

TEST_CASE("StoredVectorImage binary and CBOR round-trips preserve metadata and mipmaps", "[binary][cbor][wodi]")
{
	StoredVectorImage image;
	image.version = 1;
	image.processingSize = 64;
	image.actualSize = 16;
	image.padding = 8;
	image.logicalX = 1.0f;
	image.logicalY = 2.0f;
	image.logicalWidth = 100.0f;
	image.logicalHeight = 50.0f;
	image.aspectRatio = 2.0f;
	image.minX = 10.0f;
	image.maxX = 90.0f;
	image.minY = 10.0f;
	image.maxY = 40.0f;
	image.distanceRangeX = 2.0f;
	image.distanceRangeY = 2.0f;
	image.encodingFlags = StoredVectorImage::EncodingInverted;
	image.midpointAdjustment = 1.5f;
	image.type = MSDF;
	image.distType = Manhattan;
	image.setImageFormat(QByteArrayLiteral("PNG"));
	image.mipmaps << QByteArray::fromHex("aabb") << QByteArray::fromHex("cc");

	const StoredVectorImage binaryOut = test::vectorFromBinary(test::vectorToBinary(image));
	CHECK(binaryOut.version == 1);
	CHECK(binaryOut.processingSize == 64u);
	CHECK(binaryOut.actualSize == 16u);
	CHECK(binaryOut.logicalWidth == Approx(100.0f));
	CHECK(binaryOut.distanceRangeX == Approx(2.0f));
	CHECK(binaryOut.encodingFlags == StoredVectorImage::EncodingInverted);
	CHECK(binaryOut.mipmaps.size() == 2);
	CHECK(binaryOut.mipmaps.at(0) == image.mipmaps.at(0));

	StoredVectorImage cborOut;
	cborOut.fromCbor(image.toCbor());
	CHECK(cborOut.actualSize == 16u);
	CHECK(cborOut.logicalHeight == Approx(50.0f));
	CHECK(cborOut.mipmaps.size() == 2);
	CHECK(cborOut.mipmaps.at(1) == image.mipmaps.at(1));
}

TEST_CASE("PreprocessedFontFace repack rescales glyph payloads in memory", "[repack]")
{
	PreprocessedFontFace face = test::makeFace(3);
	face.bitmap_logical_size = 8;
	face.bitmap_size = 8;
	face.bitmap_padding = 1;
	face.distanceRangeX = 1.0f;
	face.distanceRangeY = 1.0f;

	QImage image(8, 8, QImage::Format_Grayscale8);
	image.fill(128);
	QByteArray png;
	QBuffer buffer(&png);
	buffer.open(QIODevice::WriteOnly);
	REQUIRE(image.save(&buffer, "PNG"));
	buffer.close();
	face.storedCharacters[0x41].sdf = png;

	face.repack(QByteArrayLiteral("BMP"), 4);
	CHECK(face.version == 3);
	CHECK(face.bitmap_size == 4u);
	CHECK(face.imageFormatBytes() == QByteArrayLiteral("BMP"));
	CHECK(face.distanceRangeX == Approx(0.5f));
	CHECK(face.distanceRangeY == Approx(0.5f));

	const QImage rescaled = QImage::fromData(face.storedCharacters.value(0x41).sdf);
	REQUIRE_FALSE(rescaled.isNull());
	CHECK(rescaled.size() == QSize(4, 4));
}
