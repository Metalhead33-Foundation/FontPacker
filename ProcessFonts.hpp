#ifndef PROCESSFONTS_HPP
#define PROCESSFONTS_HPP
#include <QVariant>
#include <QMap>
#include <QCborMap>
#include <QCborArray>
#include <QCborValue>
#include <QByteArray>
#include "GlHelpers.hpp"
#include <QOpenGLShaderProgram>
#include <QList>
#include <QDataStream>
#include <memory>

enum SDfGenerationMode {
	SOFTWARE,
	OPENGL_COMPUTE,
	OPENCL
};
enum SDFType {
	SDF,
	MSDFA
};
enum DistanceType {
	Manhattan,
	Euclidean
};

struct SDFGenerationArguments {
	SDfGenerationMode mode = SOFTWARE;
	SDFType type = SDF;
	DistanceType distType = Manhattan;
	uint32_t internalProcessSize;
	uint32_t intendedSize;
	uint32_t padding;
	uint32_t samples_to_check_x;
	uint32_t samples_to_check_y;
	QString font_path;
	uint32_t char_min;
	uint32_t char_max;
	std::unique_ptr<GlHelpers> glHelpers;
	std::unique_ptr<QOpenGLShaderProgram> glShader;
	void fromArgs(const QVariantMap& args);
};

typedef std::pair<float,float> Vec2f;
typedef QMap<uint32_t,Vec2f> PerCharacterKerning;
typedef QMap<uint32_t,PerCharacterKerning> KerningMap;
QDataStream &operator<<(QDataStream &stream, const PerCharacterKerning &processedFontFace);
QDataStream &operator>>(QDataStream &stream, PerCharacterKerning &processedFontFace);
QDataStream &operator<<(QDataStream &stream, const KerningMap &processedFontFace);
QDataStream &operator>>(QDataStream &stream, KerningMap &processedFontFace);

struct StoredCharacter;
struct PreprocessedFontFace {
	QString fontFamilyName;
	SDFType type;
	DistanceType distType;
	uint32_t bitmap_size;
	uint32_t bitmap_logical_size;
	uint32_t bitmap_padding;
	bool hasVert;
	KerningMap kerning;
	QMap<uint32_t,StoredCharacter> storedCharacters;
	void processFonts(const QVariantMap& args);
	void processFonts(SDFGenerationArguments& args);
	void toCbor(QCborMap& cbor) const;
	QCborMap toCbor(void) const;
	void fromCbor(const QCborMap& cbor);
	void toData(QDataStream& dataStream) const;
	void fromData(QDataStream& dataStream);
	void outToFolder(const QString& pattern) const;
};
QDataStream &operator<<(QDataStream &stream, const PreprocessedFontFace &processedFontFace);
QDataStream &operator>>(QDataStream &stream, PreprocessedFontFace &processedFontFace);

typedef struct FT_GlyphSlotRec_*  FT_GlyphSlot;
struct StoredCharacter {
	// Bitmap data
	bool valid;
	uint32_t width; // Intended width: actual size is 32x32 or whatever else set in globally in the font
	uint32_t height; // Intended height: actual size is 32x32 or whatever else set in globally in the font
	int32_t bearing_x;
	int32_t bearing_y;
	uint32_t advance_x;
	uint32_t advance_y;
	// Glyph metrics
	float metricWidth;
	float metricHeight;
	float horiBearingX;
	float horiBearingY;
	float horiAdvance;
	float vertBearingX;
	float vertBearingY;
	float vertAdvance;
	// The actual data
	QByteArray sdf;
	void toCbor(QCborMap& cbor) const;
	QCborMap toCbor(void) const;
	void fromCbor(const QCborMap& cbor);
	void fromFreeTypeGlyph(FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args = SDFGenerationArguments());
	void toData(QDataStream& dataStream) const;
	void fromData(QDataStream& dataStream);
};
QDataStream &operator<<(QDataStream &stream, const StoredCharacter &storedCharacter);
QDataStream &operator>>(QDataStream &stream, StoredCharacter &storedCharacter);


#endif // PROCESSFONTS_HPP
