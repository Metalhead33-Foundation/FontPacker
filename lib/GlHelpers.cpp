#include "GlHelpers.hpp"
#include <QOpenGLExtraFunctions>

GlHelpers::GlHelpers()
{
	QSurfaceFormat fmt;
	fmt.setDepthBufferSize(24);
	if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
		qDebug("Requesting OpenGL 4.3 core context");
		fmt.setVersion(4, 3);
		fmt.setProfile(QSurfaceFormat::CoreProfile);
	} else {
		qDebug("Requesting OpenGL ES 3.2 context");
		fmt.setVersion(3, 2);
	}
	QSurfaceFormat::setDefaultFormat(fmt);
	glContext = std::make_unique<QOpenGLContext>();
	glContext->setFormat(fmt);
	if(!glContext->create()) throw std::runtime_error("Failed to create OpenGL context!");
	glSurface = std::make_unique<QOffscreenSurface>();
	glSurface->create();
	if(!glContext->makeCurrent(glSurface.get())) throw std::runtime_error("Failed to make the context current!");
	glFuncs = glContext->functions();
	extraFuncs = glContext->extraFunctions();
	gl43Funcs = dynamic_cast<QOpenGLFunctions_4_3_Core*>(QOpenGLVersionFunctionsFactory::get(QOpenGLVersionProfile(),glContext.get()));
}

GlHelpers::GlHelpers(GlHelpers&& mov)
	: glFuncs(mov.glFuncs), extraFuncs(mov.extraFuncs), glContext(std::move(mov.glContext)), glSurface(std::move(mov.glSurface)), gl43Funcs(mov.gl43Funcs)
{
}

GlHelpers& GlHelpers::operator=(GlHelpers&& mov)
{
	this->glFuncs = mov.glFuncs;
	this->extraFuncs = mov.extraFuncs;
	this->glContext = std::move(mov.glContext);
	this->glSurface = std::move(mov.glSurface);
	this->gl43Funcs = mov.gl43Funcs;
	return *this;
}

GLuint GlTexture::getTexId() const
{
	return texId;
}

GlTexture::GlTexture()
	: texId(0), width(0), height(0), format{0, 0, 0}
{
	glGenTextures(1,&texId);
}

GlTexture::GlTexture(GLsizei width, GLsizei height, const GlTextureFormat& format, const void* data)
	: texId(0), width(width), height(height), format{ format }
{
	glGenTextures(1,&texId);
	initialize(width, height, format, data);
}

GlTexture::GlTexture(GLsizei width, GLsizei height, QImage::Format imgFormat, const void* data)
	: texId(0), width(width), height(height), format{ getTextureFormat(imgFormat) }
{
	glGenTextures(1,&texId);
	initialize(width, height, format, data);
}

GlTexture::GlTexture(const QImage& image)
	: texId(0), width(image.width()), height(image.height()), format{ getTextureFormat(image.format()) }
{
	glGenTextures(1,&texId);
	initialize(width, height, format, nullptr);
	modify(width, height, format, [&image](GLsizei scanline) { return image.scanLine(scanline); });
}

GlTexture::~GlTexture()
{
	if(texId) {
		glDeleteTextures(1,&texId);
	}
}

GlTexture::GlTexture(GlTexture&& mov)
	: texId(mov.texId), width(mov.width), height(mov.height), format(mov.format)
{
	mov.texId = 0;
}

GLsizei GlTexture::getWidth() const
{
	return width;
}

GLsizei GlTexture::getHeight() const
{
	return height;
}

const GlTextureFormat& GlTexture::getFormat() const
{
	return format;
}

GlTexture& GlTexture::operator=(GlTexture&& mov)
{
	if(texId) {
		glDeleteTextures(1,&texId);
	}
	this->texId = mov.texId;
	mov.texId = 0;
	this->width = mov.width;
	this->height = mov.height;
	this->format = mov.format;
	return *this;
}

void GlTexture::bind() const
{
	glBindTexture(GL_TEXTURE_2D, texId);
}

