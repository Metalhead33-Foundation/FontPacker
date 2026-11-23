/**
 * @file SdfGenerationOpenCL.hpp
 * @brief OpenCL 1.2 buffer-based implementation of SDF generation.
 *
 * This backend mirrors the algorithms of SdfGenerationGL (bitmap and outline
 * SDF, plus outline MSDF/MSDFA) but uses OpenCL 1.2 kernels and ordinary
 * buffers, so it needs no image or GL-sharing extensions. The kernels are
 * compiled with the fast-math options requested for this backend; outputs are
 * expected to be numerically close to, not bit-identical with, the OpenGL
 * backend.
 */

#ifndef SDFGENERATIONOPENCL_HPP
#define SDFGENERATIONOPENCL_HPP

#include "SdfGenerationContext.hpp"
#include <memory>

/**
 * @brief OpenCL 1.2 SDF generation context.
 *
 * @note The kernel source is embedded as the `sdf_opencl.cl` Qt resource in
 *       FontPackerCore. Executables that use this class must call
 *       `Q_INIT_RESOURCE(resources)` before constructing it.
 */
class SdfGenerationOpenCL : public SdfGenerationContext
{
private:
	struct Impl; ///< OpenCL runtime state, hidden from this header.
	std::unique_ptr<Impl> impl; ///< Platform, context, queue, program and kernels.

public:
	/**
	 * @brief Create an OpenCL context and compile the kernels.
	 * @param args Generation arguments selecting the distance metric.
	 * @throws std::runtime_error if no OpenCL device exists or compilation fails.
	 */
	explicit SdfGenerationOpenCL(const SDFGenerationArguments& args);

	/// @brief Release the OpenCL resources.
	~SdfGenerationOpenCL() override;

	/**
	 * @brief Generate an SDF from a raster bitmap using OpenCL.
	 *
	 * The bitmap path supports SDF only; MSDF/MSDFA require vector outlines.
	 * @param source Source grayscale bitmap (the padded L x L image).
	 * @param args Generation arguments.
	 * @return Normalized/quantized SDF image.
	 */
	QImage produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args) override;

	/**
	 * @brief Generate an SDF/MSDF from decomposed outline edges using OpenCL.
	 * @param source Decomposed outline context.
	 * @param args Generation arguments.
	 * @return Normalized/quantized SDF or MSDF image.
	 */
	QImage produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args) override;
};

#endif // SDFGENERATIONOPENCL_HPP
