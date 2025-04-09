#include "SdfGenerationGL.hpp"
#include "HugePreallocator.hpp"
#include "qopenglextrafunctions.h"
#include <QFile>
#include <QTextStream>
#include <cassert>
#include <glm/glm.hpp>
#include "RGBA8888.hpp"

struct Rgb32f {
	float r, g, b, a;
};
struct Rgba8 {
	uint8_t r, g, b, a;
};


QImage::Format SdfGenerationGL::getFinalImageFormat(const SDFGenerationArguments& args)
{
	switch (args.type ) {
		case SDF: return QImage::Format_Grayscale8;
		case MSDF: return QImage::Format_RGBA8888;
		case MSDFA: return QImage::Format_RGBA8888;
		default: return QImage::Format_Invalid;
	}
}

GlTextureFormat SdfGenerationGL::getTemporaryTextureFormat(const SDFGenerationArguments& args)
{
	switch (args.type ) {
		case SDF: return { GL_R32F, GL_RED, GL_FLOAT };
		case MSDF: return { GL_RGBA32F, GL_RGBA, GL_FLOAT };
		case MSDFA: return { GL_RGBA32F, GL_RGBA, GL_FLOAT };
		default: return { -1, 0, 0 };
	}
}

float gammaAdjust(float x) {
	float d = x - 0.5f;
	return 0.5f + 2.0f * d * d * d + 0.5f * d;
}

void SdfGenerationGL::fetchSdfFromGPU(QImage& newimg, const SDFGenerationArguments& args)
{
	std::vector<uint8_t> areTheyInside = newTex2.getTextureAs<uint8_t>();
	std::vector<float> rawDistances = newTex.getTextureAs<float>();
	float maxDistIn = std::numeric_limits<float>::epsilon();
	float maxDistOut = std::numeric_limits<float>::epsilon();
	for(size_t i = 0; i < rawDistances.size(); ++i) {
		if(areTheyInside[i]) {
			maxDistIn = std::max(maxDistIn, std::abs(rawDistances[i]) );
		} else {
			maxDistOut = std::max(maxDistOut, std::abs(rawDistances[i]) );
		}
	}
	for(size_t i = 0; i < rawDistances.size(); ++i) {
		float& it = rawDistances[i];
		if(areTheyInside[i]) {
			it /= maxDistIn;
			it = 0.5f + (it * 0.5f);
		} else {
			it /= maxDistOut;
			it = 0.5f - (it * 0.5f);
		}
	}
	if(args.midpointAdjustment.has_value()) {
		float toDivideWith = args.midpointAdjustment.value_or(1.0f);
		for(size_t i = 0; i < rawDistances.size(); ++i) {
			float& it = rawDistances[i];
			it = std::clamp(it/toDivideWith,0.0f,1.0f);
		}
	}
	if(args.gammaCorrect) {
		for(size_t i = 0; i < rawDistances.size(); ++i) {
			float& it = rawDistances[i];
			it = gammaAdjust(it);
		}
	}

	for(int y = 0; y < newimg.height(); ++y) {
		uchar* scanline = newimg.scanLine(y);
		const float* rawScanline = &rawDistances[newimg.width()*y];
		for(int x = 0; x < newimg.width(); ++x) {
			scanline[x] = static_cast<uint8_t>(rawScanline[x] * 255.f);
		}
	}
}

/*
	bool gammaCorrect;
	bool maximizeInsteadOfAverage;
	std::optional<float> midpointAdjustment;
*/

