#include "OpenGLCanvas.hpp"
#include "qopenglextrafunctions.h"
#include <QFile>
#include <QMessageBox>

/*
	std::unique_ptr<GlTexture> tex;
	std::unique_ptr<QOpenGLShaderProgram> shdr;
	std::unique_ptr<QOpenGLShader> vertexShader;
	std::unique_ptr<QOpenGLShader> fragmentShader;
	QOpenGLVertexArrayObject m_vao;
	QOpenGLBuffer m_vbo;
*/
void OpenGLCanvas::cleanup()
{
	makeCurrent();
	tex.reset();
	shdr.reset();
	vertexShader.reset();
	fragmentShader.reset();
	m_vbo.destroy();
	m_ebo.destroy();
	m_vao.destroy();
	doneCurrent();
}

const QColor& OpenGLCanvas::getBackgroundCLr() const
{
	return backgroundCLr;
}

QColor& OpenGLCanvas::getBackgroundCLr()
{
	return backgroundCLr;
}

void OpenGLCanvas::setBackgroundCLr(const QColor& newBackgroundCLr)
{
	backgroundCLr = newBackgroundCLr;
}

void OpenGLCanvas::setBackgroundCLr(QRgb newBackgroundCLr)
{
	backgroundCLr = QColor::fromRgb(newBackgroundCLr);
}

QByteArray OpenGLCanvas::getVertexShaderSource()
{
	if(!hasVertexShdr) {
		QFile fil(":/screen.vert.glsl");
		if(fil.open(QFile::ReadOnly)) {
			hasVertexShdr = true;
			vertexShdrSrc = fil.readAll();
			fil.close();
		} else throw std::runtime_error("Error setting the default Vertex Shader source code!");
	}
	return vertexShdrSrc;
}

bool OpenGLCanvas::addFragmentShader(const QString& source, QString& errorLog)
{
	if(source.isEmpty()) return false;
	makeCurrent();
	bool returned = true;
	fragmentShader = std::make_unique<QOpenGLShader>(QOpenGLShader::Fragment);
	if(!fragmentShader->compileSourceCode(source)) {
		errorLog = fragmentShader->log();
		returned = false;
	}
	if(returned) {
		shdr = std::make_unique<QOpenGLShaderProgram>();
		shdr->addShader(vertexShader.get());
		shdr->addShader(fragmentShader.get());
		if(!shdr->link()) {
			errorLog = shdr->log();
			returned = false;
		}
	}
	doneCurrent();
	return returned;
}

void OpenGLCanvas::addTexture(const QImage& img)
{
	makeCurrent();
	this->tex = std::make_unique<GlTexture>(img);
	tex->bind();
	glFuncs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_MIRRORED_REPEAT);
	glFuncs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_MIRRORED_REPEAT);
	glFuncs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glFuncs->glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	doneCurrent();
}

/*
	static bool hasVertexShdr;
	static QByteArray vertexShdrSrc;
*/

bool OpenGLCanvas::hasVertexShdr = false;
QByteArray OpenGLCanvas::vertexShdrSrc;

OpenGLCanvas::~OpenGLCanvas()
{
	cleanup();
}

struct Vec2f {
	float x;
	float y;
};

struct Vertex {
	Vec2f pos;
	Vec2f tex;
};

const Vertex vertices[4] = {
	{ {-1, 1}, { 0, 0} },
	{ {1, 1}, { 1, 0} },
	{ {-1, -1}, { 0, 1} },
	{ {1, -1}, { 1, 1} },
};
const unsigned indices[6] = {
	0, 1, 2, 1, 2, 3
};

OpenGLCanvas::OpenGLCanvas(QWidget* parent, Qt::WindowFlags f)
	: QOpenGLWidget(parent, f),
	m_vbo(QOpenGLBuffer::VertexBuffer),
	m_ebo(QOpenGLBuffer::IndexBuffer),
	backgroundCLr(0, 0, 0, 255) {
}

void OpenGLCanvas::initializeGL()
{
	this->glFuncs = QOpenGLContext::currentContext()->functions();
	this->extraFuncs = QOpenGLContext::currentContext()->extraFunctions();
	glFuncs->glEnable(GL_BLEND);
	glFuncs->glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	m_vao.create();
	if (m_vao.isCreated()) {
		m_vao.bind();
	}

	m_vbo.create();
	m_vbo.setUsagePattern(QOpenGLBuffer::UsagePattern::StaticDraw);
	m_vbo.bind();
	m_vbo.allocate(vertices, sizeof(Vertex) * 4);

	m_ebo.create();
	m_ebo.setUsagePattern(QOpenGLBuffer::UsagePattern::StaticDraw);
	m_ebo.bind();
	m_ebo.allocate(indices,sizeof(unsigned) * 6);

	if (m_vao.isCreated()) {
		m_vao.bind();
		extraFuncs->glEnableVertexAttribArray(0);
		extraFuncs->glVertexAttribPointer(0, 2,  GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>( offsetof(Vertex,pos) )  );
		extraFuncs->glEnableVertexAttribArray(1);
		extraFuncs->glVertexAttribPointer(1, 2,  GL_FLOAT, GL_FALSE, sizeof(Vertex), reinterpret_cast<void*>( offsetof(Vertex,tex) )  );
	}

	vertexShader = std::make_unique<QOpenGLShader>(QOpenGLShader::Vertex);;
	if(!vertexShader->compileSourceCode(getVertexShaderSource())) {
		QMessageBox::critical(this,"Error!",vertexShader->log());
	}
}

void OpenGLCanvas::paintGL()
{
	this->glFuncs = QOpenGLContext::currentContext()->functions();
	this->extraFuncs = QOpenGLContext::currentContext()->extraFunctions();
	glFuncs->glClearColor(backgroundCLr.redF(), backgroundCLr.greenF(), backgroundCLr.blueF(), backgroundCLr.alphaF() );
	glFuncs->glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
	if(tex && shdr && m_vao.isCreated() && m_ebo.isCreated()) {
		glFuncs->glActiveTexture(GL_TEXTURE0); // activate the texture unit first before binding texture
		tex->bind();
		shdr->bind();
		int sdfLoc = shdr->attributeLocation("sdf_tex");
		//if(sdfLoc < 0) throw std::runtime_error("Something went terribly wrong! Why is there no `sdf_tex` present?");
		shdr->setUniformValue(sdfLoc, 0);
		// Ok, now we render!
		m_vao.bind();
		m_ebo.bind();
		//glFuncs->glDrawArrays(GL_TRIANGLES,0,3);
		glFuncs->glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
	}
}
