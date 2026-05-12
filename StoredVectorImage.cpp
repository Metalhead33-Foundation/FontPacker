#include "StoredVectorImage.hpp"
#include "ConstStrings.hpp"
#include <QCborArray>
#include <algorithm>
#include <stdexcept>

namespace {

const QString VECTOR_PROCESSING_SIZE_KEY = QStringLiteral("processingSize");
const QString VECTOR_ACTUAL_SIZE_KEY = QStringLiteral("actualSize");
const QString VECTOR_LOGICAL_X_KEY = QStringLiteral("logicalX");
const QString VECTOR_LOGICAL_Y_KEY = QStringLiteral("logicalY");
const QString VECTOR_LOGICAL_WIDTH_KEY = QStringLiteral("logicalWidth");
const QString VECTOR_LOGICAL_HEIGHT_KEY = QStringLiteral("logicalHeight");
const QString VECTOR_ASPECT_RATIO_KEY = QStringLiteral("aspectRatio");
const QString VECTOR_MIN_X_KEY = QStringLiteral("minX");
const QString VECTOR_MAX_X_KEY = QStringLiteral("maxX");
const QString VECTOR_MIN_Y_KEY = QStringLiteral("minY");
const QString VECTOR_MAX_Y_KEY = QStringLiteral("maxY");
const QString VECTOR_DISTANCE_RANGE_X_KEY = QStringLiteral("distanceRangeX");
const QString VECTOR_DISTANCE_RANGE_Y_KEY = QStringLiteral("distanceRangeY");
const QString VECTOR_ENCODING_FLAGS_KEY = QStringLiteral("encodingFlags");
const QString VECTOR_MIDPOINT_ADJUSTMENT_KEY = QStringLiteral("midpointAdjustment");
const QString VECTOR_MIPMAPS_KEY = QStringLiteral("mipmaps");

void validateStoredVectorImageVersion(uint32_t version)
{
	if(version == 0 || version > StoredVectorImage::CURRENT_VERSION) {
		throw std::runtime_error("Unsupported StoredVectorImage format version.");
	}
}

void validateStoredVectorImageMagic(QDataStream& dataStream)
{
	std::array<char,StoredVectorImage::BINARY_MAGIC.size()> magic{};
	if(dataStream.readRawData(magic.data(), magic.size()) != static_cast<int>(magic.size()) || magic != StoredVectorImage::BINARY_MAGIC) {
		throw std::runtime_error("Invalid StoredVectorImage binary magic.");
	}
}

void validateStoredVectorImageCborType(const QCborMap& cbor)
{
	if(cbor.contains(CBOR_CONTAINER_TYPE_KEY) && cbor[CBOR_CONTAINER_TYPE_KEY].toString() != CBOR_STORED_VECTOR_IMAGE_TYPE) {
		throw std::runtime_error("Invalid StoredVectorImage CBOR container type.");
	}
}

}

void StoredVectorImage::toCbor(QCborMap& cbor) const
{
	cbor.insert(CBOR_CONTAINER_TYPE_KEY, CBOR_STORED_VECTOR_IMAGE_TYPE);
	cbor.insert(VERSION_KEY, version);
	cbor.insert(VECTOR_PROCESSING_SIZE_KEY, processingSize);
	cbor.insert(VECTOR_ACTUAL_SIZE_KEY, actualSize);
	cbor.insert(PADDING_KEY, padding);
	cbor.insert(VECTOR_LOGICAL_X_KEY, logicalX);
	cbor.insert(VECTOR_LOGICAL_Y_KEY, logicalY);
	cbor.insert(VECTOR_LOGICAL_WIDTH_KEY, logicalWidth);
	cbor.insert(VECTOR_LOGICAL_HEIGHT_KEY, logicalHeight);
	cbor.insert(VECTOR_ASPECT_RATIO_KEY, aspectRatio);
	cbor.insert(VECTOR_MIN_X_KEY, minX);
	cbor.insert(VECTOR_MAX_X_KEY, maxX);
	cbor.insert(VECTOR_MIN_Y_KEY, minY);
	cbor.insert(VECTOR_MAX_Y_KEY, maxY);
	cbor.insert(VECTOR_DISTANCE_RANGE_X_KEY, distanceRangeX);
	cbor.insert(VECTOR_DISTANCE_RANGE_Y_KEY, distanceRangeY);
	cbor.insert(VECTOR_ENCODING_FLAGS_KEY, encodingFlags);
	cbor.insert(VECTOR_MIDPOINT_ADJUSTMENT_KEY, midpointAdjustment);
	cbor.insert(TYPE_KEY, static_cast<unsigned>(type));
	cbor.insert(DIST_KEY, static_cast<unsigned>(distType));
	cbor.insert(IMAGE_FORMAT_KEY, QString::fromLatin1(imageFormatBytes()));

	QCborArray mipmapArray;
	for(const QByteArray& mipmap : mipmaps) {
		mipmapArray.push_back(mipmap);
	}
	cbor.insert(VECTOR_MIPMAPS_KEY, mipmapArray);
}

QCborMap StoredVectorImage::toCbor() const
{
	QCborMap cbor;
	toCbor(cbor);
	return cbor;
}

