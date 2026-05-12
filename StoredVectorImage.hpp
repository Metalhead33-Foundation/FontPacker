#ifndef STOREDVECTORIMAGE_HPP
#define STOREDVECTORIMAGE_HPP
#include <cstdint>
#include <array>
#include <QList>
#include <QByteArray>
#include <QCborMap>
#include <QDataStream>
#include "SDFGenerationArguments.hpp"

struct StoredVectorImage
{
	static constexpr std::array<char,4> BINARY_MAGIC{{'W','O','D','I'}};
	static constexpr uint32_t CURRENT_VERSION = 1;

	enum EncodingFlag : uint32_t {
		EncodingInverted = 1u << 0,             ///< SDF values were inverted after generation.
		EncodingGammaCorrected = 1u << 1,       ///< SDF values were gamma adjusted after generation.
		EncodingMaximizedDownsample = 1u << 2,  ///< Mip/downsample generation used maxing instead of averaging.
		EncodingHasMidpointAdjustment = 1u << 3 ///< \ref midpointAdjustment contains a baked adjustment value.
	};

	uint32_t version = CURRENT_VERSION; ///< StoredVectorImage serialization format version.
	uint32_t processingSize = 0; ///< The pixel size (width and height) at which the SDF was processed originally. Usually a number like 512, 1024, 2048 or 4096.
	uint32_t actualSize = 0; ///< The final size of the SDF (width and height) in pixels. If we have mipmaps, this is the size of the largest mipmap, level 0.
	uint32_t padding = 0; ///< Size of padding in pixels, out of the \ref processingSize, not \ref actualSize.

	/**
	 * @name Original coordinate mapping
	 *
	 * These fields preserve how the original vector coordinate space was
	 * normalized into the square SDF texture. During processing, the minimum
	 * X/Y coordinates are subtracted from every point, coordinates are divided
	 * by their original bounds, scaled by the processing size minus padding,
	 * and then offset by the padding. Storing the logical rectangle and original
	 * bounds lets renderers reconstruct the source layout and aspect ratio.
	 * @{
	 */
	float logicalX = 0.0f; ///< X origin of the original vector canvas/viewBox before normalization.
	float logicalY = 0.0f; ///< Y origin of the original vector canvas/viewBox before normalization.
	float logicalWidth = 0.0f; ///< Width of the original vector canvas/viewBox, excluding padding.
	float logicalHeight = 0.0f; ///< Height of the original vector canvas/viewBox, excluding padding.
	float aspectRatio = 1.0f; ///< Precalculated \ref logicalWidth / \ref logicalHeight.
	float minX = 0.0f; ///< The smallest X coordinate on the original decomposed vector image, before normalization. Given in source pixel units.
	float maxX = 0.0f; ///< The largest X coordinate on the original decomposed vector image, before normalization. Given in source pixel units, should be compared to \ref logicalWidth.
	float minY = 0.0f; ///< The smallest Y coordinate on the original decomposed vector image, before normalization. Given in source pixel units.
	float maxY = 0.0f; ///< The largest Y coordinate on the original decomposed vector image, before normalization. Given in source pixel units, should be compared to \ref logicalHeight.
	/// @}

	float distanceRangeX = 0.0f; ///< Horizontal signed-distance range encoded around the midpoint, in level-0 texture pixels.
	float distanceRangeY = 0.0f; ///< Vertical signed-distance range encoded around the midpoint, in level-0 texture pixels.
	uint32_t encodingFlags = 0; ///< Bitmask of \ref EncodingFlag values describing baked SDF value transforms.
	float midpointAdjustment = 1.0f; ///< Baked midpoint adjustment value, valid when \ref EncodingHasMidpointAdjustment is set.
	SDFType type = SDF;                              ///< Type of signed distance field (SDF, MSDF, MSDFA)
	DistanceType distType = Manhattan;               ///< Distance calculation method (Manhattan or Euclidean)
	std::array<char,8> imageFormat{{'P','N','G','\0','\0','\0','\0','\0'}}; ///< Null-terminated encoded image format
	QList<QByteArray> mipmaps; ///< The final stored mipmaps.

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
	 * @brief Set the null-terminated encoded vector image format.
	 * @param format Image format name accepted by QImageWriter (e.g. PNG, JPG, WEBP).
	 */
	void setImageFormat(const QByteArray& format);

	/**
	 * @brief Get the encoded vector image format as a null-terminated byte array.
	 * @return Image format name accepted by QImageWriter.
	 */
	QByteArray imageFormatBytes() const;
};

#endif // STOREDVECTORIMAGE_HPP
