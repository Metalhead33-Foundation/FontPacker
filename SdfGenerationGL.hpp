#ifndef SDFGENERATIONGL_HPP
#define SDFGENERATIONGL_HPP
#include "SdfGenerationContext.hpp"
#include <memory>
#include "GlHelpers.hpp"

class SdfGenerationGL : public SdfGenerationContext
{
private:
		std::unique_ptr<GlHelpers> glHelpers;
		std::unique_ptr<QOpenGLShaderProgram> glShader;
public:
	SdfGenerationGL(const SDFGenerationArguments& args);
	QImage produceSdf(const QImage& source, const SDFGenerationArguments& args) override;
};

#endif // SDFGENERATIONGL_HPP
