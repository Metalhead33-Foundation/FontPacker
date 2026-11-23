#include <catch2/catch_test_macros.hpp>

#include "GlHelpers.hpp"
#include "SdfGenerationGL.hpp"

#include <QFile>
#include <QImage>
#include <QOpenGLShaderProgram>
#include <QString>
#include <QtGlobal>

#include <exception>
#include <memory>

namespace {

QByteArray readShader(const QString& path)
{
	QFile file(path);
	REQUIRE(file.open(QFile::ReadOnly));
	return file.readAll();
}

QByteArray withManhattanDefine(QByteArray source)
{
	const int versionPos = source.indexOf("#version");
	REQUIRE(versionPos >= 0);
	const int lineEnd = source.indexOf('\n', versionPos);
	REQUIRE(lineEnd >= 0);
	source.insert(lineEnd + 1, QByteArrayLiteral("#define USE_MANHATTAN_DISTANCE\n"));
	return source;
}

void compileShader(GlHelpers& gl, const QString& path, QOpenGLShader::ShaderType type, bool manhattan = false)
{
	(void)gl;
	QByteArray source = readShader(path);
	if(manhattan) source = withManhattanDefine(source);
	QOpenGLShaderProgram program;
	INFO(path.toStdString());
	REQUIRE(program.create());
	REQUIRE(program.addShaderFromSourceCode(type, source));
	REQUIRE(program.link());
}

SDFGenerationArguments makeArgs(DistanceType distance, SDFType type = SDF)
{
	SDFGenerationArguments args;
	args.distType = distance;
	args.type = type;
	args.internalProcessSize = 32;
	args.intendedSize = 0;
	args.padding = 2;
	args.samples_to_check_x = 0;
	args.samples_to_check_y = 0;
	args.char_min = 0;
	args.char_max = 1;
	args.msdfgenColouring = false;
	args.invert = false;
	args.imageFormat = QByteArrayLiteral("PNG");
	args.forceRaster = false;
	args.gammaCorrect = false;
	args.maximizeInsteadOfAverage = false;
	args.midpointAdjustment = std::nullopt;
	args.createMipmaps = false;
	return args;
}

}

TEST_CASE("Every GLSL shader source compiles", "[glsl]")
{
	std::unique_ptr<GlHelpers> gl;
	try {
		gl = std::make_unique<GlHelpers>();
	} catch(const std::exception& error) {
		SKIP("OpenGL 4.3 context is not available: " << error.what());
	}

	const struct {
		const char* path;
		QOpenGLShader::ShaderType type;
		bool manhattan;
	} shaders[] = {
		{":/shader1.glsl", QOpenGLShader::Compute, false},
		{":/shader1.glsl", QOpenGLShader::Compute, true},
		{":/shader_msdf1.glsl", QOpenGLShader::Compute, false},
		{":/shader3.glsl", QOpenGLShader::Compute, false},
		{":/shader3.glsl", QOpenGLShader::Compute, true},
		{":/shader3_msdf.glsl", QOpenGLShader::Compute, false},
		{":/shader3_msdf.glsl", QOpenGLShader::Compute, true},
		{":/msdf_fixer.glsl", QOpenGLShader::Compute, false},
		{":/screen.vert.glsl", QOpenGLShader::Vertex, false}
	};

	for(const auto& shader : shaders) {
		compileShader(*gl, QString::fromLatin1(shader.path), shader.type, shader.manhattan);
	}
}

TEST_CASE("OpenGL Manhattan mode affects raster SDF output", "[glsl][opengl]")
{
	QImage source(32, 32, QImage::Format_Grayscale8);
	source.fill(0);
	for(int y = 0; y < 32; ++y) {
		uchar* row = source.scanLine(y);
		for(int x = 0; x < 32; ++x) {
			if(x + y >= 24) row[x] = 255;
		}
	}

	QImage euclidean;
	QImage manhattan;
	try {
		SdfGenerationGL euclideanContext(makeArgs(Euclidean));
		euclidean = euclideanContext.produceBitmapSdf(source, makeArgs(Euclidean));
	} catch(const std::exception& error) {
		SKIP("OpenGL SDF generation is not available: " << error.what());
	}
	try {
		SdfGenerationGL manhattanContext(makeArgs(Manhattan));
		manhattan = manhattanContext.produceBitmapSdf(source, makeArgs(Manhattan));
	} catch(const std::exception& error) {
		SKIP("OpenGL SDF generation is not available: " << error.what());
	}

	REQUIRE(euclidean.size() == QSize(32, 32));
	REQUIRE(manhattan.size() == QSize(32, 32));
	CHECK(euclidean != manhattan);
}
