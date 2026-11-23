/**
 * @file StoredVectorImage.hpp
 * @brief Standalone SDF container for a single SVG graphic (`.wodi`).
 *
 * Unlike PreprocessedFontFace, this container describes one vector graphic and
 * stores its source coordinate rectangle, the nominal encoded distance ranges
 * and an optional mip chain. It is written by `--outvectorbin` and read by
 * `--invectorbin`.
 *
 * @note The image is a square tile. The source graphic is stretched onto the
 *       `(processingSize - 2 * padding)` content box; `logical*`, `min*` and
 *       `max*` let a renderer reconstruct the original rectangle and aspect
 *       ratio. Padding is inside `processingSize`, never added on top of it.
 * @see BINARY_FORMAT.md, SdfGenerationContext::processSvg(StoredVectorImage&, ...)
 */

#ifndef STOREDVECTORIMAGE_HPP
#define STOREDVECTORIMAGE_HPP
#include <cstdint>
#include <array>
#include <QList>
#include <QByteArray>
#include <QCborMap>
#include <QDataStream>
#include "SDFGenerationArguments.hpp"

/**
 * @brief A single vector graphic baked into one SDF image (plus mip chain).
 * @struct StoredVectorImage
 */
struct StoredVectorImage
{
	static constexpr std::array<char,4> BINARY_MAGIC{{'W','O','D','I'}}; ///< Magic bytes at the start of every `.wodi` file.
	static constexpr uint32_t CURRENT_VERSION = 1; ///< Current `.wodi` format version; readers reject 0 and values above this.

	/**
	 * @brief Bit flags recording which transforms are already baked into the pixels.
	 * @enum EncodingFlag
	 */
	enum EncodingFlag : uint32_t {
		EncodingInverted = 1u << 0,             ///< SDF values were inverted (`1 - x`).
		EncodingGammaCorrected = 1u << 1,       ///< The cubic gamma adjustment around 0.5 was applied.
		EncodingMaximizedDownsample = 1u << 2,  ///< Mip/downsample generation used maxing instead of averaging.
		EncodingHasMidpointAdjustment = 1u << 3 ///< midpointAdjustment contains a baked divisor.
	};

	uint32_t version = CURRENT_VERSION; ///< Serialization format version written to the header.
	uint32_t processingSize = 0; ///< `L`: square generation size in pixels (typically 512, 1024, 2048 or 4096).
	uint32_t actualSize = 0; ///< `S`: size of the level-0 stored image, i.e. of `mipmaps[0]`; assigned from the final image width.
	uint32_t padding = 0; ///< `P`: border in pixels of the `processingSize` grid, **not** of `actualSize`. Part of `processingSize`.

	/**
	 * @name Original coordinate mapping
	 *
	 * These fields preserve how the original vector coordinate space was
	 * normalized into the square SDF texture. During processing the control-point
	 * bounds are captured, the points are shifted by the minimum, divided by the
	 * original extent, scaled to `processingSize - 2 * padding`, and offset by the
	 * padding. Storing the logical rectangle and original bounds lets renderers
	 * reconstruct the source layout and aspect ratio.
	 * @{
	 */
	float logicalX = 0.0f; ///< X origin of the original canvas/viewBox. Currently 0 because svgtiny does not expose it.
	float logicalY = 0.0f; ///< Y origin of the original canvas/viewBox. Currently 0 because svgtiny does not expose it.
	float logicalWidth = 0.0f; ///< Width of the original canvas/viewBox (SVG user units), excluding padding; falls back to maxX - minX.
	float logicalHeight = 0.0f; ///< Height of the original canvas/viewBox (SVG user units), excluding padding; falls back to maxY - minY.
	float aspectRatio = 1.0f; ///< Precalculated logicalWidth / logicalHeight, or 1.0 when the height is 0.
	float minX = 0.0f; ///< Smallest control-point X before normalization, in source units.
	float maxX = 0.0f; ///< Largest control-point X before normalization, in source units.
	float minY = 0.0f; ///< Smallest control-point Y before normalization, in source units.
	float maxY = 0.0f; ///< Largest control-point Y before normalization, in source units.
	/// @}

	float distanceRangeX = 0.0f; ///< Nominal encoded radius in X, in level-0 texels: (samples/2 or padding) * actualSize / processingSize.
	float distanceRangeY = 0.0f; ///< Nominal encoded radius in Y, in level-0 texels; same formula as X.
	uint32_t encodingFlags = 0; ///< Bitmask of EncodingFlag values describing transforms baked into the pixels.
	float midpointAdjustment = 1.0f; ///< Baked midpoint divisor; valid only when EncodingHasMidpointAdjustment is set (otherwise written as 1.0).
	SDFType type = SDF;                              ///< Distance-field flavour: SDF, MSDF or MSDFA.
	DistanceType distType = Manhattan;               ///< Distance norm used during generation.
	std::array<char,8> imageFormat{{'P','N','G','\0','\0','\0','\0','\0'}}; ///< Null-terminated, zero-padded, upper-case codec name shared by all mip levels.
	QList<QByteArray> mipmaps; ///< Encoded images; [0] is level 0 at actualSize, each following entry is half the previous size. At least one entry.

	/**
	 * @brief Serialize to CBOR format.
	 * @param cbor The CBOR map to populate with data.
	 */
	void toCbor(QCborMap& cbor) const;

	/**
	 * @brief Serialize to CBOR format.
	 * @return A new CBOR map containing the vector image data.
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
	 * @brief Set the null-terminated encoded vector image format.
	 *
	 * The value is trimmed, upper-cased, truncated to 7 bytes and zero-padded.
	 * An empty value falls back to `PNG`.
	 * @param format Image format name accepted by QImageWriter (e.g. PNG, JPG, WEBP).
	 */
	void setImageFormat(const QByteArray& format);

	/**
	 * @brief Get the encoded vector image format as a NUL-terminated byte array.
	 * @return Image format name accepted by QImageWriter, without the padding.
	 */
	QByteArray imageFormatBytes() const;
};

#endif // STOREDVECTORIMAGE_HPP
