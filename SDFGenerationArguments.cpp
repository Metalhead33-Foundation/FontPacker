#include "SDFGenerationArguments.hpp"
#include "ConstStrings.hpp"

#ifdef HIRES
const unsigned INTERNAL_RENDER_SIZE = 4096;
const unsigned PADDING = 400;
#else
const unsigned INTERNAL_RENDER_SIZE = 1024;
const unsigned PADDING = 100;
#endif
const unsigned INTENDED_SIZE = 32;

/*
extern const QString GAMMA_CORRECT_KEY;
extern const QString MIDPOINT_ADJUSTMENT_KEY;
extern const QString MAXIMIZE_INSTEAD_OF_AVERAGE_KEY;
*/

void SDFGenerationArguments::fromArgs(const QVariantMap& args)
{
	this->invert = args.contains(INVERT_KEY);
	this->jpeg = args.contains(JPEG_KEY);
	this->forceRaster = args.contains(FORCE_RASTER_KEY);
	this->gammaCorrect = args.contains(GAMMA_CORRECT_KEY);
	this->maximizeInsteadOfAverage = args.contains(MAXIMIZE_INSTEAD_OF_AVERAGE_KEY);
	this->internalProcessSize = args.value(INTERNAL_PROCESS_SIZE_KEY, INTERNAL_RENDER_SIZE).toUInt();
	this->intendedSize = args.value(INTENDED_SIZE_KEY, 0).toUInt();
	this->padding = args.value(PADDING_KEY, PADDING).toUInt();
	this->samples_to_check_x = args.value(SAMPLES_TO_CHECK_X_KEY, 0).toUInt();
	this->samples_to_check_y = args.value(SAMPLES_TO_CHECK_Y_KEY, 0).toUInt();
	this->char_min = args.value(CHAR_MIN_KEY, 0).toUInt();
	this->char_max = args.value(CHAR_MAX_KEY, 0xE007F).toUInt();
	this->font_path = args.value(IN_FONT_KEY, DEFAULT_FONT_PATH).toString();
	// Midpoint bias
	if(args.contains(MIDPOINT_ADJUSTMENT_KEY)) {
		QVariant midpointAdj = args.value(MIDPOINT_ADJUSTMENT_KEY);
		switch (midpointAdj.typeId() ) {
			case QMetaType::QString: {
				QString midpointAdjStr = midpointAdj.toString();
				this->midpointAdjustment = midpointAdjStr.toFloat();
				break;
			}
			case QMetaType::Int: {
				this->midpointAdjustment = static_cast<float>(midpointAdj.toInt());
				break;
			}
			case QMetaType::UInt: {
				this->midpointAdjustment = static_cast<float>(midpointAdj.toUInt());
				break;
			}
			case QMetaType::Float: {
				this->midpointAdjustment = midpointAdj.toFloat();
				break;
			}
			case QMetaType::Double: {
				this->midpointAdjustment = static_cast<float>(midpointAdj.toDouble());
				break;
			}
			default: {
				this->midpointAdjustment = {};
				break;
			}
			}
	} else {
		this->midpointAdjustment = {};
	}

	// Mode
	{
		QVariant genMod = args.value(MODE_KEY);
		switch (genMod.typeId() ) {
			case QMetaType::QString: {
				QString genModS = genMod.toString();
				if(!genModS.compare(SOFTWARE_MODE_KEY,Qt::CaseInsensitive)) this->mode = SDfGenerationMode::SOFTWARE;
				else if(!genModS.compare(OPENGL_MODE_KEY,Qt::CaseInsensitive)) this->mode = SDfGenerationMode::OPENGL_COMPUTE;
				else if(!genModS.compare(OPENCL_MODE_KEY,Qt::CaseInsensitive)) this->mode = SDfGenerationMode::OPENCL;
				else this->mode = SDfGenerationMode::SOFTWARE;
				break;
			}
			case QMetaType::Int: {
				this->mode = static_cast<SDfGenerationMode>(genMod.toInt());
				break;
			}
			case QMetaType::UInt: {
				this->mode = static_cast<SDfGenerationMode>(genMod.toUInt());
				break;
			}
			default: {
				this->mode = SDfGenerationMode::SOFTWARE;
				break;
			}
		}
	}
	
	
	// Type
	{
		QVariant sdfType = args.value(TYPE_KEY);
		switch (sdfType.typeId() ) {
			case QMetaType::QString: {
				QString sdfTypeS = sdfType.toString();
				if(!sdfTypeS.compare(SDF_MODE_KEY,Qt::CaseInsensitive)) this->type = SDFType::SDF;
				else if(!sdfTypeS.compare(MSDF_MODE_KEY,Qt::CaseInsensitive)) this->type = SDFType::MSDF;
				else if(!sdfTypeS.compare(MSDFA_MODE_KEY,Qt::CaseInsensitive)) this->type = SDFType::MSDFA;
				else this->type = SDFType::SDF;
				break;
			}
			case QMetaType::Int: {
				this->type = static_cast<SDFType>(sdfType.toInt());
				break;
			}
			case QMetaType::UInt: {
				this->type = static_cast<SDFType>(sdfType.toUInt());
				break;
			}
			default: {
				this->type = SDFType::SDF;
				break;
			}
		}
	}
	
	// Dist
	{
		QVariant distType = args.value(DIST_KEY);
		switch (distType.typeId() ) {
			case QMetaType::QString: {
				QString distTypeS = distType.toString();
				if(!distTypeS.compare(MANHATTAN_MODE_KEY,Qt::CaseInsensitive)) this->distType = DistanceType::Manhattan;
				else if(!distTypeS.compare(EUCLIDEAN_MODE_KEY,Qt::CaseInsensitive)) this->distType = DistanceType::Euclidean;
				else this->distType = DistanceType::Manhattan;
				break;
			}
			case QMetaType::Int: {
				this->distType = static_cast<DistanceType>(distType.toInt());
				break;
			}
			case QMetaType::UInt: {
				this->distType = static_cast<DistanceType>(distType.toUInt());
				break;
			}
			default: {
				this->distType = mode == SDfGenerationMode::SOFTWARE ? DistanceType::Manhattan : DistanceType::Euclidean;
				break;
			}
		}
	}
}