GlTextureFormat GlTexture::getTextureFormat(QImage::Format imgFormat)
{
	switch(imgFormat) {
		case QImage::Format_Invalid:
			return { 0, 0, 0};
		case QImage::Format_Mono:
		case QImage::Format_MonoLSB:
		case QImage::Format_Indexed8:
		case QImage::Format_Grayscale8:
			return { GL_R8, GL_RED, GL_UNSIGNED_BYTE };
		case QImage::Format_RGB32:
		case QImage::Format_ARGB32:
		case QImage::Format_ARGB32_Premultiplied:
			return { GL_RGBA8, GL_BGRA, GL_UNSIGNED_BYTE };
		case QImage::Format_RGB16:
			return { GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_6_5 };
		case QImage::Format_RGB555:
			return { GL_RGB5, GL_RGB, GL_UNSIGNED_SHORT_5_5_5_1 };
		case QImage::Format_RGB888:
		case QImage::Format_BGR888:
			return { GL_RGB8, GL_RGB, GL_UNSIGNED_BYTE };
		case QImage::Format_RGBX8888:
		case QImage::Format_RGBA8888:
		case QImage::Format_RGBA8888_Premultiplied:
			return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE };
		case QImage::Format_RGB30:
		case QImage::Format_BGR30:
			return { GL_RGB10_A2, GL_RGB, GL_UNSIGNED_INT_10_10_10_2 };
		case QImage::Format_A2RGB30_Premultiplied:
		case QImage::Format_A2BGR30_Premultiplied:
			return { GL_RGB10_A2, GL_RGBA, GL_UNSIGNED_INT_10_10_10_2 };
		case QImage::Format_Alpha8:
			return { GL_R8, GL_RED, GL_UNSIGNED_BYTE };
		case QImage::Format_Grayscale16:
			return { GL_R16, GL_RED, GL_UNSIGNED_SHORT };
		case QImage::Format_RGBX64:
		case QImage::Format_RGBA64:
		case QImage::Format_RGBA64_Premultiplied:
			return { GL_RGBA16, GL_RGBA, GL_UNSIGNED_SHORT };
		case QImage::Format_RGBX16FPx4:
		case QImage::Format_RGBA16FPx4:
		case QImage::Format_RGBA16FPx4_Premultiplied:
			return { GL_RGBA16F, GL_RGBA, GL_HALF_FLOAT };
		case QImage::Format_RGBX32FPx4:
		case QImage::Format_RGBA32FPx4:
		case QImage::Format_RGBA32FPx4_Premultiplied:
			return { GL_RGBA32F, GL_RGBA, GL_FLOAT };
		case QImage::Format_CMYK8888:
			return { GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE }; // CMYK isn't directly supported, mapping to RGBA
		case QImage::NImageFormats:
		default:
			return { 0, 0, 0};
	}
}
size_t GlTexture::getBytesPerPixel(GLint internalformat)
{
	switch (internalformat) {
		case GL_R8: case GL_R8_SNORM: case GL_R8I: case GL_R8UI:
			return 1;
		case GL_R16: case GL_R16_SNORM: case GL_R16I: case GL_R16UI: case GL_R16F:
			return 2;
		case GL_RG8: case GL_RG8_SNORM: case GL_RG8I: case GL_RG8UI:
			return 2;
		case GL_RG16: case GL_RG16_SNORM: case GL_RG16I: case GL_RG16UI: case GL_RG16F:
			return 4;
		case GL_RGB4: case GL_RGB5: case GL_R3_G3_B2:
			return 2;
		case GL_RGB8: case GL_RGB8_SNORM: case GL_RGB8I: case GL_RGB8UI:
			return 3;
		case GL_RGB10: case GL_RGB12: case GL_RGB16_SNORM: case GL_RGB16I: case GL_RGB16UI: case GL_RGB16F:
			return 6;
		case GL_RGB10_A2: case GL_RGB10_A2UI: case GL_RGBA2:
			return 4;
		case GL_RGBA4: case GL_RGB5_A1:
			return 2;
		case GL_RGBA8: case GL_RGBA8_SNORM: case GL_RGBA8I: case GL_RGBA8UI:
			return 4;
		case GL_RGBA12: case GL_RGBA16: case GL_RGBA16I: case GL_RGBA16UI: case GL_RGBA16F:
			return 8;
		case GL_SRGB8: case GL_RGB9_E5:
			return 3;
		case GL_SRGB8_ALPHA8:
			return 4;
		case GL_R32F:
			return 4;
		case GL_RG32F:
			return 8;
		case GL_RGB32F:
			return 12;
		case GL_RGBA32F:
			return 16;
		case GL_R11F_G11F_B10F:
			return 4;
		case GL_R32I: case GL_R32UI:
			return 4;
		case GL_RG32I: case GL_RG32UI:
			return 8;
		case GL_RGB32I: case GL_RGB32UI:
			return 12;
		case GL_RGBA32I: case GL_RGBA32UI:
			return 16;
		default:
			return 0;
	}
}

size_t GlTexture::getBytesPerPixel() const
{
	return getBytesPerPixel(this->format.internalformat);
}

