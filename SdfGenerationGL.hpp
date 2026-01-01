/**
 * @file SdfGenerationGL.hpp
 * @brief OpenGL compute shader-based GPU implementation of SDF generation.
 * 
 * This class implements SDF generation using OpenGL 4.3+ compute shaders,
 * providing high-performance GPU-accelerated rendering.
 */

#ifndef SDFGENERATIONGL_HPP
#define SDFGENERATIONGL_HPP
#include "SdfGenerationContext.hpp"
#include <memory>
#include "GlHelpers.hpp"

#if defined (_MSC_VER)
#define STD140 __declspec(align(16))
#elif defined(__GNUC__) || defined(__clang__)
#define STD140 __attribute__((aligned(16)))
#else
#define STD140
#endif

/**
 * @brief Uniform buffer structure for compute shader (std140 layout).
 * @struct UniformForCompute
 */
struct STD140 UniformForCompute {
	int32_t width, height;  ///< Texture dimensions
	int32_t padding[2];     ///< Padding for std140 alignment
};

/**
 * @brief OpenGL compute shader-based SDF generation context.
 * 
 * Implements SDF generation using GPU compute shaders for high performance.
 * Requires OpenGL 4.3+ with compute shader support. Uses multiple compute
 * shader passes for different SDF types (SDF, MSDF, MSDFA).
 */
class SdfGenerationGL : public SdfGenerationContext
{
private:
	/**
	 * @brief Get the final image format based on SDF type.
	 * @param args Generation arguments.
	 * @return QImage format for the output.
	 */
	static QImage::Format getFinalImageFormat(const SDFGenerationArguments& args);
	
	/**
	 * @brief Get the temporary texture format for intermediate processing.
	 * @param args Generation arguments.
	 * @return OpenGL texture format.
	 */
	static GlTextureFormat getTemporaryTextureFormat(const SDFGenerationArguments& args);
	
	GlHelpers glHelpers;                                    ///< OpenGL helper functions and context
	QImage::Format finalImageFormat;                        ///< Format for final output image
	GlTextureFormat temporaryTextureFormat;                 ///< Format for intermediate textures
	std::unique_ptr<QOpenGLShaderProgram> glShader;         ///< Primary compute shader program
	std::unique_ptr<QOpenGLShaderProgram> glShader2;        ///< Secondary compute shader program
	std::unique_ptr<QOpenGLShaderProgram> msdfFixerShader;  ///< MSDF edge fixing shader program
	UniformForCompute uniform;                              ///< Uniform buffer data
	GlTexture oldTex;                                       ///< Previous iteration texture
	GlTexture newTex;                                      ///< Current iteration texture
	GlTexture newTex2;                                     ///< Secondary texture buffer
	GlTexture newTex3;                                     ///< Tertiary texture buffer
	GlStorageBuffer uniformBuffer;                          ///< Uniform buffer object
	GlStorageBuffer ssboForEdges;                          ///< Shader storage buffer for edge data

	int fontUniform;          ///< Font texture uniform location
	int sdfUniform1;          ///< First SDF texture uniform location
	int sdfUniform2;          ///< Second SDF texture uniform location
	int dimensionsUniform;    ///< Dimensions uniform location
	
	/**
	 * @brief Fetch SDF result from GPU and convert to QImage.
	 * @param newimg Output image to populate.
	 * @param args Generation arguments.
	 */
	void fetchSdfFromGPU(QImage& newimg, const SDFGenerationArguments& args);
	
	/**
	 * @brief Fetch MSDF result from GPU and convert to QImage.
	 * @param newimg Output image to populate.
	 * @param args Generation arguments.
	 */
	void fetchMSDFFromGPU(QImage& newimg, const SDFGenerationArguments& args);

	int sdfUniform1_vec;       ///< First SDF texture uniform location (vector version)
	int sdfUniform2_vec;      ///< Second SDF texture uniform location (vector version)
	int ssboUniform_vec;       ///< SSBO uniform location (vector version)
	int dimensionsUniform_vec; ///< Dimensions uniform location (vector version)

	int fixer_tex_uniform1;   ///< First texture uniform for MSDF fixer shader
	int fixer_tex_uniform2;   ///< Second texture uniform for MSDF fixer shader
	
public:
	/**
	 * @brief Constructor - initializes OpenGL context and shaders.
	 * @param args Generation arguments (used to determine shader selection).
	 */
	SdfGenerationGL(const SDFGenerationArguments& args);
	
	/**
	 * @brief Generate SDF from a bitmap image using GPU compute shaders.
	 * @param source Source bitmap image.
	 * @param args Generation arguments.
	 * @return Generated SDF image.
	 */
	QImage produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args) override;
	
	/**
	 * @brief Generate SDF from font outline decomposition using GPU compute shaders.
	 * @param source Font outline decomposition context.
	 * @param args Generation arguments.
	 * @return Generated SDF image.
	 */
	QImage produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args) override;
};

#endif // SDFGENERATIONGL_HPP