void StoredVectorImage::fromCbor(const QCborMap& cbor)
{
	validateStoredVectorImageCborType(cbor);
	this->version = static_cast<uint32_t>(cbor[VERSION_KEY].toInteger(CURRENT_VERSION));
	validateStoredVectorImageVersion(version);
	this->processingSize = static_cast<uint32_t>(cbor[VECTOR_PROCESSING_SIZE_KEY].toInteger(0));
	this->actualSize = static_cast<uint32_t>(cbor[VECTOR_ACTUAL_SIZE_KEY].toInteger(0));
	this->padding = static_cast<uint32_t>(cbor[PADDING_KEY].toInteger(0));
	this->logicalX = cbor[VECTOR_LOGICAL_X_KEY].toDouble(0.0);
	this->logicalY = cbor[VECTOR_LOGICAL_Y_KEY].toDouble(0.0);
	this->logicalWidth = cbor[VECTOR_LOGICAL_WIDTH_KEY].toDouble(0.0);
	this->logicalHeight = cbor[VECTOR_LOGICAL_HEIGHT_KEY].toDouble(0.0);
	this->aspectRatio = cbor[VECTOR_ASPECT_RATIO_KEY].toDouble(1.0);
	this->minX = cbor[VECTOR_MIN_X_KEY].toDouble(0.0);
	this->maxX = cbor[VECTOR_MAX_X_KEY].toDouble(0.0);
	this->minY = cbor[VECTOR_MIN_Y_KEY].toDouble(0.0);
	this->maxY = cbor[VECTOR_MAX_Y_KEY].toDouble(0.0);
	this->distanceRangeX = cbor[VECTOR_DISTANCE_RANGE_X_KEY].toDouble(0.0);
	this->distanceRangeY = cbor[VECTOR_DISTANCE_RANGE_Y_KEY].toDouble(0.0);
	this->encodingFlags = static_cast<uint32_t>(cbor[VECTOR_ENCODING_FLAGS_KEY].toInteger(0));
	this->midpointAdjustment = cbor[VECTOR_MIDPOINT_ADJUSTMENT_KEY].toDouble(1.0);
	this->type = static_cast<SDFType>(cbor[TYPE_KEY].toInteger(SDF));
	this->distType = static_cast<DistanceType>(cbor[DIST_KEY].toInteger(Manhattan));
	setImageFormat(cbor[IMAGE_FORMAT_KEY].toString(QStringLiteral("PNG")).toLatin1());

	mipmaps.clear();
	const QCborArray mipmapArray = cbor[VECTOR_MIPMAPS_KEY].toArray();
	for(const QCborValue& mipmap : mipmapArray) {
		mipmaps.push_back(mipmap.toByteArray());
	}
}

void StoredVectorImage::toData(QDataStream& dataStream) const
{
	dataStream.writeRawData(BINARY_MAGIC.data(), BINARY_MAGIC.size());
	dataStream << version << processingSize << actualSize << padding
			   << logicalX << logicalY << logicalWidth << logicalHeight << aspectRatio
			   << minX << maxX << minY << maxY
			   << distanceRangeX << distanceRangeY << encodingFlags << midpointAdjustment
			   << static_cast<uint8_t>(type) << static_cast<uint8_t>(distType);
	dataStream.writeRawData(imageFormat.data(), imageFormat.size());
	dataStream << static_cast<uint32_t>(mipmaps.size());
	for(const QByteArray& mipmap : mipmaps) {
		dataStream << static_cast<uint32_t>(mipmap.size());
		dataStream.writeRawData(mipmap.data(), static_cast<int>(mipmap.size()));
	}
}

void StoredVectorImage::fromData(QDataStream& dataStream)
{
	uint8_t tmpType;
	uint8_t tmpDistType;
	uint32_t mipmapCount;
	validateStoredVectorImageMagic(dataStream);
	dataStream >> version;
	validateStoredVectorImageVersion(version);
	dataStream >> processingSize >> actualSize >> padding
			   >> logicalX >> logicalY >> logicalWidth >> logicalHeight >> aspectRatio
			   >> minX >> maxX >> minY >> maxY
			   >> distanceRangeX >> distanceRangeY >> encodingFlags >> midpointAdjustment
			   >> tmpType >> tmpDistType;
	type = static_cast<SDFType>(tmpType);
	distType = static_cast<DistanceType>(tmpDistType);
	dataStream.readRawData(imageFormat.data(), imageFormat.size());
	imageFormat.back() = '\0';

	dataStream >> mipmapCount;
	mipmaps.clear();
	for(uint32_t i = 0; i < mipmapCount; ++i) {
		uint32_t mipmapSize;
		dataStream >> mipmapSize;
		QByteArray mipmap(mipmapSize, 0);
		dataStream.readRawData(mipmap.data(), static_cast<int>(mipmapSize));
		mipmaps.push_back(mipmap);
	}
}

void StoredVectorImage::setImageFormat(const QByteArray& format)
{
	imageFormat.fill('\0');
	QByteArray normalized = format.trimmed().toUpper();
	if(normalized.isEmpty()) normalized = QByteArrayLiteral("PNG");
	const qsizetype bytesToCopy = std::min<qsizetype>(normalized.size(), static_cast<qsizetype>(imageFormat.size() - 1));
	std::copy_n(normalized.constData(), bytesToCopy, imageFormat.data());
}

QByteArray StoredVectorImage::imageFormatBytes() const
{
	const auto terminator = std::find(imageFormat.begin(), imageFormat.end(), '\0');
	return QByteArray(imageFormat.data(), std::distance(imageFormat.begin(), terminator));
}
