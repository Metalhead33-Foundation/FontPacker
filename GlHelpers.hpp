/**
 * @file GlHelpers.hpp
 * @brief OpenGL helper classes for texture and buffer management.
 * 
 * Provides RAII wrappers for OpenGL resources including textures,
 * storage buffers, and OpenGL context management.
 */

#ifndef GLHELPERS_HPP
#define GLHELPERS_HPP
#include <QOpenGLFunctions>
#include <QOpenGLShaderProgram>
#include <QOffscreenSurface>
#include <QImage>
#include <QOpenGLVersionFunctionsFactory>
#include <QOpenGLFunctions_4_3_Core>
#include <QImage>
#include <QByteArray>
#include <vector>
#include <functional>
#include <span>

/**
 * @brief Helper structure for managing OpenGL context and function pointers.
 * @struct GlHelpers
 * 
 * Provides access to OpenGL functions and manages an offscreen rendering context.
 * Non-copyable, but supports move semantics.
 */
struct GlHelpers {
private:
	GlHelpers(const GlHelpers& cpy) = delete;
	GlHelpers& operator=(const GlHelpers& cpy) = delete;
	
public:
	QOpenGLFunctions* glFuncs;              ///< Basic OpenGL functions
	QOpenGLExtraFunctions* extraFuncs;      ///< Extended OpenGL functions
	QOpenGLFunctions_4_3_Core* gl43Funcs;   ///< OpenGL 4.3 core functions (for compute shaders)
	std::unique_ptr<QOpenGLContext> glContext;  ///< OpenGL context
	std::unique_ptr<QOffscreenSurface> glSurface; ///< Offscreen rendering surface
	
	/**
	 * @brief Constructor - initializes OpenGL context and function pointers.
	 */
	GlHelpers();
	
	/**
	 * @brief Move constructor.
	 * @param mov Source to move from.
	 */
	GlHelpers(GlHelpers&& mov);
	
	/**
	 * @brief Move assignment operator.
	 * @param mov Source to move from.
	 * @return Reference to this.
	 */
	GlHelpers& operator=(GlHelpers&& mov);
};

/**
 * @brief OpenGL texture format specification.
 * @struct GlTextureFormat
 */
struct GlTextureFormat
{
	GLint internalformat;  ///< Internal texture format (e.g., GL_RGBA8)
	GLenum format;         ///< Pixel format (e.g., GL_RGBA)
	GLenum type;           ///< Pixel data type (e.g., GL_UNSIGNED_BYTE)
};

/**
 * @brief RAII wrapper for OpenGL textures.
 * @class GlTexture
 * 
 * Manages OpenGL texture objects with automatic cleanup. Supports creation
 * from QImage, raw data, or with specific formats. Non-copyable, but supports move semantics.
 */
class GlTexture {
public:
	/**
	 * @brief Function type for scanline-based texture modification.
	 * @param scanline Scanline index (0-based).
	 * @return Pointer to scanline data.
	 */
	typedef std::function<const void*(GLsizei scanline)> ScanlineIteratingFunction;
	
private:
	GLuint texId;           ///< OpenGL texture ID
	GLsizei width;          ///< Texture width
	GLsizei height;         ///< Texture height
	GlTextureFormat format; ///< Texture format
	GlTexture(const GlTexture& cpy) = delete;
	GlTexture& operator=(const GlTexture& cpy) = delete;
	
public:
	/**
	 * @brief Get the OpenGL texture ID.
	 * @return Texture ID.
	 */
	GLuint getTexId() const;
	
	/**
	 * @brief Default constructor - creates an invalid texture.
	 */
	GlTexture();
	
	/**
	 * @brief Construct texture with specified format.
	 * @param width Texture width.
	 * @param height Texture height.
	 * @param format Texture format.
	 * @param data Initial pixel data (nullptr for uninitialized).
	 */
	GlTexture(GLsizei width, GLsizei height, const GlTextureFormat& format, const void * data = nullptr);
	
	/**
	 * @brief Construct texture from QImage format.
	 * @param width Texture width.
	 * @param height Texture height.
	 * @param imgFormat QImage format.
	 * @param data Initial pixel data (nullptr for uninitialized).
	 */
	GlTexture(GLsizei width, GLsizei height, QImage::Format imgFormat, const void * data = nullptr);
	
	/**
	 * @brief Construct texture from QImage.
	 * @param image Source image.
	 */
	GlTexture(const QImage& image);
	
	/**
	 * @brief Destructor - deletes OpenGL texture.
	 */
	~GlTexture();
	
	/**
	 * @brief Move constructor.
	 * @param mov Source to move from.
	 */
	GlTexture(GlTexture&& mov);
	
	/**
	 * @brief Move assignment operator.
	 * @param mov Source to move from.
	 * @return Reference to this.
	 */
	GlTexture& operator=(GlTexture&& mov);
	
