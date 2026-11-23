/**
 * @file SDFGenerationArguments.hpp
 * @brief Configuration for one SDF generation run, plus the serialized enums.
 *
 * SDFGenerationArguments is filled from the command line (`SDFGenerationArguments::fromArgs`)
 * or by the GUI, and is consumed by the SdfGenerationContext implementations.
 * SDFType and DistanceType are also the values stored in `.wodf`/`.wodi`.
 */

#ifndef SDFGENERATIONARGUMENTS_HPP
#define SDFGENERATIONARGUMENTS_HPP
#include <cstdint>
#include <memory>
#include <QVariant>
#include <QByteArray>
#include <optional>

/**
 * @brief Backend used to evaluate the distance field.
 * @enum SDfGenerationMode
 *
 * This value is a runtime choice and is never serialized.
 */
enum SDfGenerationMode {
	SOFTWARE,        ///< CPU implementation (single-channel bitmap SDF only at present).
	OPENGL_COMPUTE,  ///< OpenGL 4.3+ compute-shader implementation (outline and bitmap).
	OPENCL           ///< Declared, not implemented; selecting it throws.
};

/**
 * @brief Distance-field flavour, serialized as the `type` byte.
 * @enum SDFType
 */
enum SDFType {
	SDF,    ///< Single-channel signed distance field (8-bit grayscale image).
	MSDF,   ///< Multi-channel signed distance field (RGB image).
	MSDFA   ///< Multi-channel signed distance field with alpha (RGBA image).
};

/**
 * @brief Distance norm, serialized as the `distType` byte.
 * @enum DistanceType
 */
enum DistanceType {
	Manhattan,  ///< Manhattan distance, L1 norm: abs(dx) + abs(dy).
	Euclidean   ///< Euclidean distance, L2 norm: sqrt(dx*dx + dy*dy).
};

/**
 * @brief How SVG shapes are grouped into output glyphs.
 * @enum SvgTreatment
 */
enum SvgTreatment {
	SeparateShapes,  ///< One output glyph per SVG shape (font-face output only).
	ShapesAllInOne   ///< All shapes are merged into one output glyph.
};

/**
 * @brief All parameters of a generation run.
 * @struct SDFGenerationArguments
 *
 * Geometry invariants:
 * - `internalProcessSize` is `L` and `intendedSize` is `S`; both describe square
 *   images. `intendedSize == 0` disables downsampling, so the stored images are
 *   `L x L`; the writer records `bitmap_size = L` rather than 0.
 * - `padding` is `P`, expressed in pixels of the `L` grid and included in `L`.
 *   The drawable content box is `L - 2P`; `2 * padding < internalProcessSize`
 *   must hold, and `samples_to_check_*` (when non-zero) replaces `padding` as
 *   the SDF search radius without changing the geometry.
 * - FreeType is configured at `em = internalProcessSize - padding`, so the
 *   metrics stored in StoredCharacter are in that pixel space.
 *
 * @see BINARY_FORMAT.md, SdfGenerationContext
 */
struct SDFGenerationArguments {
	SDfGenerationMode mode = SOFTWARE;            ///< Backend to use; `Software` by default, `OpenCL` throws.
	SvgTreatment svgTreatment = ShapesAllInOne;   ///< Grouping of SVG shapes into output glyphs.
	SDFType type = SDF;                           ///< Distance-field flavour to generate.
	DistanceType distType = Manhattan;            ///< Distance norm; the CLI default is Manhattan for Software and Euclidean for OpenGL.
	uint32_t internalProcessSize;                 ///< `L`: square generation resolution, e.g. 1024 (4096 with HIRES).
	uint32_t intendedSize;                        ///< `S`: square stored resolution; `0` = no downsampling.
	uint32_t padding;                             ///< `P`: border on all four sides, in pixels of the `L` grid; must satisfy 2P < L.
	uint32_t samples_to_check_x;                  ///< SDF search radius in X; `0` = use `padding`. The effective half-radius is this value divided by 2.
	uint32_t samples_to_check_y;                  ///< SDF search radius in Y; `0` = use `padding`. The effective half-radius is this value divided by 2.
	QString font_path;                            ///< Input font path (used by `processFont`).
	uint32_t char_min;                            ///< First Unicode code point of the processed range (inclusive).
	uint32_t char_max;                            ///< One past the last processed code point (the loop is `c < char_max`).
	bool msdfgenColouring;                        ///< Use the msdfgen-style edge colour assignment for MSDF.
	bool invert;                                  ///< Invert the final distance values (`1 - x`).
	QByteArray imageFormat;                       ///< Encoded image codec passed to QImage::save (upper-cased); stored in the container's 8-byte field.
	bool forceRaster;                             ///< Rasterise glyphs instead of using the outline path; required for Software mode.
bool gammaCorrect;                            ///< Apply the cubic gamma adjustment around 0.5.
bool maximizeInsteadOfAverage;                ///< Use max instead of average when downsampling.
std::optional<float> midpointAdjustment;      ///< Optional divisor baked into the distance values; must be positive when present.
bool createMipmaps;                           ///< Store every halving level; only used for StoredVectorImage output.

/**
 * @brief Parse arguments from a QVariantMap (typically from the command line or UI).
 *
 * Unknown/missing keys fall back to the defaults documented above. Flag keys
 * are tested with `contains()`; value keys are parsed as strings, integers or
 * floats, and the textual enumerations are matched case-insensitively.
 * @param args Map of argument keys to values.
 */
void fromArgs(const QVariantMap& args);
};

#endif // SDFGENERATIONARGUMENTS_HPP
