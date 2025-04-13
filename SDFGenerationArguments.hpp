#ifndef SDFGENERATIONARGUMENTS_HPP
#define SDFGENERATIONARGUMENTS_HPP
#include <cstdint>
#include <memory>
#include <QVariant>
#include <optional>

enum SDfGenerationMode {
	SOFTWARE,
	OPENGL_COMPUTE,
	OPENCL
};
enum SDFType {
	SDF,
	MSDF,
	MSDFA
};
enum DistanceType {
	Manhattan,
	Euclidean
};

struct SDFGenerationArguments {
	SDfGenerationMode mode = SOFTWARE;
	SDFType type = SDF;
	DistanceType distType = Manhattan;
	uint32_t internalProcessSize;
	uint32_t intendedSize;
	uint32_t padding;
	uint32_t samples_to_check_x;
	uint32_t samples_to_check_y;
	QString font_path;
	uint32_t char_min;
	uint32_t char_max;
	bool invert;
	bool jpeg;
	bool forceRaster;
	bool gammaCorrect;
	bool maximizeInsteadOfAverage;
	std::optional<float> midpointAdjustment;
	void fromArgs(const QVariantMap& args);
};

#endif // SDFGENERATIONARGUMENTS_HPP
