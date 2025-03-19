#ifndef GLHELPERS_HPP
#define GLHELPERS_HPP
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOffscreenSurface>
#include <QImage>
#include <QImage>
#include <QByteArray>
#include <vector>
#include <functional>

struct GlHelpers {
private:
	GlHelpers(const GlHelpers& cpy) = delete;
	GlHelpers& operator=(const GlHelpers& cpy) = delete;
public:
	QOpenGLFunctions* glFuncs;
	QOpenGLExtraFunctions* extraFuncs;
	std::unique_ptr<QOpenGLContext> glContext;
	std::unique_ptr<QOffscreenSurface> glSurface;
	GlHelpers();
	GlHelpers(GlHelpers&& mov);
	GlHelpers& operator=(GlHelpers&& mov);
};

struct GlTextureFormat
{
	GLint internalformat;
	GLenum format;
	GLenum type;
};

class GlTexture {
public:
	typedef std::function<const void*(GLsizei scanline)> ScanlineIteratingFunction;
private:
	GLuint texId;
	GLsizei width;
	GLsizei height;
	GlTextureFormat format;
	GlTexture(const GlTexture& cpy) = delete;
	GlTexture& operator=(const GlTexture& cpy) = delete;
public:
	GLuint getTexId() const;
	GlTexture();
	GlTexture(GLsizei width, GLsizei height, const GlTextureFormat& format, const void * data = nullptr);
	GlTexture(GLsizei width, GLsizei height, QImage::Format imgFormat, const void * data = nullptr);
	GlTexture(const QImage& image);
	~GlTexture();
	GlTexture(GlTexture&& mov);
	GlTexture& operator=(GlTexture&& mov);
	GLsizei getWidth() const;
	GLsizei getHeight() const;
	const GlTextureFormat& getFormat() const;

	void bind() const;
	static GlTextureFormat getTextureFormat(QImage::Format imgFormat);
	static size_t getBytesPerPixel(GLint internalformat);
	size_t getBytesPerPixel() const;
	void initialize(GLsizei width, GLsizei height, const GlTextureFormat& format, const void * data = nullptr);
	void initialize(GLsizei width, GLsizei height, QImage::Format imgFormat, const void * data = nullptr);
	void modify(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const GlTextureFormat& format, const void * pixels);
	void modify(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, QImage::Format imgFormat, const void * pixels);
	void modify(GLsizei width, GLsizei height, const GlTextureFormat& format, const ScanlineIteratingFunction& scanlineGetter);
	void modify(GLsizei width, GLsizei height, QImage::Format imgFormat, const ScanlineIteratingFunction& scanlineGetter);
	void getTexture(const GlTextureFormat& format, void* pixels) const;
	void getTexture(QImage::Format imgFormat, void* pixels) const;
	void getTexture(void* pixels) const;
	QByteArray getTexture(const GlTextureFormat& format) const;
	QByteArray getTexture(QImage::Format imgFormat) const;
	QByteArray getTexture() const;
	template <typename T, typename Alloc = std::allocator<T>> std::vector<T,Alloc> getTextureAs() const {
		size_t elemSize = getBytesPerPixel() / sizeof(T);
		std::vector<T,Alloc> vec(elemSize * width * height);
		getTexture(vec.data());
		return vec;
	}
	void bindAsImage(QOpenGLExtraFunctions* extraFuncs, GLuint unit, GLenum access) const;
};

class GlStorageBuffer {
private:
	GLenum target;
	size_t size;
	GLuint buffId;
	QOpenGLFunctions* glFuncs;
	QOpenGLExtraFunctions* extraFuncs;
	GlStorageBuffer(const GlStorageBuffer& cpy) = delete;
	GlStorageBuffer& operator=(const GlStorageBuffer& cpy) = delete;
public:
	GlStorageBuffer(QOpenGLFunctions* glFuncs, QOpenGLExtraFunctions* extraFuncs, bool isSSBO=false);
	~GlStorageBuffer();
	GlStorageBuffer(GlStorageBuffer&& mov);
	GlStorageBuffer& operator=(GlStorageBuffer&& mov);
	void bind() const;
	void bindBase(GLuint index) const;
	void initialize(GLsizeiptr size, const void* data = nullptr);
	template <typename T> void initializeFrom(const T& data) {
		initialize(sizeof(T), static_cast<const void*>(&data));
	}
	void modify(GLintptr offset, GLsizeiptr size, const void * data);
	template <typename T> void modifyFrom(const T& data) {
		modify(0, sizeof(T), static_cast<const void*>(&data));
	}
	size_t getSize() const;
	GLenum getTarget() const;
	GLuint getBuffId() const;
};

#endif // GLHELPERS_HPP