	/**
	 * @brief Get texture width.
	 * @return Width in pixels.
	 */
	GLsizei getWidth() const;
	
	/**
	 * @brief Get texture height.
	 * @return Height in pixels.
	 */
	GLsizei getHeight() const;
	
	/**
	 * @brief Get texture format.
	 * @return Format specification.
	 */
	const GlTextureFormat& getFormat() const;

	/**
	 * @brief Bind texture to active texture unit.
	 */
	void bind() const;
	
	/**
	 * @brief Convert QImage format to OpenGL texture format.
	 * @param imgFormat QImage format.
	 * @return OpenGL texture format.
	 */
	static GlTextureFormat getTextureFormat(QImage::Format imgFormat);
	
	/**
	 * @brief Get bytes per pixel for an internal format.
	 * @param internalformat OpenGL internal format.
	 * @return Bytes per pixel.
	 */
	static size_t getBytesPerPixel(GLint internalformat);
	
	/**
	 * @brief Get bytes per pixel for this texture.
	 * @return Bytes per pixel.
	 */
	size_t getBytesPerPixel() const;
	
	/**
	 * @brief Initialize or reinitialize texture.
	 * @param width Texture width.
	 * @param height Texture height.
	 * @param format Texture format.
	 * @param data Initial pixel data (nullptr for uninitialized).
	 */
	void initialize(GLsizei width, GLsizei height, const GlTextureFormat& format, const void * data = nullptr);
	
	/**
	 * @brief Initialize or reinitialize texture from QImage format.
	 * @param width Texture width.
	 * @param height Texture height.
	 * @param imgFormat QImage format.
	 * @param data Initial pixel data (nullptr for uninitialized).
	 */
	void initialize(GLsizei width, GLsizei height, QImage::Format imgFormat, const void * data = nullptr);
	
	/**
	 * @brief Modify a subregion of the texture.
	 * @param xoffset X offset of the subregion.
	 * @param yoffset Y offset of the subregion.
	 * @param width Width of the subregion.
	 * @param height Height of the subregion.
	 * @param format Pixel format.
	 * @param pixels Pixel data.
	 */
	void modify(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, const GlTextureFormat& format, const void * pixels);
	
	/**
	 * @brief Modify a subregion of the texture from QImage format.
	 * @param xoffset X offset of the subregion.
	 * @param yoffset Y offset of the subregion.
	 * @param width Width of the subregion.
	 * @param height Height of the subregion.
	 * @param imgFormat QImage format.
	 * @param pixels Pixel data.
	 */
	void modify(GLint xoffset, GLint yoffset, GLsizei width, GLsizei height, QImage::Format imgFormat, const void * pixels);
	
	/**
	 * @brief Modify entire texture using scanline iterator.
	 * @param width Texture width.
	 * @param height Texture height.
	 * @param format Pixel format.
	 * @param scanlineGetter Function to get scanline data.
	 */
	void modify(GLsizei width, GLsizei height, const GlTextureFormat& format, const ScanlineIteratingFunction& scanlineGetter);
	
	/**
	 * @brief Modify entire texture using scanline iterator with QImage format.
	 * @param width Texture width.
	 * @param height Texture height.
	 * @param imgFormat QImage format.
	 * @param scanlineGetter Function to get scanline data.
	 */
	void modify(GLsizei width, GLsizei height, QImage::Format imgFormat, const ScanlineIteratingFunction& scanlineGetter);
	
	/**
	 * @brief Modify entire texture from QImage.
	 * @param qimg Source image.
	 */
	void modify(const QImage& qimg);
	
	/**
	 * @brief Read texture data with specified format.
	 * @param format Pixel format.
	 * @param pixels Output buffer.
	 */
	void getTexture(const GlTextureFormat& format, void* pixels) const;
	
	/**
	 * @brief Read texture data with QImage format.
	 * @param imgFormat QImage format.
	 * @param pixels Output buffer.
	 */
	void getTexture(QImage::Format imgFormat, void* pixels) const;
	
	/**
	 * @brief Read texture data using texture's native format.
	 * @param pixels Output buffer.
	 */
	void getTexture(void* pixels) const;
	
	/**
	 * @brief Read texture data as QByteArray with specified format.
	 * @param format Pixel format.
	 * @return Byte array containing pixel data.
	 */
	QByteArray getTexture(const GlTextureFormat& format) const;
	
	/**
	 * @brief Read texture data as QByteArray with QImage format.
	 * @param imgFormat QImage format.
	 * @return Byte array containing pixel data.
	 */
	QByteArray getTexture(QImage::Format imgFormat) const;
	
	/**
	 * @brief Read texture data as QByteArray using native format.
	 * @return Byte array containing pixel data.
	 */
	QByteArray getTexture() const;
	
