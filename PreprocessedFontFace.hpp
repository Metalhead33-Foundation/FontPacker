#ifndef PREPROCESSEDFONTFACE_HPP
#define PREPROCESSEDFONTFACE_HPP
#include <cstdint>
#include <QString>
#include <QCborMap>
#include "SDFGenerationArguments.hpp"
#include "StoredCharacter.hpp"

typedef std::pair<float,float> Vec2f;
typedef QMap<uint32_t,Vec2f> PerCharacterKerning;
typedef QMap<uint32_t,PerCharacterKerning> KerningMap;
QDataStream &operator<<(QDataStream &stream, const PerCharacterKerning &processedFontFace);
QDataStream &operator>>(QDataStream &stream, PerCharacterKerning &processedFontFace);
QDataStream &operator<<(QDataStream &stream, const KerningMap &processedFontFace);
QDataStream &operator>>(QDataStream &stream, KerningMap &processedFontFace);

struct PreprocessedFontFace {
	QString fontFamilyName;
	SDFType type;
	DistanceType distType;
	uint32_t bitmap_size;
	uint32_t bitmap_logical_size;
	uint32_t bitmap_padding;
	bool hasVert;
	bool jpeg;
	KerningMap kerning;
	QMap<uint32_t,StoredCharacter> storedCharacters;
	void toCbor(QCborMap& cbor) const;
	QCborMap toCbor(void) const;
	void fromCbor(const QCborMap& cbor);
	void toData(QDataStream& dataStream) const;
	void fromData(QDataStream& dataStream);
	void outToFolder(const QString& pattern) const;
};
QDataStream &operator<<(QDataStream &stream, const PreprocessedFontFace &processedFontFace);
QDataStream &operator>>(QDataStream &stream, PreprocessedFontFace &processedFontFace);

#endif // PREPROCESSEDFONTFACE_HPP
