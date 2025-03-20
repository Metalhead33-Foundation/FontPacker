#include "SdfGenerationGL.hpp"
#include "HugePreallocator.hpp"
#include "qopenglextrafunctions.h"
#include <QFile>
#include <QTextStream>

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
		case MSDF: return QImage::Format_RGB888;
		case MSDFA: return QImage::Format_RGBA8888;
		default: return QImage::Format_Invalid;
	}
}

GlTextureFormat SdfGenerationGL::getTemporaryTextureFormat(const SDFGenerationArguments& args)
{
	switch (args.type ) {
		case SDF: return { GL_R32F, GL_RED, GL_FLOAT };
		case MSDF: return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
		case MSDFA: return { GL_RGBA32F, GL_RGBA, GL_FLOAT };
		default: return { -1, 0, 0 };
	}
}
/*
	// Old Tex
	GlTexture oldTex(source);
	// New Tex
	GlTexture newTex(newimg.width(),newimg.height(), temporaryTextureFormat );
	// New Tex 2
	GlTexture newTex2(newimg.width(),newimg.height(), { GL_R8, GL_RED, GL_UNSIGNED_BYTE } );
	GlStorageBuffer uniformBuffer(glHelpers.glFuncs, glHelpers.extraFuncs);
*/

SdfGenerationGL::SdfGenerationGL(const SDFGenerationArguments& args) :
	glHelpers(), finalImageFormat(getFinalImageFormat(args)), temporaryTextureFormat(getTemporaryTextureFormat(args)),
	oldTex(args.internalProcessSize,args.internalProcessSize, QImage::Format_Grayscale8),
	newTex(args.internalProcessSize, args.internalProcessSize, temporaryTextureFormat),
	newTex2(args.internalProcessSize, args.internalProcessSize, { GL_R8, GL_RED, GL_UNSIGNED_BYTE }),
	uniformBuffer(glHelpers.glFuncs, glHelpers.extraFuncs)
{
	QTextStream errStrm(stderr);
	glShader = std::make_unique<QOpenGLShaderProgram>();
	if(!glShader->create()) throw std::runtime_error("Failed to create shader!");
	QFile res(args.type == SDFType::SDF ? (args.distType == DistanceType::Euclidean ? ":/shader1.glsl" : ":/shader2.glsl") : ":/shader_msdf1.glsl");
	if(res.open(QFile::ReadOnly)) {
		QByteArray shdrArr = res.readAll();
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
	uniform.width = args.samples_to_check_x ? args.samples_to_check_x / 2 : args.padding;
	uniform.height = args.samples_to_check_y ? args.samples_to_check_y / 2 : args.padding;
	uniformBuffer.initializeFrom(uniform);
	glHelpers.glFuncs->glUseProgram(glShader->programId());
	fontUniform = glShader->uniformLocation("fontTexture");
	sdfUniform1 = glShader->uniformLocation("rawSdfTexture");
	sdfUniform2 = glShader->uniformLocation("isInsideTex");
	dimensionsUniform = glShader->uniformLocation("Dimensions");
}

QImage SdfGenerationGL::produceSdf(const QImage& source, const SDFGenerationArguments& args)
{
	glPixelStorei( GL_PACK_ALIGNMENT, 1);
	glPixelStorei(  GL_UNPACK_ALIGNMENT, 1);
	oldTex.modify(source);
	glHelpers.glFuncs->glUseProgram(glShader->programId());
	oldTex.bindAsImage(glHelpers.extraFuncs, 0, GL_READ_ONLY);
	glHelpers.glFuncs->glUniform1i(fontUniform,0);
	newTex.bindAsImage(glHelpers.extraFuncs, 1, GL_WRITE_ONLY);
	glHelpers.glFuncs->glUniform1i(sdfUniform1,1);
	if(args.type == SDFType::SDF) {
		newTex2.bindAsImage(glHelpers.extraFuncs, 2, GL_WRITE_ONLY);
		glHelpers.glFuncs->glUniform1i(sdfUniform2,2);
		uniformBuffer.bindBase(3);
	}
	glHelpers.extraFuncs->glUniformBlockBinding(glShader->programId(), dimensionsUniform, 3);
	glHelpers.extraFuncs->glDispatchCompute(args.internalProcessSize,args.internalProcessSize,1);
	glHelpers.extraFuncs->glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	QImage newimg(args.internalProcessSize, args.internalProcessSize, finalImageFormat);
	switch (args.type) {
		case SDF: {
			std::vector<uint8_t> areTheyInside = newTex2.getTextureAs<uint8_t>();
			std::vector<float> rawDistances = newTex.getTextureAs<float>();
			float maxDistIn = std::numeric_limits<float>::epsilon();
			float maxDistOut = std::numeric_limits<float>::epsilon();
			for(size_t i = 0; i < rawDistances.size(); ++i) {
				if(areTheyInside[i]) {
					maxDistIn = std::max(maxDistIn,rawDistances[i]);
				} else {
					maxDistOut = std::max(maxDistOut,rawDistances[i]);
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

			for(int y = 0; y < newimg.height(); ++y) {
				uchar* scanline = newimg.scanLine(y);
				const float* rawScanline = &rawDistances[newimg.width()*y];
				for(int x = 0; x < newimg.width(); ++x) {
					scanline[x] = static_cast<uint8_t>(rawScanline[x] * 255.f);
				}
			}
			break;
		}
		case MSDF: {
			// We need HugePreallocator!
			//std::vector<Rgb32f,Mallocator<Rgb32f>> rawDistances = newTex.getTextureAs<Rgb32f,Mallocator<Rgb32f>>();
			std::vector<Rgba8,HugePreallocator<Rgba8>> rawDistances = newTex.getTextureAs<Rgba8,HugePreallocator<Rgba8>>();
			/*float maxR = std::numeric_limits<float>::epsilon();
			float maxG = std::numeric_limits<float>::epsilon();
			float maxB = std::numeric_limits<float>::epsilon();
			for(const auto& it : rawDistances) {
				maxR = std::max(maxR,it.r);
				maxG = std::max(maxR,it.g);
				maxB = std::max(maxR,it.b);
			}
			for(auto& it : rawDistances) {
				it.r /= maxR;
				it.g /= maxG;
				it.b /= maxB;
			}*/
			for(int y = 0; y < newimg.height(); ++y) {
				QRgb* scanline = reinterpret_cast<QRgb*>(newimg.scanLine(y));
				const Rgba8* rawScanline = &rawDistances[newimg.width()*y];
				for(int x = 0; x < newimg.width(); ++x) {
					const Rgba8& rawPixel = rawScanline[x];
					QColor qclr;
					qclr.setRgb(rawPixel.r, rawPixel.g, rawPixel.b, rawPixel.a );
					//qclr.setRgbF(rawPixel.r,rawPixel.g,rawPixel.b,1.0f);
					scanline[x] = qclr.rgb();
				}
			}
			break;
		}
		case MSDFA:
			break;
		default: break;
	}
	return newimg;
}
