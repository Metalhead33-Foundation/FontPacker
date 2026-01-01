/**
 * @file OpenGLCanvas.hpp
 * @brief OpenGL widget for displaying SDF textures in the GUI.
 * 
 * Provides a Qt OpenGL widget for rendering and displaying signed distance field
 * textures. Used in the main window for glyph preview functionality.
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
 * @brief OpenGL widget for rendering SDF textures.
 * @class OpenGLCanvas
 */
class OpenGLCanvas : public QOpenGLWidget
{
	Q_OBJECT
private:
	QOpenGLFunctions* glFuncs;
	QOpenGLExtraFunctions* extraFuncs;
	std::unique_ptr<GlTexture> tex;
	std::unique_ptr<QOpenGLShaderProgram> shdr;
	std::unique_ptr<QOpenGLShader> vertexShader;
	std::unique_ptr<QOpenGLShader> fragmentShader;
	QOpenGLVertexArrayObject m_vao;
	QOpenGLBuffer m_vbo;
	QOpenGLBuffer m_ebo;
	void cleanup();
	QColor backgroundCLr;
	static bool hasVertexShdr;
	static QByteArray vertexShdrSrc;
public:
	~OpenGLCanvas();
	OpenGLCanvas(QWidget *parent = nullptr, Qt::WindowFlags f = Qt::WindowFlags());
	void initializeGL() override;
	void paintGL() override;
	const QColor& getBackgroundCLr() const;
	QColor& getBackgroundCLr();
	void setBackgroundCLr(const QColor& newBackgroundCLr);
	void setBackgroundCLr(QRgb newBackgroundCLr);
	static QByteArray getVertexShaderSource();
	bool addFragmentShader(const QString& source, QString& errorLog);
	void addTexture(const QImage& img);
};

#endif // OPENGLCANVAS_HPP
