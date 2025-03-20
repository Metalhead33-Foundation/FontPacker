#ifndef STOREDCHARACTER_HPP
#define STOREDCHARACTER_HPP
#include <cstdint>
#include <QByteArray>
#include <QCborMap>
#include "SDFGenerationArguments.hpp"

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
	void toData(QDataStream& dataStream) const;
	void fromData(QDataStream& dataStream);
};

QDataStream &operator<<(QDataStream &stream, const StoredCharacter &storedCharacter);
QDataStream &operator>>(QDataStream &stream, StoredCharacter &storedCharacter);

#endif // STOREDCHARACTER_HPP
