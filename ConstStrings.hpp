/**
 * @file ConstStrings.hpp
 * @brief String constants for the argument map and the CBOR serialization.
 *
 * All keys used by the command line / GUI argument map and by the CBOR
 * representation are declared here, so that producers and consumers cannot
 * drift apart. The binary `.wodf`/`.wodi` formats do not use these strings.
 *
 * @note The CBOR key for `PreprocessedFontFace::bitmap_size` is the legacy
 *       string `"dimensions"`, not `"bitmap_size"`.
 */

#ifndef CONSTSTRINGS_HPP
#define CONSTSTRINGS_HPP
#include <QString>

/** @name Glyph and container CBOR keys
 *  @{ */
extern const QString VALID_KEY;                      ///< `"valid"` - glyph validity flag.
extern const QString VERSION_KEY;                    ///< `"version"` - container format version.
extern const QString CBOR_CONTAINER_TYPE_KEY;        ///< `"containerType"` - container discriminator.
extern const QString CBOR_PREPROCESSED_FONT_FACE_TYPE; ///< `"PreprocessedFontFace"` - font discriminator value.
extern const QString CBOR_STORED_VECTOR_IMAGE_TYPE;  ///< `"StoredVectorImage"` - vector-image discriminator value.
extern const QString WIDTH_KEY;                      ///< `"width"` - glyph box width in pixels.
extern const QString HEIGHT_KEY;                     ///< `"height"` - glyph box height in pixels.
extern const QString BEARING_X_KEY;                  ///< `"bearing_x"` - left bearing in pixels.
extern const QString BEARING_Y_KEY;                  ///< `"bearing_y"` - top bearing in pixels.
extern const QString ADVANCE_X_KEY;                  ///< `"advance_x"` - horizontal advance (26.6 in binary; CBOR stores the raw integer).
extern const QString ADVANCE_Y_KEY;                  ///< `"advance_y"` - vertical advance (26.6 in binary).
extern const QString SDF_KEY;                        ///< `"sdf"` - encoded glyph image bytes.
/** @} */

/** @name CLI argument keys and values
 *  @{ */
extern const QString MODE_KEY;                       ///< `"mode"` - backend selection.
extern const QString TYPE_KEY;                       ///< `"type"` - SDF flavour selection.
extern const QString DIST_KEY;                       ///< `"dist"` - distance norm selection.
extern const QString SOFTWARE_MODE_KEY;              ///< `"Software"` - CPU backend name.
extern const QString OPENGL_MODE_KEY;                ///< `"OpenGL"` - compute-shader backend name.
extern const QString OPENCL_MODE_KEY;                ///< `"OpenCL"` - unsupported backend name.
extern const QString SDF_MODE_KEY;                   ///< `"SDF"` - single-channel flavour name.
extern const QString MSDF_MODE_KEY;                  ///< `"MSDF"` - multi-channel flavour name.
extern const QString MSDFA_MODE_KEY;                 ///< `"MSDFA"` - multi-channel with alpha flavour name.
extern const QString MANHATTAN_MODE_KEY;             ///< `"Manhattan"` - L1 norm name.
extern const QString EUCLIDEAN_MODE_KEY;             ///< `"Euclidean"` - L2 norm name.
extern const QString INTERNAL_PROCESS_SIZE_KEY;      ///< `"internalprocesssize"` - `L`, the generation size.
extern const QString INTENDED_SIZE_KEY;              ///< `"intendedsize"` - `S`, the stored size; 0 disables downsampling.
extern const QString PADDING_KEY;                    ///< `"padding"` - `P`, the border inside `L`.
extern const QString SAMPLES_TO_CHECK_X_KEY;         ///< `"samplestocheckx"` - SDF search radius in X; 0 uses the padding.
extern const QString SAMPLES_TO_CHECK_Y_KEY;         ///< `"samplestochecky"` - SDF search radius in Y; 0 uses the padding.
extern const QString CHAR_MIN_KEY;                   ///< `"mincharcode"` - first processed code point (inclusive).
extern const QString CHAR_MAX_KEY;                   ///< `"maxcharcode"` - one past the last processed code point (exclusive).
extern const QString IN_FONT_KEY;                    ///< `"infont"` - input font path.
extern const QString IN_BIN_KEY;                     ///< `"inbin"` - input `.wodf` path.
extern const QString IN_CBOR_KEY;                    ///< `"incbor"` - input CBOR font path.
extern const QString IN_VECTOR_BIN_KEY;              ///< `"invectorbin"` - input `.wodi` path.
extern const QString IN_VECTOR_CBOR_KEY;             ///< `"invectorcbor"` - input CBOR vector-image path.
extern const QString IN_SVG_KEY;                     ///< `"insvg"` - input SVG path.
extern const QString OUT_FONT_KEY;                   ///< `"outfont"` - per-glyph file pattern.
extern const QString OUT_BIN_KEY;                    ///< `"outbin"` - output `.wodf` path.
extern const QString OUT_CBOR_KEY;                   ///< `"outcbor"` - output CBOR font path.
extern const QString OUT_VECTOR_BIN_KEY;             ///< `"outvectorbin"` - output `.wodi` path.
extern const QString OUT_VECTOR_CBOR_KEY;            ///< `"outvectorcbor"` - output CBOR vector-image path.
extern const QString DEFAULT_FONT_PATH;              ///< Default `--infont` value used when none is supplied.
/** @} */

