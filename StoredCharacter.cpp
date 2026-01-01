/**
 * @file StoredCharacter.cpp
 * @brief Implementation of StoredCharacter serialization and deserialization.
 * 
 * Implements conversion between StoredCharacter structures and:
 * - CBOR format (for JSON-like serialization)
 * - Binary QDataStream format (see BINARY_FORMAT.md)
 */

#include "StoredCharacter.hpp"
#include "ConstStrings.hpp"

void StoredCharacter::toCbor(QCborMap& cbor) const
{
	cbor.insert(VALID_KEY,valid);
	cbor.insert(WIDTH_KEY,width);
	cbor.insert(HEIGHT_KEY,height);
	cbor.insert(BEARING_X_KEY,bearing_x);
	cbor.insert(BEARING_Y_KEY,bearing_y);
	cbor.insert(ADVANCE_X_KEY,advance_x);
	cbor.insert(ADVANCE_Y_KEY,advance_y);
	cbor.insert(METRICSWIDTH_KEY,metricWidth);
	cbor.insert(METRICSHEIGHT_KEY,metricHeight);
	cbor.insert(HORIBEARINGX_KEY,horiBearingX);
	cbor.insert(HORIBEARINGY_KEY,horiBearingY);
	cbor.insert(HORIADVANCE_KEY,horiAdvance);
	cbor.insert(VERTBEARINGX_KEY,vertBearingX);
	cbor.insert(VERTBEARINGY_KEY,vertBearingY);
	cbor.insert(VERTADVANCE_KEY,vertAdvance);
	cbor.insert(SDF_KEY,sdf);
}

QCborMap StoredCharacter::toCbor() const
{
	QCborMap cbor;
	toCbor(cbor);
	return cbor;
}

void StoredCharacter::fromCbor(const QCborMap& cbor)
{
	// Base data
	this->valid = cbor[VALID_KEY].toBool(false);
	this->width = cbor[WIDTH_KEY].toInteger(0);
	this->height = cbor[HEIGHT_KEY].toInteger(0);
	this->bearing_x = cbor[BEARING_X_KEY].toInteger(0);
	this->bearing_y = cbor[BEARING_Y_KEY].toInteger(0);
	this->advance_x = cbor[ADVANCE_X_KEY].toInteger(0);
	this->advance_y = cbor[ADVANCE_Y_KEY].toInteger(0);
	// Metrics
	this->metricWidth = cbor[METRICSWIDTH_KEY].toDouble();
	this->metricHeight = cbor[METRICSHEIGHT_KEY].toDouble();
	this->horiBearingX = cbor[HORIBEARINGX_KEY].toDouble();
	this->horiBearingY = cbor[HORIBEARINGY_KEY].toDouble();
	this->horiAdvance = cbor[HORIADVANCE_KEY].toDouble();
	this->vertBearingX = cbor[VERTBEARINGX_KEY].toDouble();
	this->vertBearingY = cbor[VERTBEARINGY_KEY].toDouble();
	this->vertAdvance = cbor[VERTADVANCE_KEY].toDouble();
	// The actual data
	this->sdf = cbor[SDF_KEY].toByteArray();
}

void StoredCharacter::toData(QDataStream& dataStream) const
{
	dataStream << valid;
	if(valid) {
		dataStream << width << height << bearing_x << bearing_y << advance_x << advance_y << metricWidth << metricHeight
				   << horiBearingX << horiBearingY << horiAdvance << vertBearingX << vertBearingY << vertAdvance << static_cast<uint32_t>(sdf.length());
		dataStream.writeRawData(sdf.data(),sdf.length());
	}
}

void StoredCharacter::fromData(QDataStream& dataStream)
{
	dataStream >> valid;
	if(valid) {
		uint32_t sdf_len;
		dataStream >> width >> height >> bearing_x >> bearing_y >> advance_x >> advance_y >> metricWidth >> metricHeight
			>> horiBearingX >> horiBearingY >> horiAdvance >> vertBearingX >> vertBearingY >> vertAdvance >> sdf_len;
		QByteArray raw(sdf_len, 0);
		dataStream.readRawData(raw.data(),sdf_len);
		this->sdf = raw;
	}
}

QDataStream& operator<<(QDataStream& stream, const StoredCharacter& storedCharacter) {
	storedCharacter.toData(stream);
	return stream;
}

QDataStream& operator>>(QDataStream& stream, StoredCharacter& storedCharacter) {
	storedCharacter.fromData(stream);
	return stream;
}
