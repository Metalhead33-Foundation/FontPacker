/**
 * @file StoredCharacter.hpp
 * @brief Per-glyph payload: encoded SDF image plus layout metrics.
 *
 * A StoredCharacter is one entry of a PreprocessedFontFace glyph table. It is
 * self-contained: the encoded distance-field image, the image's intended
 * layout box, and FreeType-compatible metrics. A runtime can lay out and draw
 * text from these fields without linking FreeType.
 *
 * @note For font input, all float metrics are **pixels**, measured in the
 *       FreeType pixel space where 1 em = `bitmap_logical_size -
 *       bitmap_padding`. They are not in font design units and not in stored
 *       texels. In format version 3 every advance is a float in this pixel
 *       space; versions 1 and 2 store `advance_x` / `advance_y` as raw 26.6
 *       integers (version 1 SVG stored plain pixels) and the reader converts them. For SVG input, every numeric
 *       field is in **SVG user units**.
 * @note The fit uses the metric box. metricWidth, metricHeight, horiBearingX
 *       and horiBearingY describe the FreeType metric box of a font glyph or
 *       the viewport of an SVG glyph; the fit does not overwrite them.
 * @note The image is a full square tile (`bitmap_size` x `bitmap_size`, or
 *       `bitmap_logical_size` square for a legacy version 1 file with 0).
 *       There is no atlas and no sub-rectangle. Padding is part of the logical
 *       size; see BINARY_FORMAT.md, section "Raster geometry".
 * @see PreprocessedFontFace, BINARY_FORMAT.md
 */

#ifndef STOREDCHARACTER_HPP
#define STOREDCHARACTER_HPP
#include <cstdint>
#include <QByteArray>
#include <QCborMap>
#include "SDFGenerationArguments.hpp"

/**
 * @brief Opaque FreeType glyph-slot handle.
 * @typedef FT_GlyphSlot
 *
 * Only a pointer type is needed here; the definition lives in FreeType.
 */
typedef struct FT_GlyphSlotRec_*  FT_GlyphSlot;

/**
 * @brief One glyph: encoded distance field plus layout metrics.
 * @struct StoredCharacter
 *
 * The integer fields describe the glyph's intended pixel box; the float fields
 * carry the same information with sub-pixel precision. The `sdf` member holds
 * one complete encoded image (format taken from the owning font face's
 * `imageFormat`); the image is a square, padded, anisotropically stretched
 * tile whose content box must be restored with width/height.
 *
 * Serialization is documented byte for byte in BINARY_FORMAT.md; the C++ struct
 * itself is never written with memcpy.
 */
struct StoredCharacter {
	static constexpr uint32_t CURRENT_VERSION = 3; ///< Glyph payload format version: 3 = float advances, 1/2 = raw 26.6 advances.

// Bitmap data
bool valid;                    ///< Whether this glyph contains a payload. False = 1 byte on disk; true with an empty sdf is a valid empty glyph (e.g. space).
uint32_t width;                ///< Intended glyph box width in whole pixels at em = logical size - padding. Not the texture size.
uint32_t height;               ///< Intended glyph box height in whole pixels at em = logical size - padding. Not the texture size.
int32_t bearing_x;             ///< Horizontal offset from the pen origin to the box's left edge, in whole pixels (signed).
int32_t bearing_y;             ///< Vertical offset from the baseline up to the box's top edge, in whole pixels (signed).
float advance_x;               ///< Horizontal advance in pixels (version 3+). Versions 1/2 store this as a raw 26.6 integer on disk (version 1 SVG: plain pixel).
float advance_y;               ///< Vertical advance in pixels (version 3+). Versions 1/2 store this as a raw 26.6 integer on disk (version 1 SVG: plain pixel).

// Glyph metrics (floating-point, more precise than integer values)
float metricWidth;             ///< Metric-box width used by the fit; FreeType metrics or the SVG viewport width.
float metricHeight;            ///< Metric-box height used by the fit; FreeType metrics or the SVG viewport height.
float horiBearingX;            ///< Sub-pixel left bearing of the metric box (0 for SVG).
float horiBearingY;            ///< Sub-pixel top bearing of the metric box (viewport height for SVG).
float horiAdvance;             ///< Horizontal advance in pixels (normally equal to advance_x).
float vertBearingX;            ///< Vertical-layout bearing X in pixels; meaningful only when the face has vertical metrics.
float vertBearingY;            ///< Vertical-layout bearing Y in pixels; meaningful only when the face has vertical metrics.
float vertAdvance;             ///< Vertical advance in pixels; meaningful only when the face has vertical metrics.

// The actual data
QByteArray sdf;                ///< One encoded square SDF/MSDF/MSDFA image; codec = the font face's imageFormat.

	/**
	 * @brief Serialize to CBOR format.
	 * @param cbor The CBOR map to populate with data.
	 * @param version Container format version; 3 writes float advances, 1/2 write raw 26.6 integers.
	 * @param legacySvgAdvances Version 1 SVG glyphs stored plain pixel advances instead of 26.6.
	 */
	void toCbor(QCborMap& cbor, uint32_t version = CURRENT_VERSION, bool legacySvgAdvances = false) const;

	/**
	 * @brief Serialize to CBOR format.
	 * @param version Container format version.
	 * @param legacySvgAdvances Version 1 SVG plain-pixel advances.
	 * @return A new CBOR map containing the character data.
	 */
	QCborMap toCbor(uint32_t version = CURRENT_VERSION, bool legacySvgAdvances = false) const;

	/**
	 * @brief Deserialize from CBOR format.
	 * @param cbor The CBOR map to read data from.
	 * @param version Container format version; 1/2 values are divided by 64 (unless legacySvgAdvances).
	 * @param legacySvgAdvances Version 1 SVG plain-pixel advances.
	 */
	void fromCbor(const QCborMap& cbor, uint32_t version = CURRENT_VERSION, bool legacySvgAdvances = false);

	/**
	 * @brief Serialize to binary QDataStream format.
	 * @param dataStream The data stream to write to (Qt_4_0, BigEndian).
	 * @param version Container format version; 3 writes floats, 1/2 write raw 26.6.
	 * @param legacySvgAdvances Version 1 SVG plain-pixel advances.
	 * @see BINARY_FORMAT.md
	 */
	void toData(QDataStream& dataStream, uint32_t version = CURRENT_VERSION, bool legacySvgAdvances = false) const;

	/**
	 * @brief Deserialize from binary QDataStream format.
	 * @param dataStream The data stream to read from (Qt_4_0, BigEndian).
	 * @param version Container format version; 1/2 values are divided by 64 (unless legacySvgAdvances).
	 * @param legacySvgAdvances Version 1 SVG plain-pixel advances.
	 * @see BINARY_FORMAT.md
	 */
	void fromData(QDataStream& dataStream, uint32_t version = CURRENT_VERSION, bool legacySvgAdvances = false);
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
