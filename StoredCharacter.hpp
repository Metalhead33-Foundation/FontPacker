/**
 * @file StoredCharacter.hpp
 * @brief Structure for storing individual glyph data including SDF bitmap and metrics.
 */

#ifndef STOREDCHARACTER_HPP
#define STOREDCHARACTER_HPP
#include <cstdint>
#include <QByteArray>
#include <QCborMap>
#include "SDFGenerationArguments.hpp"

typedef struct FT_GlyphSlotRec_*  FT_GlyphSlot;

/**
 * @brief Structure storing data for a single glyph character.
 * 
 * Contains both bitmap dimensions and FreeType-style metrics, along with
 * the actual signed distance field (SDF) bitmap data.
 */
struct StoredCharacter {
	// Bitmap data
	bool valid;                    ///< Whether this glyph contains valid data
	uint32_t width;                ///< Intended width (actual SDF size is set globally in font)
	uint32_t height;               ///< Intended height (actual SDF size is set globally in font)
	int32_t bearing_x;             ///< X bearing (horizontal offset from origin, signed)
	int32_t bearing_y;             ///< Y bearing (vertical offset from baseline, signed)
	uint32_t advance_x;             ///< Horizontal advance width in pixels
	uint32_t advance_y;             ///< Vertical advance height in pixels
	
	// Glyph metrics (floating-point, more precise than integer values)
	float metricWidth;             ///< Metric width in font units
	float metricHeight;            ///< Metric height in font units
	float horiBearingX;            ///< Horizontal bearing X in font units
	float horiBearingY;            ///< Horizontal bearing Y in font units
	float horiAdvance;             ///< Horizontal advance in font units
	float vertBearingX;            ///< Vertical bearing X in font units
	float vertBearingY;            ///< Vertical bearing Y in font units
	float vertAdvance;             ///< Vertical advance in font units
	
	// The actual data
	QByteArray sdf;                ///< Signed distance field bitmap data (format depends on font type)
	
	/**
	 * @brief Serialize to CBOR format.
	 * @param cbor The CBOR map to populate with data.
	 */
	void toCbor(QCborMap& cbor) const;
	
	/**
	 * @brief Serialize to CBOR format.
	 * @return A new CBOR map containing the character data.
	 */
	QCborMap toCbor(void) const;
	
	/**
	 * @brief Deserialize from CBOR format.
	 * @param cbor The CBOR map to read data from.
	 */
	void fromCbor(const QCborMap& cbor);
	
	/**
	 * @brief Serialize to binary QDataStream format.
	 * @param dataStream The data stream to write to.
	 */
	void toData(QDataStream& dataStream) const;
	
	/**
	 * @brief Deserialize from binary QDataStream format.
	 * @param dataStream The data stream to read from.
	 */
	void fromData(QDataStream& dataStream);
};

/**
 * @brief Serialize StoredCharacter to QDataStream.
 * @param stream The data stream to write to.
 * @param storedCharacter The character data to serialize.
 * @return Reference to the stream.
 */
QDataStream &operator<<(QDataStream &stream, const StoredCharacter &storedCharacter);

/**
 * @brief Deserialize StoredCharacter from QDataStream.
 * @param stream The data stream to read from.
 * @param storedCharacter The character data to populate.
 * @return Reference to the stream.
 */
QDataStream &operator>>(QDataStream &stream, StoredCharacter &storedCharacter);

#endif // STOREDCHARACTER_HPP