	/**
	 * @brief Read texture data as a vector of a specific type.
	 * @tparam T Element type.
	 * @tparam Alloc Allocator type.
	 * @return Vector containing texture data.
	 */
	template <typename T, typename Alloc = std::allocator<T>> std::vector<T,Alloc> getTextureAs() const {
		size_t elemSize = getBytesPerPixel() / sizeof(T);
		std::vector<T,Alloc> vec(elemSize * width * height);
		getTexture(vec.data());
		return vec;
	}
	
	/**
	 * @brief Bind texture as an image for compute shader access.
	 * @param extraFuncs OpenGL extra functions pointer.
	 * @param unit Image unit index.
	 * @param access Access mode (GL_READ_ONLY, GL_WRITE_ONLY, GL_READ_WRITE).
	 */
	void bindAsImage(QOpenGLExtraFunctions* extraFuncs, GLuint unit, GLenum access) const;
};

/**
 * @brief RAII wrapper for OpenGL storage buffers (UBO/SSBO).
 * @class GlStorageBuffer
 * 
 * Manages OpenGL uniform buffer objects (UBO) or shader storage buffer objects (SSBO).
 * Non-copyable, but supports move semantics.
 */
class GlStorageBuffer {
private:
	GLenum target;                    ///< Buffer target (GL_UNIFORM_BUFFER or GL_SHADER_STORAGE_BUFFER)
	size_t size;                      ///< Buffer size in bytes
	GLuint buffId;                    ///< OpenGL buffer ID
	QOpenGLFunctions* glFuncs;        ///< OpenGL functions pointer
	QOpenGLExtraFunctions* extraFuncs; ///< OpenGL extra functions pointer
	GlStorageBuffer(const GlStorageBuffer& cpy) = delete;
	GlStorageBuffer& operator=(const GlStorageBuffer& cpy) = delete;
	
public:
	/**
	 * @brief Constructor.
	 * @param glFuncs OpenGL functions pointer.
	 * @param extraFuncs OpenGL extra functions pointer.
	 * @param isSSBO If true, create SSBO; if false, create UBO.
	 */
	GlStorageBuffer(QOpenGLFunctions* glFuncs, QOpenGLExtraFunctions* extraFuncs, bool isSSBO=false);
	
	/**
	 * @brief Destructor - deletes OpenGL buffer.
	 */
	~GlStorageBuffer();
	
	/**
	 * @brief Move constructor.
	 * @param mov Source to move from.
	 */
	GlStorageBuffer(GlStorageBuffer&& mov);
	
	/**
	 * @brief Move assignment operator.
	 * @param mov Source to move from.
	 * @return Reference to this.
	 */
	GlStorageBuffer& operator=(GlStorageBuffer&& mov);
	
	/**
	 * @brief Bind buffer to its target.
	 */
	void bind() const;
	
	/**
	 * @brief Bind buffer to a specific index (for UBO/SSBO binding points).
	 * @param index Binding index.
	 */
	void bindBase(GLuint index) const;
	
	/**
	 * @brief Initialize or reinitialize buffer.
	 * @param size Buffer size in bytes.
	 * @param data Initial data (nullptr for uninitialized).
	 */
	void initialize(GLsizeiptr size, const void* data = nullptr);
	
	/**
	 * @brief Initialize buffer from a span of data.
	 * @tparam T Element type.
	 * @param data Span of data to copy.
	 */
	template <typename T> void initializeFromSpan(const std::span<const T>& data) {
		initialize(data.size_bytes(),data.data());
	}
	
	/**
	 * @brief Initialize buffer from a single object.
	 * @tparam T Object type.
	 * @param data Object to copy.
	 */
	template <typename T> void initializeFrom(const T& data) {
		initialize(sizeof(T), static_cast<const void*>(&data));
	}
	
	/**
	 * @brief Modify a portion of the buffer.
	 * @param offset Byte offset into the buffer.
	 * @param size Number of bytes to modify.
	 * @param data Source data.
	 */
	void modify(GLintptr offset, GLsizeiptr size, const void * data);
	
	/**
	 * @brief Modify buffer from a span (replaces entire buffer).
	 * @tparam T Element type.
	 * @param data Span of data.
	 */
	template <typename T> void modifyFromSpan(const std::span<const T>& data)  {
		modify(0, sizeof(T), data.size_bytes(), data.data() );
	}
	
	/**
	 * @brief Modify buffer from a single object (replaces entire buffer).
	 * @tparam T Object type.
	 * @param data Object to copy.
	 */
	template <typename T> void modifyFrom(const T& data) {
		modify(0, sizeof(T), static_cast<const void*>(&data));
	}
	
	/**
	 * @brief Get buffer size.
	 * @return Size in bytes.
	 */
	size_t getSize() const;
	
	/**
	 * @brief Get buffer target.
	 * @return OpenGL target constant.
	 */
	GLenum getTarget() const;
	
	/**
	 * @brief Get buffer ID.
	 * @return OpenGL buffer ID.
	 */
	GLuint getBuffId() const;
};

#endif // GLHELPERS_HPP
