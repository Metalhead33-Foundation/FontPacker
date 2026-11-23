/**
 * @file StoredCharacter.cpp
 * @brief Implementation of StoredCharacter serialization and deserialization.
 *
 * Implements conversion between StoredCharacter structures and:
 * - CBOR format (for JSON-like serialization)
 * - Binary QDataStream format (see BINARY_FORMAT.md)
 *
 * Format versions 1 and 2 stored advance_x / advance_y as raw 26.6 fixed-point
 * integers. Version 3 stores them as float pixels; the reader converts the old
 * values when loading a version 1 or 2 container.
 */

#include "StoredCharacter.hpp"
#include "ConstStrings.hpp"
#include <cmath>
#include <limits>

namespace {

float decodeFixed26_6(uint32_t raw)
{
	return static_cast<float>(raw) / 64.0f;
}

uint32_t encodeFixed26_6(float value)
{
	if(!(value > 0.0f)) return 0;
	const double scaled = std::round(static_cast<double>(value) * 64.0);
	if(scaled >= static_cast<double>(std::numeric_limits<uint32_t>::max())) {
		return std::numeric_limits<uint32_t>::max();
	}
	return static_cast<uint32_t>(scaled);
}

uint32_t encodePlainAdvance(float value)
{
	if(!(value > 0.0f)) return 0;
	const double rounded = std::round(static_cast<double>(value));
	if(rounded >= static_cast<double>(std::numeric_limits<uint32_t>::max())) {
		return std::numeric_limits<uint32_t>::max();
	}
	return static_cast<uint32_t>(rounded);
}

}

void StoredCharacter::toCbor(QCborMap& cbor, uint32_t version, bool legacySvgAdvances) const
{
	cbor.insert(VALID_KEY,valid);
	cbor.insert(WIDTH_KEY,width);
	cbor.insert(HEIGHT_KEY,height);
	cbor.insert(BEARING_X_KEY,bearing_x);
	cbor.insert(BEARING_Y_KEY,bearing_y);
	if(version >= CURRENT_VERSION) {
		cbor.insert(ADVANCE_X_KEY, static_cast<double>(advance_x));
		cbor.insert(ADVANCE_Y_KEY, static_cast<double>(advance_y));
	} else if(legacySvgAdvances) {
		// Version 1 SVG glyphs stored plain pixel advances in the integer fields.
		cbor.insert(ADVANCE_X_KEY, encodePlainAdvance(advance_x));
		cbor.insert(ADVANCE_Y_KEY, encodePlainAdvance(advance_y));
	} else {
		// Versions 1 and 2 stored raw 26.6 fixed-point advances.
		cbor.insert(ADVANCE_X_KEY, encodeFixed26_6(advance_x));
		cbor.insert(ADVANCE_Y_KEY, encodeFixed26_6(advance_y));
	}
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

QCborMap StoredCharacter::toCbor(uint32_t version, bool legacySvgAdvances) const
{
	QCborMap cbor;
	toCbor(cbor, version, legacySvgAdvances);
	return cbor;
}

void StoredCharacter::fromCbor(const QCborMap& cbor, uint32_t version, bool legacySvgAdvances)
{
	// Base data
	this->valid = cbor[VALID_KEY].toBool(false);
	this->width = cbor[WIDTH_KEY].toInteger(0);
	this->height = cbor[HEIGHT_KEY].toInteger(0);
	this->bearing_x = cbor[BEARING_X_KEY].toInteger(0);
	this->bearing_y = cbor[BEARING_Y_KEY].toInteger(0);
	if(version >= CURRENT_VERSION) {
		this->advance_x = static_cast<float>(cbor[ADVANCE_X_KEY].toDouble(0.0));
		this->advance_y = static_cast<float>(cbor[ADVANCE_Y_KEY].toDouble(0.0));
	} else if(legacySvgAdvances) {
		this->advance_x = static_cast<float>(cbor[ADVANCE_X_KEY].toInteger(0));
		this->advance_y = static_cast<float>(cbor[ADVANCE_Y_KEY].toInteger(0));
	} else {
		this->advance_x = decodeFixed26_6(static_cast<uint32_t>(cbor[ADVANCE_X_KEY].toInteger(0)));
		this->advance_y = decodeFixed26_6(static_cast<uint32_t>(cbor[ADVANCE_Y_KEY].toInteger(0)));
	}
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

void StoredCharacter::toData(QDataStream& dataStream, uint32_t version, bool legacySvgAdvances) const
{
	dataStream << valid;
	if(valid) {
		dataStream << width << height << bearing_x << bearing_y;
		if(version >= CURRENT_VERSION) {
			dataStream << advance_x << advance_y;
		} else if(legacySvgAdvances) {
			dataStream << encodePlainAdvance(advance_x) << encodePlainAdvance(advance_y);
		} else {
			dataStream << encodeFixed26_6(advance_x) << encodeFixed26_6(advance_y);
		}
		dataStream << metricWidth << metricHeight
				   << horiBearingX << horiBearingY << horiAdvance << vertBearingX << vertBearingY << vertAdvance << static_cast<uint32_t>(sdf.length());
		dataStream.writeRawData(sdf.data(),sdf.length());
	}
}

void StoredCharacter::fromData(QDataStream& dataStream, uint32_t version, bool legacySvgAdvances)
{
	dataStream >> valid;
	if(valid) {
		uint32_t sdf_len;
		dataStream >> width >> height >> bearing_x >> bearing_y;
		if(version >= CURRENT_VERSION) {
			dataStream >> advance_x >> advance_y;
		} else {
			uint32_t raw_x, raw_y;
			dataStream >> raw_x >> raw_y;
			if(legacySvgAdvances) {
				advance_x = static_cast<float>(raw_x);
				advance_y = static_cast<float>(raw_y);
			} else {
				advance_x = decodeFixed26_6(raw_x);
				advance_y = decodeFixed26_6(raw_y);
			}
		}
		dataStream >> metricWidth >> metricHeight
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
