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
		if(args.invert) it = 1.0f - it;
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
	/*glHelpers.glFuncs->glUseProgram(msdfFixerShader->programId());
	newTex.bindAsImage(glHelpers.extraFuncs, 0, GL_READ_ONLY);
	glHelpers.glFuncs->glUniform1i(fixer_tex_uniform1,0);
	newTex3.bindAsImage(glHelpers.extraFuncs, 1, GL_WRITE_ONLY);
	glHelpers.glFuncs->glUniform1i(fixer_tex_uniform2,1);
	glHelpers.extraFuncs->glDispatchCompute(args.internalProcessSize,args.internalProcessSize,1);
	glHelpers.extraFuncs->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);*/
	std::vector<RGBA8888> areTheyInside = newTex2.getTextureAs<RGBA8888>();
	// We need HugePreallocator!
	//std::vector<glm::fvec4> rawDistances = newTex3.getTextureAs<glm::fvec4>();
	std::vector<glm::fvec4> rawDistances = newTex.getTextureAs<glm::fvec4>();
	//std::vector<Rgba8,HugePreallocator<Rgba8>> rawDistances = newTex.getTextureAs<Rgba8,HugePreallocator<Rgba8>>();
	//std::vector<Rgba8> rawDistances = newTex.getTextureAs<Rgba8>();
	glm::fvec3 maxDistIn(std::numeric_limits<float>::lowest());
	glm::fvec3 maxDistOut(std::numeric_limits<float>::lowest());

	glm::fvec4 minDist(std::numeric_limits<float>::max());
	glm::fvec4 maxDist(std::numeric_limits<float>::lowest());

	/*const float distCap = std::sqrt(static_cast<float>(args.samples_to_check_x) * static_cast<float>(args.samples_to_check_y));

	for(size_t i = 0; i < rawDistances.size(); ++i) {
		glm::fvec4& it = rawDistances[i];
		for(int z = 0; z < 4; ++z) {
			float& t = it[z];
			t = std::clamp(t, -distCap, distCap );
		}
	}*/
	for(size_t i = 0; i < rawDistances.size(); ++i) {
		for(int z = 0; z < 4; ++z) {
			maxDist[z] = std::max(maxDist[z], rawDistances[i][z]);
			minDist[z] = std::min(minDist[z], rawDistances[i][z]);
		}
	}
	for(size_t i = 0; i < rawDistances.size(); ++i) {
		glm::fvec4& it = rawDistances[i];
		for(int z = 0; z < 4; ++z) {
			float& t = it[z];
			const float& max = maxDist[z];
			const float& min = minDist[z];
			// Normalize from [min, max] to [-1, 1] while keeping 0 as 0
			if(t >= 0.0f) {
				t /= max;
			} else {
				t /= min;
				t *= -1.0f;
			}
			// Normalize from from [-1, 1] to [0, 1] while [0] becomes [0.5]
			t *= 0.5f;
			t += 0.5f;
			if(args.invert) t = 1.0f - t;
		}
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
	if( args.type != SDFType::MSDFA ) {
		for(size_t i = 0; i < rawDistances.size(); ++i) {
			rawDistances[i].a = 1.0f;
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
	newTex3(args.internalProcessSize, args.internalProcessSize, temporaryTextureFormat),
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
	if(args.type != SDFType::SDF) {
		msdfFixerShader = std::make_unique<QOpenGLShaderProgram>();
		QFile res(":/msdf_fixer.glsl");
		if(res.open(QFile::ReadOnly)) {
			QByteArray shdrArr = res.readAll();
			if(!msdfFixerShader->addCacheableShaderFromSourceCode(QOpenGLShader::Compute,shdrArr)) {
				errStrm << msdfFixerShader->log() << '\n';
				errStrm.flush();
				throw std::runtime_error("Failed to add shader!");
			}
			if(!msdfFixerShader->link()) {
				errStrm << msdfFixerShader->log() << '\n';
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
	if(args.type != SDFType::SDF) {
		glHelpers.glFuncs->glUseProgram(msdfFixerShader->programId());
		fixer_tex_uniform1 = msdfFixerShader->uniformLocation("sdf_input");
		fixer_tex_uniform2 = msdfFixerShader->uniformLocation("sdf_output");
	}
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
			fetchMSDFFromGPU(newimg,args);
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
			fetchMSDFFromGPU(newimg,args);
			break;
		default: break;
	}
	return newimg;
}