/** @name PreprocessedFontFace CBOR keys
 *  @{ */
extern const QString BITMAP_SIZE_KEY;                ///< `"dimensions"` - legacy CBOR key for `bitmap_size` (`S`).
extern const QString BITMAP_LOGICAL_SIZE_KEY;        ///< `"logical_size"` - CBOR key for `bitmap_logical_size` (`L`).
extern const QString GLYPHS_KEY;                     ///< `"glyphs"` - map of code point to glyph map.
extern const QString FONT_NAME_KEY;                  ///< `"fontFamily"` - family name string.
extern const QString HORIBEARINGX_KEY;               ///< `"horiBearingX"` - float horizontal bearing.
extern const QString HORIBEARINGY_KEY;               ///< `"horiBearingY"` - float vertical bearing.
extern const QString HORIADVANCE_KEY;                ///< `"horiAdvance"` - float horizontal advance.
extern const QString VERTBEARINGX_KEY;               ///< `"vertBearingX"` - float vertical-layout bearing X.
extern const QString VERTBEARINGY_KEY;               ///< `"vertBearingY"` - float vertical-layout bearing Y.
extern const QString VERTADVANCE_KEY;                ///< `"vertAdvance"` - float vertical advance.
extern const QString HASVERTICAL_KEY;                ///< `"hasVertical"` - legacy key, not written by the current serializer.
extern const QString LINEARHORIADVANCE_KEY;          ///< `"linearHoriAdvance"` - reserved legacy key.
extern const QString LINEARVERTADVANCE_KEY;          ///< `"linearVertAdvance"` - reserved legacy key.
extern const QString METRICSWIDTH_KEY;               ///< `"metricsWidth"` - float metric-box width.
extern const QString METRICSHEIGHT_KEY;              ///< `"metricsHeight"` - float metric-box height.
extern const QString HAS_VERT_KEY;                   ///< `"hasVert"` - whether vertical metrics are valid.
extern const QString ASCENDER_KEY;                   ///< `"ascender"` - face ascender in pixels.
extern const QString DESCENDER_KEY;                  ///< `"descender"` - face descender in pixels.
extern const QString FACE_HEIGHT_KEY;                ///< `"faceHeight"` - baseline-to-baseline distance in pixels.
extern const QString MAX_ADVANCE_KEY;                ///< `"maxAdvance"` - maximum advance in pixels.
extern const QString UNITS_PER_EM_KEY;               ///< `"unitsPerEm"` - original design units per EM.
extern const QString DISTANCE_RANGE_X_KEY;           ///< `"distanceRangeX"` - nominal encoded SDF radius in level-0 texels (X).
extern const QString DISTANCE_RANGE_Y_KEY;           ///< `"distanceRangeY"` - nominal encoded SDF radius in level-0 texels (Y).
extern const QString ENCODING_FLAGS_KEY;             ///< `"encodingFlags"` - bitmask of `StoredVectorImage::EncodingFlag` values baked into the pixels.
extern const QString CBOR_MIDPOINT_ADJUSTMENT_KEY;   ///< `"midpointAdjustment"` - baked midpoint divisor (CBOR spelling; the CLI spelling is lower-case).
extern const QString KERNING_KEY;                    ///< `"kerning"` - nested kerning map.
extern const QString IMAGE_FORMAT_KEY;               ///< `"format"` - encoded image codec name.
/** @} */

/** @name Generation flags and adjustments
 *  @{ */
extern const QString FORCE_RASTER_KEY;               ///< `"forceraster"` - use the bitmap path instead of outlines.
extern const QString GAMMA_CORRECT_KEY;              ///< `"gammacorrect"` - apply the cubic gamma adjustment.
extern const QString MIDPOINT_ADJUSTMENT_KEY;        ///< `"midpointadjustment"` - bake a midpoint divisor.
extern const QString MAXIMIZE_INSTEAD_OF_AVERAGE_KEY; ///< `"maximizeinsteadofaverage"` - max-downsampling.
extern const QString INVERT_KEY;                     ///< `"invert"` - invert the distance values.
extern const QString MSDFGEN_COLOURING;              ///< `"msdfgencoloring"` - msdfgen-style edge colouring.
extern const QString CREATE_MIPMAPS_KEY;             ///< `"createmipmaps"` - store all mip levels in a `.wodi`.
/** @} */

#endif // CONSTSTRINGS_HPP
