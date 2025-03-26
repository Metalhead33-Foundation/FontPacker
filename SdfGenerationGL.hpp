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

struct STD140 UniformForCompute {
	int32_t width, height;
	int32_t padding[2];
};

class SdfGenerationGL : public SdfGenerationContext
{
private:
		static QImage::Format getFinalImageFormat(const SDFGenerationArguments& args);
		static GlTextureFormat getTemporaryTextureFormat(const SDFGenerationArguments& args);
		GlHelpers glHelpers;
		QImage::Format finalImageFormat;
		GlTextureFormat temporaryTextureFormat;
		std::unique_ptr<QOpenGLShaderProgram> glShader;
		std::unique_ptr<QOpenGLShaderProgram> glShader2;
		UniformForCompute uniform;
		GlTexture oldTex;
		GlTexture newTex;
		GlTexture newTex2;
		GlStorageBuffer uniformBuffer;
		GlStorageBuffer ssboForEdges;

		int fontUniform;
		int sdfUniform1;
		int sdfUniform2;
		int dimensionsUniform;
		void fetchSdfFromGPU(QImage& newimg);
		void fetchMSDFFromGPU(QImage& newimg);

		int sdfUniform1_vec;
		int sdfUniform2_vec;
		int ssboUniform_vec;
		int dimensionsUniform_vec;
public:
	SdfGenerationGL(const SDFGenerationArguments& args);
	QImage produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args) override;
	QImage produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args) override;
};

#endif // SDFGENERATIONGL_HPP