void SdfGenerationGL::fetchMSDFFromGPU(QImage& newimg, const SDFGenerationArguments& args)
{
	std::vector<RGBA8888> areTheyInside = newTex2.getTextureAs<RGBA8888>();
	// We need HugePreallocator!
	std::vector<glm::fvec4> rawDistances = newTex.getTextureAs<glm::fvec4>();
	//std::vector<Rgba8,HugePreallocator<Rgba8>> rawDistances = newTex.getTextureAs<Rgba8,HugePreallocator<Rgba8>>();
	//std::vector<Rgba8> rawDistances = newTex.getTextureAs<Rgba8>();
	glm::fvec3 maxDistIn(std::numeric_limits<float>::epsilon());
	glm::fvec3 maxDistOut(std::numeric_limits<float>::epsilon());
	glm::fvec4 maxDistAbs(std::numeric_limits<float>::epsilon());

	for(size_t i = 0; i < rawDistances.size(); ++i) {
		for(int z = 0; z < 4; ++z) {
			maxDistAbs[z] = std::max(maxDistAbs.r, std::abs(rawDistances[i][z]));
		}
		/*if(areTheyInside[i].r) {
			maxDistIn.r = std::max(maxDistIn.r, std::abs(rawDistances[i].r) );
		} else {
			maxDistOut.r = std::max(maxDistOut.r, std::abs(rawDistances[i].r) );
		}
		if(areTheyInside[i].g) {
			maxDistIn.g = std::max(maxDistIn.g, std::abs(rawDistances[i].g) );
		} else {
			maxDistOut.g = std::max(maxDistOut.g, std::abs(rawDistances[i].g) );
		}
		if(areTheyInside[i].b) {
			maxDistIn.b = std::max(maxDistIn.b, std::abs(rawDistances[i].b) );
		} else {
			maxDistOut.b = std::max(maxDistOut.b, std::abs(rawDistances[i].b) );
		}*/
		/*if(areTheyInside[i].a) {
			maxDistIn.r = std::max(maxDistIn.r, std::abs(rawDistances[i].r) );
			maxDistIn.g = std::max(maxDistIn.g, std::abs(rawDistances[i].g) );
			maxDistIn.b = std::max(maxDistIn.b, std::abs(rawDistances[i].b) );
		} else {
			maxDistOut.r = std::max(maxDistOut.r, std::abs(rawDistances[i].r) );
			maxDistOut.g = std::max(maxDistOut.g, std::abs(rawDistances[i].g) );
			maxDistOut.b = std::max(maxDistOut.b, std::abs(rawDistances[i].b) );
		}*/
		/*maxDistIn.r = std::max(maxDistIn.r, std::abs(rawDistances[i].r) );
		maxDistIn.g = std::max(maxDistIn.g, std::abs(rawDistances[i].g) );
		maxDistIn.b = std::max(maxDistIn.b, std::abs(rawDistances[i].b) );*/
	}
	for(size_t i = 0; i < rawDistances.size(); ++i) {
		glm::fvec4& it = rawDistances[i];
		for(int z = 0; z < 4; ++z) {
			it[z] /= maxDistAbs[z];
			it[z] *= -1.0f;
			it[z] /= 2.0f;
			it[z] += 0.5f;
		}
		/*if(areTheyInside[i].r) {
			it.r /= maxDistIn.r;
			it.r = 0.5f + (it.r * 0.5f);
		} else {
			it.r /= maxDistOut.r;
			it.r = 0.5f - (it.r * 0.5f);
		}
		if(areTheyInside[i].g) {
			it.g /= maxDistIn.g;
			it.g = 0.5f + (it.g * 0.5f);
		} else {
			it.g /= maxDistOut.g;
			it.g = 0.5f - (it.g * 0.5f);
		}
		if(areTheyInside[i].b) {
			it.b /= maxDistIn.b;
			it.b = 0.5f + (it.b * 0.5f);
		} else {
			it.b /= maxDistOut.b;
			it.b = 0.5f - (it.b * 0.5f);
		}*/
		/*if(areTheyInside[i].a) {
			it /= glm::fvec4(maxDistIn,1.0f);
			it = 0.5f + (it * 0.5f);
		} else {
			it /= glm::fvec4(maxDistOut,1.0f);
			it = 0.5f - (it * 0.5f);
		}*/
		it.a = 1.0f;
		/*it /= glm::fvec4(maxDistIn,1.0f);
		it.r = 1.0f - it.r;
		it.g = 1.0f - it.g;
		it.b = 1.0f - it.b;*/
	}
	if(args.midpointAdjustment.has_value()) {
		float toDivideWith = args.midpointAdjustment.value_or(1.0f);
		for(size_t i = 0; i < rawDistances.size(); ++i) {
			glm::fvec4& it = rawDistances[i];
			it.x = std::clamp(it.x/toDivideWith,0.0f,1.0f);
			it.y = std::clamp(it.y/toDivideWith,0.0f,1.0f);
			it.z = std::clamp(it.z/toDivideWith,0.0f,1.0f);
			it.w = std::clamp(it.w/toDivideWith,0.0f,1.0f);
		}
	}
	if(args.gammaCorrect) {
		for(size_t i = 0; i < rawDistances.size(); ++i) {
			glm::fvec4& it = rawDistances[i];
			it.x = gammaAdjust(it.x);
			it.y = gammaAdjust(it.y);
			it.z = gammaAdjust(it.z);
			it.w = gammaAdjust(it.w);
		}
	}
	for(int y = 0; y < newimg.height(); ++y) {
		RGBA8888* scanline = reinterpret_cast<RGBA8888*>(newimg.scanLine(y));
		const glm::fvec4* rawScanline = &rawDistances[newimg.width()*y];
		for(int x = 0; x < newimg.width(); ++x) {
			const glm::fvec4& rawPixel = rawScanline[x];
			RGBA8888& outputPixel = scanline[x];
			outputPixel.fromFvec4(rawPixel);
		}
	}
}

