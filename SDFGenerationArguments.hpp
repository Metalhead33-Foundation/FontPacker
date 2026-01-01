/**
 * @file SDFGenerationArguments.hpp
 * @brief Configuration arguments and enumerations for SDF generation.
 */

#ifndef SDFGENERATIONARGUMENTS_HPP
#define SDFGENERATIONARGUMENTS_HPP
#include <cstdint>
#include <memory>
#include <QVariant>
#include <optional>

/**
 * @brief Generation mode for SDF computation.
 * @enum SDfGenerationMode
 */
enum SDfGenerationMode {
	SOFTWARE,        ///< CPU-based software rendering
	OPENGL_COMPUTE,  ///< GPU-based OpenGL compute shader rendering
	OPENCL           ///< OpenCL-based rendering (not currently implemented)
};

/**
 * @brief Type of signed distance field to generate.
 * @enum SDFType
 */
enum SDFType {
	SDF,    ///< Standard single-channel signed distance field
	MSDF,   ///< Multi-channel signed distance field (RGB channels)
	MSDFA   ///< Multi-channel signed distance field with alpha channel (RGBA)
};

/**
 * @brief Distance calculation method for SDF generation.
 * @enum DistanceType
 */
enum DistanceType {
	Manhattan,  ///< Manhattan distance (L1 norm: |x| + |y|)
	Euclidean   ///< Euclidean distance (L2 norm: √(x² + y²))
};

/**
 * @brief Treatment of SVG shapes during processing.
 * @enum SvgTreatment
 */
enum SvgTreatment {
	SeparateShapes,  ///< Process each shape separately
	ShapesAllInOne   ///< Combine all shapes into a single glyph
};

/**
 * @brief Configuration structure for SDF generation parameters.
 * 
 * Contains all settings needed to generate signed distance fields from fonts
 * or SVG files, including rendering mode, SDF type, dimensions, and various
 * processing options.
 */
struct SDFGenerationArguments {
	SDfGenerationMode mode = SOFTWARE;              ///< Rendering mode (software/OpenGL/OpenCL)
	SvgTreatment svgTreatment = ShapesAllInOne;   ///< How to treat SVG shapes
	SDFType type = SDF;                            ///< Type of SDF to generate
	DistanceType distType = Manhattan;            ///< Distance calculation method
	uint32_t internalProcessSize;                 ///< Internal processing resolution (usually higher than intended)
	uint32_t intendedSize;                        ///< Final output size in pixels
	uint32_t padding;                             ///< Padding around glyphs in pixels
	uint32_t samples_to_check_x;                  ///< Number of samples to check in X direction for distance calculation
	uint32_t samples_to_check_y;                  ///< Number of samples to check in Y direction for distance calculation
	QString font_path;                            ///< Path to font file
	uint32_t char_min;                            ///< Minimum Unicode code point to process
	uint32_t char_max;                            ///< Maximum Unicode code point to process
	bool msdfgenColouring;                        ///< Use msdfgen-style edge coloring algorithm
	bool invert;                                  ///< Invert the SDF (inside becomes outside)
	bool jpeg;                                    ///< Compress SDF data as JPEG
	bool forceRaster;                             ///< Force rasterization instead of vector processing
	bool gammaCorrect;                            ///< Apply gamma correction
	bool maximizeInsteadOfAverage;                ///< Use maximum instead of average when downsampling
	std::optional<float> midpointAdjustment;     ///< Optional adjustment to SDF midpoint threshold
	
	/**
	 * @brief Parse arguments from a QVariantMap (typically from command-line or UI).
	 * @param args Map of argument keys to values.
	 */
	void fromArgs(const QVariantMap& args);
};

#endif // SDFGENERATIONARGUMENTS_HPP