void GlTexture::initialize(GLsizei width, GLsizei height, const GlTextureFormat& format, const void* data)
{
	glPixelStorei( GL_PACK_ALIGNMENT, 1);
	glPixelStorei(  GL_UNPACK_ALIGNMENT, 1);
	bind();
	this->format = format;
	this->width = width;
	this->height = height;
	glTexImage2D(GL_TEXTURE_2D, 0, format.internalformat, width, height, 0, format.format, format.type, data);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void GlTexture::initialize(GLsizei width, GLsizei height, QImage::Format imgFormat, const void* data)
{
	auto format = getTextureFormat(imgFormat);
	initialize(width, height, format, data);
}

void GlTexture::modify(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const GlTextureFormat& format, const void* pixels)
{
	bind();
	glTexSubImage2D(GL_TEXTURE_2D, 0, xoffset, yoffset, width, height, format.format, format.type, pixels);
}

void GlTexture::modify(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, QImage::Format imgFormat, const void* pixels)
{
	auto format = getTextureFormat(imgFormat);
	modify(xoffset, yoffset, width, height, format, pixels);
}

void GlTexture::modify(GLsizei width, GLsizei height, const GlTextureFormat& format, const ScanlineIteratingFunction& scanlineGetter)
{
	bind();
	for(GLsizei y = 0; y < height; ++y) {
		const void* scanline = scanlineGetter(y);
		glTexSubImage2D(GL_TEXTURE_2D, 0, 0, y, width, 1, format.format, format.type, scanline);
	}
}

void GlTexture::modify(GLsizei width, GLsizei height, QImage::Format imgFormat, const ScanlineIteratingFunction& scanlineGetter)
{
	auto format = getTextureFormat(imgFormat);
	modify(width, height, format, scanlineGetter);
}

void GlTexture::modify(const QImage& qimg)
{
	modify(qimg.width(), qimg.height(), qimg.format(), [&qimg](GLsizei scanline) { return qimg.scanLine(scanline); } );
}

void GlTexture::getTexture(const GlTextureFormat& format, void* pixels) const
{
	bind();
	glGetTexImage(GL_TEXTURE_2D, 0, format.format, format.type, pixels);
}

void GlTexture::getTexture(QImage::Format imgFormat, void* pixels) const
{
	auto format = getTextureFormat(imgFormat);
	getTexture(format, pixels);
}

void GlTexture::getTexture(void* pixels) const
{
	bind();
	glGetTexImage(GL_TEXTURE_2D, 0, format.format, format.type, pixels);
}

QByteArray GlTexture::getTexture(const GlTextureFormat& format) const
{
	QByteArray arr(width * height * getBytesPerPixel(), Qt::Initialization::Uninitialized);
	getTexture(format, arr.data());
	return arr;
}

QByteArray GlTexture::getTexture(QImage::Format imgFormat) const
{
	QByteArray arr(width * height * getBytesPerPixel(), Qt::Initialization::Uninitialized);
	getTexture(imgFormat, arr.data());
	return arr;
}

QByteArray GlTexture::getTexture() const
{
	QByteArray arr(width * height * getBytesPerPixel(), Qt::Initialization::Uninitialized);
	getTexture(arr.data());
	return arr;
}

void GlTexture::bindAsImage(QOpenGLExtraFunctions* extraFuncs, GLuint unit, GLenum access) const
{
	extraFuncs->glBindImageTexture(unit, texId, 0, false, 0, access, format.internalformat);
}


GlStorageBuffer::GlStorageBuffer(QOpenGLFunctions* glFuncs, QOpenGLExtraFunctions* extraFuncs, bool isSSBO)
	: target(isSSBO ? GL_SHADER_STORAGE_BUFFER : GL_UNIFORM_BUFFER), size(0), glFuncs(glFuncs), extraFuncs(extraFuncs)
{
	glFuncs->glGenBuffers(1,&buffId);
}

GlStorageBuffer::~GlStorageBuffer()
{
	if(buffId) {
		glFuncs->glDeleteBuffers(1,&buffId);
	}
}

GlStorageBuffer::GlStorageBuffer(GlStorageBuffer&& mov)
	: target(mov.target), size(mov.size), buffId(mov.buffId), glFuncs(mov.glFuncs), extraFuncs(mov.extraFuncs)
{
	mov.buffId = 0;
}

size_t GlStorageBuffer::getSize() const
{
	return size;
}

GLenum GlStorageBuffer::getTarget() const
{
	return target;
}

GLuint GlStorageBuffer::getBuffId() const
{
	return buffId;
}

GlStorageBuffer& GlStorageBuffer::operator=(GlStorageBuffer&& mov)
{
	this->glFuncs = mov.glFuncs;
	this->extraFuncs = mov.extraFuncs;
	if(buffId) {
		glFuncs->glDeleteBuffers(1,&buffId);
	}
	this->target = mov.target;
	this->size = mov.size;
	this->buffId = mov.buffId;
	mov.buffId = 0;
	return *this;
}

void GlStorageBuffer::bind() const
{
	glFuncs->glBindBuffer(target, buffId);
}

void GlStorageBuffer::bindBase(GLuint index) const
{
	extraFuncs->glBindBufferBase(target, index, buffId);
}

void GlStorageBuffer::initialize(GLsizeiptr size, const void* data)
{
	bind();
	this->size = size;
	glFuncs->glBufferData(target, size, data,  GL_DYNAMIC_DRAW);
}

void GlStorageBuffer::modify(GLintptr offset, GLsizeiptr size, const void* data)
{
	bind();
	glFuncs->glBufferSubData(target, offset, size, data);
}