SdfGenerationGL::SdfGenerationGL(const SDFGenerationArguments& args) :
	glHelpers(), finalImageFormat(getFinalImageFormat(args)), temporaryTextureFormat(getTemporaryTextureFormat(args)),
	oldTex(args.internalProcessSize,args.internalProcessSize, QImage::Format_Grayscale8),
	newTex(args.internalProcessSize, args.internalProcessSize, temporaryTextureFormat),
	newTex2(args.internalProcessSize, args.internalProcessSize, args.type == SDFType::SDF ? QImage::Format_Grayscale8 : QImage::Format_RGBA8888 ),
	uniformBuffer(glHelpers.glFuncs, glHelpers.extraFuncs), ssboForEdges(glHelpers.glFuncs, glHelpers.extraFuncs, true)
{
	QTextStream errStrm(stderr);
	glShader = std::make_unique<QOpenGLShaderProgram>();
	if(!glShader->create()) throw std::runtime_error("Failed to create shader!");
	{
		QFile res(args.type == SDFType::SDF ? ":/shader1.glsl" : ":/shader_msdf1.glsl");
		if(res.open(QFile::ReadOnly)) {
			QByteArray shdrArr = res.readAll();
			if(args.distType == DistanceType::Manhattan) {
				shdrArr.insert(shdrArr.indexOf('\n')+1,QByteArrayLiteral("#define USE_MANHATTAN_DISTANCE\n"));
			}
			if(!glShader->addCacheableShaderFromSourceCode(QOpenGLShader::Compute,shdrArr)) {
				errStrm << glShader->log() << '\n';
				errStrm.flush();
				throw std::runtime_error("Failed to add shader!");
			}
			if(!glShader->link()) {
				errStrm << glShader->log() << '\n';
				errStrm.flush();
				throw std::runtime_error("Failed to compile OpenGL shader!");
			}
		}
	}
	glShader2 = std::make_unique<QOpenGLShaderProgram>();
	{
		QFile res(args.type == SDFType::SDF ? ":/shader3.glsl" : ":/shader3_msdf.glsl");
		if(res.open(QFile::ReadOnly)) {
			QByteArray shdrArr = res.readAll();
			if(args.distType == DistanceType::Manhattan) {
				shdrArr.insert(shdrArr.indexOf('\n')+1,QByteArrayLiteral("#define USE_MANHATTAN_DISTANCE\n"));
			}
			if(!glShader2->addCacheableShaderFromSourceCode(QOpenGLShader::Compute,shdrArr)) {
				errStrm << glShader2->log() << '\n';
				errStrm.flush();
				throw std::runtime_error("Failed to add shader!");
			}
			if(!glShader2->link()) {
				errStrm << glShader2->log() << '\n';
				errStrm.flush();
				throw std::runtime_error("Failed to compile OpenGL shader!");
			}
		}
	}
	uniform.width = args.samples_to_check_x ? args.samples_to_check_x / 2 : args.padding;
	uniform.height = args.samples_to_check_y ? args.samples_to_check_y / 2 : args.padding;
	uniformBuffer.initializeFrom(uniform);
	glHelpers.glFuncs->glUseProgram(glShader->programId());
	fontUniform = glShader->uniformLocation("fontTexture");
	sdfUniform1 = glShader->uniformLocation("rawSdfTexture");
	sdfUniform2 = glShader->uniformLocation("isInsideTex");
	dimensionsUniform = glShader->uniformLocation("Dimensions");
	glHelpers.glFuncs->glUseProgram(glShader2->programId());
	sdfUniform1_vec = glShader2->uniformLocation("rawSdfTexture");
	sdfUniform2_vec = glShader2->uniformLocation("isInsideTex");
	ssboUniform_vec = glHelpers.extraFuncs->glGetProgramResourceIndex(glShader2->programId(), GL_SHADER_STORAGE_BLOCK, "EdgeBuffer");
	dimensionsUniform_vec = glShader2->uniformLocation("Dimensions");
}

