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
 *       texels. The single exception is advance_x / advance_y, which are raw
 *       26.6 fixed-point (divide by 64). For SVG input, every numeric field is
 *       in **SVG user units** and advance_x / advance_y are plain pixels.
 * @warning For glyphs produced from outlines or SVG, the fit step overwrites
 *       metricWidth, metricHeight, horiBearingX and horiBearingY with the
 *       outline bounds (minX, maxX, minY, maxY respectively). Use width,
 *       height, bearing_x, bearing_y and horiAdvance for layout. Only raster
 *       glyphs (--forceraster or bitmap fonts) keep the FreeType metric values
 *       in those four fields. See BINARY_FORMAT.md, "Metric caveat".
 * @note The image is a full square tile (`bitmap_size` x `bitmap_size`, or
 *       `bitmap_logical_size` square when that field is 0). There is no atlas
 *       and no sub-rectangle. Padding is part of the logical size; see
 *       BINARY_FORMAT.md, section "Raster geometry".
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
// Bitmap data
bool valid;                    ///< Whether this glyph contains a payload. False = 1 byte on disk; true with an empty sdf is a valid empty glyph (e.g. space).
uint32_t width;                ///< Intended glyph box width in whole pixels at em = logical size - padding. Not the texture size.
uint32_t height;               ///< Intended glyph box height in whole pixels at em = logical size - padding. Not the texture size.
int32_t bearing_x;             ///< Horizontal offset from the pen origin to the box's left edge, in whole pixels (signed).
int32_t bearing_y;             ///< Vertical offset from the baseline up to the box's top edge, in whole pixels (signed).
uint32_t advance_x;             ///< Horizontal advance in **26.6 fixed-point pixels** (divide by 64).
uint32_t advance_y;             ///< Vertical advance in **26.6 fixed-point pixels** (divide by 64).

// Glyph metrics (floating-point, more precise than integer values)
float metricWidth;             ///< Raster: metric-box width. Vector: clobbered with the outline's minimum X (see the struct warning).
float metricHeight;            ///< Raster: metric-box height. Vector: clobbered with the outline's maximum X (not a height).
float horiBearingX;            ///< Raster: sub-pixel bearing X. Vector: clobbered with the outline's minimum Y.
float horiBearingY;            ///< Raster: sub-pixel bearing Y. Vector: clobbered with the outline's maximum Y.
float horiAdvance;             ///< Horizontal advance in pixels (advance_x / 64).
float vertBearingX;            ///< Vertical-layout bearing X in pixels; meaningful only when the face has vertical metrics.
float vertBearingY;            ///< Vertical-layout bearing Y in pixels; meaningful only when the face has vertical metrics.
float vertAdvance;             ///< Vertical advance in pixels; meaningful only when the face has vertical metrics.

// The actual data
QByteArray sdf;                ///< One encoded square SDF/MSDF/MSDFA image; codec = the font face's imageFormat.

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
	 * @param dataStream The data stream to write to (Qt_4_0, BigEndian).
	 * @see BINARY_FORMAT.md
	 */
	void toData(QDataStream& dataStream) const;

	/**
	 * @brief Deserialize from binary QDataStream format.
	 * @param dataStream The data stream to read from (Qt_4_0, BigEndian).
	 * @see BINARY_FORMAT.md
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
