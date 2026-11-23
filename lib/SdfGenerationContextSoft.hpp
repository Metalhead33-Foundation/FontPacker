/**
 * @file SdfGenerationContextSoft.hpp
 * @brief CPU-based software implementation of SDF generation.
 * 
 * This class implements SDF generation using CPU-based algorithms,
 * suitable for systems without GPU support or for smaller workloads.
 */

#ifndef SDFGENERATIONCONTEXTSOFT_HPP
#define SDFGENERATIONCONTEXTSOFT_HPP

#include "SdfGenerationContext.hpp"

/**
 * @brief Software-based SDF generation context.
 * 
 * Implements SDF generation using CPU algorithms. Uses distance field
 * calculations with configurable distance metrics (Manhattan or Euclidean).
 *
 * @warning Only produceBitmapSdf() is implemented. produceOutlineSdf() has an
 *          empty body in the current tree, so fonts with vector outlines and
 *          SVG input require the OpenGL backend; use `--forceraster` to route
 *          TrueType/OpenType glyphs through the bitmap path. This backend also
 *          emits single-channel 8-bit SDF images only, regardless of `type`.
 */
class SdfGenerationContextSoft : public SdfGenerationContext
{
public:
	/**
	 * @brief Constructor.
	 */
	SdfGenerationContextSoft();
	
	/**
	 * @brief Generate SDF from a bitmap image using CPU algorithms.
	 * @param source Source bitmap image.
	 * @param args Generation arguments.
	 * @return Generated SDF image.
	 */
	QImage produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args) override;
	
	/**
	 * @brief Generate SDF from font outline decomposition using CPU algorithms.
	 * @warning Not implemented in the current tree (empty body); calling it is
	 *          undefined behaviour. Use the OpenGL backend for outlines.
	 * @param source Font outline decomposition context.
	 * @param args Generation arguments.
	 * @return Generated SDF image.
	 */
	QImage produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args) override;
};

#endif // SDFGENERATIONCONTEXTSOFT_HPP