/*
		int sdfUniform1_vec;
		int sdfUniform2_vec;
		int ssboUniform_vec;
*/

QImage SdfGenerationGL::produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args)
{
	glPixelStorei( GL_PACK_ALIGNMENT, 1);
	glPixelStorei(  GL_UNPACK_ALIGNMENT, 1);
	glHelpers.glFuncs->glUseProgram(glShader->programId());
	oldTex.modify(source);
	glHelpers.glFuncs->glUseProgram(glShader->programId());
	oldTex.bindAsImage(glHelpers.extraFuncs, 0, GL_READ_ONLY);
	glHelpers.glFuncs->glUniform1i(fontUniform,0);
	newTex.bindAsImage(glHelpers.extraFuncs, 1, GL_WRITE_ONLY);
	glHelpers.glFuncs->glUniform1i(sdfUniform1,1);
	newTex2.bindAsImage(glHelpers.extraFuncs, 2, GL_WRITE_ONLY);
	glHelpers.glFuncs->glUniform1i(sdfUniform2,2);
	uniformBuffer.bindBase(3);
	glHelpers.extraFuncs->glUniformBlockBinding(glShader->programId(), dimensionsUniform, 3);
	glHelpers.extraFuncs->glDispatchCompute(args.internalProcessSize,args.internalProcessSize,1);
	glHelpers.extraFuncs->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	QImage newimg(args.internalProcessSize, args.internalProcessSize, finalImageFormat);
	switch (args.type) {
		case SDF: {
			fetchSdfFromGPU(newimg,args);
			break;
		}
		case MSDF: {
			fetchMSDFFromGPU(newimg,args);
			break;
		}
		case MSDFA:
			break;
		default: break;
	}
	return newimg;
}

QImage SdfGenerationGL::produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args)
{
	glPixelStorei( GL_PACK_ALIGNMENT, 1);
	glPixelStorei(  GL_UNPACK_ALIGNMENT, 1);
	QImage newimg(args.internalProcessSize, args.internalProcessSize, finalImageFormat);
	glHelpers.glFuncs->glUseProgram(glShader2->programId());
	ssboForEdges.bind();
	ssboForEdges.initializeFromSpan( std::span<const EdgeSegment>( source.edges.data(), source.edges.size() ) );
	newTex.bindAsImage(glHelpers.extraFuncs, 1, GL_WRITE_ONLY);
	glHelpers.glFuncs->glUniform1i(sdfUniform1_vec,1);
	newTex2.bindAsImage(glHelpers.extraFuncs, 2, GL_WRITE_ONLY);
	glHelpers.glFuncs->glUniform1i(sdfUniform2_vec,2);
	ssboForEdges.bindBase(3);
	uniformBuffer.bindBase(4);
	glHelpers.gl43Funcs->glShaderStorageBlockBinding(glShader2->programId(), ssboUniform_vec, 3);
	glHelpers.extraFuncs->glUniformBlockBinding(glShader->programId(), dimensionsUniform_vec, 4);
	glHelpers.extraFuncs->glDispatchCompute(args.internalProcessSize,args.internalProcessSize,1);
	glHelpers.extraFuncs->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
	switch (args.type) {
		case SDF: {
			fetchSdfFromGPU(newimg,args);
			break;
		}
		case MSDF: {
			fetchMSDFFromGPU(newimg,args);
			break;
		}
		case MSDFA:
			break;
		default: break;
	}
	return newimg;
}
