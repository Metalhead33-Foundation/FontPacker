/**
 * @file PreprocessedFontFace.hpp
 * @brief Core data structure for storing preprocessed font face data with glyphs and kerning information.
 */

#ifndef PREPROCESSEDFONTFACE_HPP
#define PREPROCESSEDFONTFACE_HPP
#include <cstdint>
#include <QString>
#include <QCborMap>
#include "SDFGenerationArguments.hpp"
#include "StoredCharacter.hpp"

/**
 * @brief 2D vector type represented as a pair of floats.
 * @typedef Vec2f
 */
typedef std::pair<float,float> Vec2f;

/**
 * @brief Map of character code points to kerning offset vectors.
 * @typedef PerCharacterKerning
 * Maps a second character code point to a Vec2f kerning offset when paired with a first character.
 */
typedef QMap<uint32_t,Vec2f> PerCharacterKerning;

/**
 * @brief Map of character code points to per-character kerning data.
 * @typedef KerningMap
 * Maps a first character code point to its PerCharacterKerning map.
 */
typedef QMap<uint32_t,PerCharacterKerning> KerningMap;

/**
 * @brief Serialize PerCharacterKerning to QDataStream.
 * @param stream The data stream to write to.
 * @param processedFontFace The kerning data to serialize.
 * @return Reference to the stream.
 */
QDataStream &operator<<(QDataStream &stream, const PerCharacterKerning &processedFontFace);

/**
 * @brief Deserialize PerCharacterKerning from QDataStream.
 * @param stream The data stream to read from.
 * @param processedFontFace The kerning data to populate.
 * @return Reference to the stream.
 */
QDataStream &operator>>(QDataStream &stream, PerCharacterKerning &processedFontFace);

/**
 * @brief Serialize KerningMap to QDataStream.
 * @param stream The data stream to write to.
 * @param processedFontFace The kerning map to serialize.
 * @return Reference to the stream.
 */
QDataStream &operator<<(QDataStream &stream, const KerningMap &processedFontFace);

/**
 * @brief Deserialize KerningMap from QDataStream.
 * @param stream The data stream to read from.
 * @param processedFontFace The kerning map to populate.
 * @return Reference to the stream.
 */
QDataStream &operator>>(QDataStream &stream, KerningMap &processedFontFace);

/**
 * @brief Main structure containing all preprocessed font face data.
 * 
 * This structure stores font metadata, glyph data, and kerning information.
 * It supports serialization to both CBOR and binary QDataStream formats.
 * See BINARY_FORMAT.md for the binary format specification.
 */
struct PreprocessedFontFace {
	QString fontFamilyName;                    ///< Font family name (UTF-8)
	SDFType type;                              ///< Type of signed distance field (SDF, MSDF, MSDFA)
	DistanceType distType;                     ///< Distance calculation method (Manhattan or Euclidean)
	uint32_t bitmap_size;                      ///< Internal bitmap processing size in pixels
	uint32_t bitmap_logical_size;              ///< Logical/intended bitmap size in pixels
	uint32_t bitmap_padding;                  ///< Padding around glyphs in pixels
	bool hasVert;                              ///< Whether vertical layout metrics are available
	bool jpeg;                                 ///< Whether SDF bitmap data is JPEG compressed
	KerningMap kerning;                        ///< Kerning information for character pairs
	QMap<uint32_t,StoredCharacter> storedCharacters; ///< Map of Unicode code points to glyph data
	
	/**
	 * @brief Serialize to CBOR format.
	 * @param cbor The CBOR map to populate with data.
	 */
	void toCbor(QCborMap& cbor) const;
	
	/**
	 * @brief Serialize to CBOR format.
	 * @return A new CBOR map containing the font face data.
	 */
	QCborMap toCbor(void) const;
	
	/**
	 * @brief Deserialize from CBOR format.
	 * @param cbor The CBOR map to read data from.
	 */
	void fromCbor(const QCborMap& cbor);
	
	/**
	 * @brief Serialize to binary QDataStream format.
	 * @param dataStream The data stream to write to (must be Qt_4_0, BigEndian).
	 * @see BINARY_FORMAT.md for format specification.
	 */
	void toData(QDataStream& dataStream) const;
	
	/**
	 * @brief Deserialize from binary QDataStream format.
	 * @param dataStream The data stream to read from (must be Qt_4_0, BigEndian).
	 * @see BINARY_FORMAT.md for format specification.
	 */
	void fromData(QDataStream& dataStream);
	
	/**
	 * @brief Export glyph SDF data to individual files.
	 * @param pattern File path pattern with %1 placeholder for character code (e.g., "glyph_%1.bin").
	 */
	void outToFolder(const QString& pattern) const;
};

/**
 * @brief Serialize PreprocessedFontFace to QDataStream.
 * @param stream The data stream to write to.
 * @param processedFontFace The font face to serialize.
 * @return Reference to the stream.
 */
QDataStream &operator<<(QDataStream &stream, const PreprocessedFontFace &processedFontFace);

/**
 * @brief Deserialize PreprocessedFontFace from QDataStream.
 * @param stream The data stream to read from.
 * @param processedFontFace The font face to populate.
 * @return Reference to the stream.
 */
QDataStream &operator>>(QDataStream &stream, PreprocessedFontFace &processedFontFace);

#endif // PREPROCESSEDFONTFACE_HPP
