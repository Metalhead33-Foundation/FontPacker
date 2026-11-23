/**
 * @file OpenGLCanvas.hpp
 * @brief Qt OpenGL widget used to preview an SDF texture in the GUI.
 *
 * The canvas owns a single texture and a very small shader program. The main
 * window feeds it the decoded glyph/mip image and a fragment shader; the widget
 * only handles the quad, the texture binding and the background colour.
 */

#ifndef OPENGLCANVAS_HPP
#define OPENGLCANVAS_HPP
#include <QObject>
#include <QOpenGLWidget>
#include <QWidget>
#include <QOpenGLShaderProgram>
#include <QOpenGLShader>
#include <QOpenGLVertexArrayObject>
#include <QOpenGLBuffer>
#include "GlHelpers.hpp"
#include <memory>
#include <QColor>
#include <QRgb>

/**
 * @brief OpenGL widget that draws one texture with a user-supplied shader.
 * @class OpenGLCanvas
 */
class OpenGLCanvas : public QOpenGLWidget
{
	Q_OBJECT
private:
	QOpenGLFunctions* glFuncs;                  ///< Basic GL function table (owned by the context).
	QOpenGLExtraFunctions* extraFuncs;          ///< Extra GL function table (owned by the context).
	std::unique_ptr<GlTexture> tex;             ///< The previewed SDF texture.
	std::unique_ptr<QOpenGLShaderProgram> shdr; ///< Linked preview shader program.
	std::unique_ptr<QOpenGLShader> vertexShader;   ///< Vertex shader (from the static default source).
	std::unique_ptr<QOpenGLShader> fragmentShader; ///< Fragment shader supplied by the main window.
	QOpenGLVertexArrayObject m_vao;             ///< Quad vertex array object.
	QOpenGLBuffer m_vbo;                        ///< Quad vertex buffer.
	QOpenGLBuffer m_ebo;                        ///< Quad index buffer.
	QColor backgroundCLr;                       ///< Clear colour used by paintGL().
	static bool hasVertexShdr;                  ///< Whether vertexShdrSrc has been loaded.
	static QByteArray vertexShdrSrc;            ///< Default vertex shader source (from the Qt resource).

	/**
	 * @brief Release GL resources created by this widget.
	 *
	 * Called from the destructor with the context made current.
	 */
	void cleanup();

public:
	/// @brief Destructor; releases the GL resources.
	~OpenGLCanvas();

	/**
	 * @brief Construct the widget.
	 * @param parent Parent widget.
	 * @param f Window flags.
	 */
	OpenGLCanvas(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());

	/// @brief Initialise GL objects after the context has been created.
	void initializeGL() override;

	/// @brief Draw the current texture with the current shader.
	void paintGL() override;

	/**
	 * @brief Get the clear colour (const access).
	 * @return The current background colour.
	 */
	const QColor& getBackgroundCLr() const;

	/**
	 * @brief Get the clear colour (mutable access).
	 * @return The current background colour.
	 */
	QColor& getBackgroundCLr();

	/**
	 * @brief Set the clear colour.
	 * @param newBackgroundCLr New background colour.
	 */
	void setBackgroundCLr(const QColor& newBackgroundCLr);

	/**
	 * @brief Set the clear colour from a packed QRgb value.
	 * @param newBackgroundCLr New background colour.
	 */
	void setBackgroundCLr(QRgb newBackgroundCLr);

	/**
	 * @brief Get the default vertex shader source.
	 * @return The vertex shader source loaded from the Qt resource `:/screen.vert.glsl`.
	 */
	static QByteArray getVertexShaderSource();

	/**
	 * @brief Replace the fragment shader and relink the program.
	 * @param source Fragment shader source.
	 * @param errorLog Receives the compiler/linker log on failure.
	 * @return True on success.
	 */
	bool addFragmentShader(const QString& source, QString& errorLog);

	/**
	 * @brief Upload an image as the previewed texture.
	 * @param img Image to upload (converted to a supported format).
	 */
	void addTexture(const QImage& img);
};

#endif // OPENGLCANVAS_HPP
