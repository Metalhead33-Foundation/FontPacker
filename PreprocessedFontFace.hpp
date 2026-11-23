/**
 * @file PreprocessedFontFace.hpp
 * @brief In-memory representation of a preprocessed font: face metrics, glyphs, kerning.
 *
 * This is the object serialized as `.wodf` (binary) or as a CBOR map. It owns a
 * map of StoredCharacter glyphs plus the kerning table and the face-wide
 * metrics needed for layout.
 *
 * @section geometry Geometry summary
 * The three size fields are single integers because every stored glyph image is
 * a **square** tile:
 *
 * - `bitmap_logical_size` (`L`) - resolution at which the distance field was
 *   generated; padding is **inside** this size;
 * - `bitmap_padding` (`P`) - border on all four sides, in logical pixels; the
 *   content box is `(L - 2P) x (L - 2P)`;
 * - `bitmap_size` (`S`) - stored image size after downsampling; `0` means
 *   "no downsampling", i.e. the stored images are `L x L`.
 *
 * Glyph images are stretched anisotropically into the content box. The real
 * aspect ratio is recovered from each StoredCharacter's `width`/`height` and
 * bearings. See BINARY_FORMAT.md for the full explanation and formulas.
 *
 * @note For glyphs generated from outlines or SVG, four float fields of each
 *       StoredCharacter (`metricWidth`, `metricHeight`, `horiBearingX`,
 *       `horiBearingY`) are overwritten with the outline bounds by the current
 *       fit step. See StoredCharacter's warning before using them.
 *
 * @see StoredCharacter, KerningMap, BINARY_FORMAT.md
 */

#ifndef PREPROCESSEDFONTFACE_HPP
#define PREPROCESSEDFONTFACE_HPP
#include <array>
#include <cstdint>
#include <QByteArray>
#include <QString>
#include <QCborMap>
#include "SDFGenerationArguments.hpp"
#include "StoredCharacter.hpp"

/**
 * @brief 2D vector represented as a pair of floats.
 * @typedef Vec2f
 *
 * Used for kerning offsets. Components are in pixels at the FreeType pixel size
 * `internalProcessSize - padding`, matching StoredCharacter::horiAdvance.
 */
typedef std::pair<float,float> Vec2f;

/**
 * @brief Map of second-character code points to kerning offset vectors.
 * @typedef PerCharacterKerning
 *
 * For a given first character, maps the second character of a pair to its
 * (x, y) kerning offset. Only non-zero pairs are stored.
 */
typedef QMap<uint32_t,Vec2f> PerCharacterKerning;

/**
 * @brief Map of first-character code points to their per-character kerning data.
 * @typedef KerningMap
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
 * @brief A complete preprocessed font face.
 * @struct PreprocessedFontFace
 *
 * For font input, the layout fields (`ascender` ... `maxAdvance`) are stored in
 * pixels at the FreeType pixel size `bitmap_logical_size - bitmap_padding`, and
 * `unitsPerEm` is the original design-unit count, so
 * `value_font_units = value_px * unitsPerEm /
 * (bitmap_logical_size - bitmap_padding)`.
 * For SVG input there is no FreeType face: `ascender`/`faceHeight` are the
 * viewport height, `maxAdvance` is the viewport width, `descender` is 0 and
 * `unitsPerEm` is the viewport height; all values are in SVG user units.
 *
 * `storedCharacters` is keyed by Unicode code point. Every value's `sdf` member
 * is one independently encoded square tile; there is no atlas in this object.
 */
struct PreprocessedFontFace {
	static constexpr std::array<char,4> BINARY_MAGIC{{'W','O','D','F'}}; ///< Magic bytes at the start of every `.wodf` file.
	static constexpr uint32_t CURRENT_VERSION = 1; ///< Current `.wodf` format version; readers reject 0 and values above this.

	uint32_t version = CURRENT_VERSION;        ///< Serialization format version written to the header.
	QString fontFamilyName;                    ///< Font family name, serialized as length-prefixed UTF-8 (NUL included).
	SDFType type;                              ///< Distance-field flavour: SDF, MSDF or MSDFA.
	DistanceType distType;                     ///< Distance norm used during generation (Manhattan or Euclidean).
	uint32_t bitmap_size;                      ///< **Stored** square size `S` after downsampling; `0` means "same as bitmap_logical_size".
	uint32_t bitmap_logical_size;              ///< **Generation** square size `L`; padding is included in it.
	uint32_t bitmap_padding;                   ///< Border `P` on all four sides, in pixels of the `L` grid. Not added on top of `L`.
	bool hasVert;                              ///< Whether the vertical metrics (vertBearing*, vertAdvance) are meaningful.
	std::array<char,8> imageFormat{{'P','N','G','\0','\0','\0','\0','\0'}}; ///< Null-terminated, zero-padded, upper-case image codec name (max 7 characters).
	float ascender = 0.0f;                     ///< Font: ascender in pixels at em = L - P. SVG: viewport height in user units.
	float descender = 0.0f;                    ///< Font: descender in pixels at em = L - P (usually negative). SVG: 0.
	float faceHeight = 0.0f;                   ///< Font: baseline-to-baseline distance in pixels at em = L - P. SVG: viewport height.
	float maxAdvance = 0.0f;                   ///< Font: maximum advance in pixels at em = L - P. SVG: viewport width.
	uint32_t unitsPerEm = 0;                   ///< Font: original design units per EM (converts pixels to font units). SVG: viewport height, not a design-unit count.
	KerningMap kerning;                        ///< Sparse kerning table; offsets are pixels at em = L - P.
	QMap<uint32_t,StoredCharacter> storedCharacters; ///< Glyph payloads keyed by Unicode code point.

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
	 * @see BINARY_FORMAT.md for the complete field order.
	 */
	void toData(QDataStream& dataStream) const;

	/**
	 * @brief Deserialize from binary QDataStream format.
	 * @param dataStream The data stream to read from (must be Qt_4_0, BigEndian).
	 * @see BINARY_FORMAT.md for the complete field order.
	 */
	void fromData(QDataStream& dataStream);

	/**
	 * @brief Set the null-terminated encoded glyph image format.
	 *
	 * The value is trimmed, upper-cased, truncated to 7 bytes and zero-padded.
	 * An empty value falls back to `PNG`.
	 * @param format Image format name accepted by QImageWriter (e.g. PNG, JPG, WEBP).
	 */
	void setImageFormat(const QByteArray& format);

	/**
	 * @brief Get the encoded glyph image format as a NUL-terminated byte array.
	 * @return Image format name accepted by QImageWriter, without the padding.
	 */
	QByteArray imageFormatBytes() const;

	/**
	 * @brief Export each glyph's encoded payload to an individual file.
 * @param pattern File path pattern with a `%1` placeholder for the code point
 *        (e.g. `glyph_%1.png`). The extension should match imageFormat, because
 *        the bytes are written as stored and not re-encoded.
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
