#include "MainWindow.hpp"
#include "ui_MainWindow.h"
#include <QFileDialog>
#include <QFile>
#include <QMessageBox>
#include <stdexcept>

namespace {

QByteArray magicBytes(const std::array<char,4>& magic)
{
	return QByteArray(magic.data(), static_cast<qsizetype>(magic.size()));
}

}

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, ui(new Ui::MainWindow), font_face(nullptr)
{
	connect(this,&MainWindow::changeState,this,&MainWindow::onStateChange);
	connect(this,&MainWindow::fontHasBeenLoaded,this,&MainWindow::onFontHasBeenLoaded);
	connect(this,&MainWindow::vecetorHasBeenLoaded,this,&MainWindow::onVectorHasBeenLoaded);
	ui->setupUi(this);
	highlighter = new argos::CQTOpenGLLuaSyntaxHighlighter(ui->glslEditor->document(), false);
	emit changeState(MainWindowState::INITIAL);
}

MainWindow::~MainWindow()
{
	delete ui;
}


void MainWindow::on_loadPathBtn_clicked()
{
	ui->loadPathEdit->setText(QFileDialog::getOpenFileName(this, tr("Open file"), QString(), tr("Font files (*.ttf *.otf);;WOD binary files (*.wodf *.wodi);;Binary files (*.bin);;CBOR files (*.cbor *.vcbor);;")));
}


void MainWindow::on_saveBtnPath_clicked()
{
	ui->savePathEdit->setText(QFileDialog::getSaveFileName(this, tr("Open file"), QString(), tr("WOD font files (*.wodf);;Binary files (*.bin);;CBOR files (*.cbor);;")));
}


void MainWindow::on_loadPathEdit_textChanged(const QString &arg1)
{
	ui->loadBtn->setEnabled( !arg1.isEmpty() );
}


void MainWindow::on_savePathEdit_textChanged(const QString &arg1)
{
	ui->saveBtn->setEnabled( !arg1.isEmpty() );
}


void MainWindow::on_loadBtn_clicked()
{
	QString loadPath = ui->loadPathEdit->text();
	if(loadPath.endsWith(QStringLiteral(".cbor"), Qt::CaseInsensitive) || loadPath.endsWith(QStringLiteral(".vcbor"), Qt::CaseInsensitive)) {
		try {
			loadCborFont(loadPath);
		} catch(std::exception& exp) {
			QMessageBox::critical(this, QStringLiteral("Error!"), QString::fromUtf8(exp.what()));
		}
	}
	else if(loadPath.endsWith(QStringLiteral(".wodf"), Qt::CaseInsensitive) || loadPath.endsWith(QStringLiteral(".wodi"), Qt::CaseInsensitive) || loadPath.endsWith(QStringLiteral(".bin"), Qt::CaseInsensitive)) {
		try {
			loadBinFont(loadPath);
		} catch(std::exception& exp) {
			QMessageBox::critical(this, QStringLiteral("Error!"), QString::fromUtf8(exp.what()));
		}
	}
	else {
		// Regular font?
	}
}

void MainWindow::onStateChange(int newState)
{
	this->state = static_cast<MainWindowState>(newState);
	switch (state) {
		case INITIAL: {
			ui->tabWidget->setEnabled(false);
			break;
		}
		case FONT_PATH_SET: {
			ui->tabWidget->setEnabled(true);
			ui->processFontTab->setEnabled(true);
			ui->displayFontTab->setEnabled(false);
			ui->shaderTestTab->setEnabled(false);
			break;
		}
		case PREPROCESSED_FONT_LAODED: {
			ui->tabWidget->setEnabled(true);
			ui->processFontTab->setEnabled(false);
			ui->displayFontTab->setEnabled(true);
			ui->shaderTestTab->setEnabled(true);
			break;
		}
		default:
			break;
	}
}

void MainWindow::onFontHasBeenLoaded()
{
	glyphsVector.clear();
	ui->selectGlyphComboBox1->clear();
	ui->selectGlyphComboBox2->clear();
	for(auto it = std::begin(font_face->storedCharacters); it != std::end(font_face->storedCharacters); ++it) {
		glyphsVector.push_back(it.key());
	}
	for(const auto it : glyphsVector) {
		ui->selectGlyphComboBox1->addItem(QString::number(it), it);
		ui->selectGlyphComboBox2->addItem(QString::number(it), it);
	}
	emit changeState(MainWindowState::PREPROCESSED_FONT_LAODED);
}

void MainWindow::onVectorHasBeenLoaded()
{
	glyphsVector.clear();
	ui->selectGlyphComboBox1->clear();
	ui->selectGlyphComboBox2->clear();
	int i = 0;
	for(auto it = std::begin(vector_face->mipmaps); it != std::end(vector_face->mipmaps); ++it) {
		glyphsVector.push_back(i++);
	}
	for(const auto it : glyphsVector) {
		ui->selectGlyphComboBox1->addItem(QString::number(it), it);
		ui->selectGlyphComboBox2->addItem(QString::number(it), it);
	}
	emit changeState(MainWindowState::PREPROCESSED_FONT_LAODED);
}

void MainWindow::loadCborFont(const QString& filepath)
{
	QFile fil(filepath);
	if(!fil.open(QFile::ReadOnly)) {
		throw std::runtime_error("Failed to open file for reading!");
	}
	QCborValue cborv = QCborValue::fromCbor(fil.readAll());
	fil.close();
	const QCborMap cbor = cborv.toMap();
	const QString containerType = cbor[CBOR_CONTAINER_TYPE_KEY].toString();
	if(containerType == CBOR_STORED_VECTOR_IMAGE_TYPE) {
		this->vector_face = std::make_unique<StoredVectorImage>();
		vector_face->fromCbor(cbor);
		this->font_face.reset();
		glyphsVector.clear();
		ui->selectGlyphComboBox1->clear();
		ui->selectGlyphComboBox2->clear();
		emit vecetorHasBeenLoaded();
		return;
	}
	if(containerType.isEmpty() || containerType == CBOR_PREPROCESSED_FONT_FACE_TYPE) {
		this->font_face = std::make_unique<PreprocessedFontFace>();
		font_face->fromCbor(cbor);
		this->vector_face.reset();
		emit fontHasBeenLoaded();
		return;
	}
	throw std::runtime_error(QStringLiteral("Unsupported CBOR container type: %1").arg(containerType).toStdString());
}

void MainWindow::loadBinFont(const QString& filepath)
{
	QFile fil(filepath);
	if(!fil.open(QFile::ReadOnly)) {
		throw std::runtime_error("Failed to open file for reading!");
	}
	const QByteArray magic = fil.peek(static_cast<qint64>(PreprocessedFontFace::BINARY_MAGIC.size()));
	QDataStream binF(&fil);
	binF.setVersion(QDataStream::Qt_4_0);
	binF.setByteOrder(QDataStream::BigEndian);
	if(magic == magicBytes(PreprocessedFontFace::BINARY_MAGIC)) {
		this->font_face = std::make_unique<PreprocessedFontFace>();
		font_face->fromData(binF);
		this->vector_face.reset();
		emit fontHasBeenLoaded();
		return;
	}
	if(magic == magicBytes(StoredVectorImage::BINARY_MAGIC)) {
		this->vector_face = std::make_unique<StoredVectorImage>();
		vector_face->fromData(binF);
		this->font_face.reset();
		glyphsVector.clear();
		ui->selectGlyphComboBox1->clear();
		ui->selectGlyphComboBox2->clear();
		emit vecetorHasBeenLoaded();
		return;
	}
	throw std::runtime_error("Unsupported binary container magic.");
}

/*
enum MainWindowState {
	INITIAL,
	FONT_PATH_SET,
	PREPROCESSED_FONT_LAODED
};
*/


void MainWindow::on_selectGlyphArrowLeft2_clicked()
{
	ui->selectGlyphComboBox2->setCurrentIndex( std::max( (ui->selectGlyphComboBox2->currentIndex() - 1), 0 ) );
}


void MainWindow::on_selectGlyphArrowRight2_clicked()
{
	ui->selectGlyphComboBox2->setCurrentIndex( std::min( (ui->selectGlyphComboBox2->currentIndex() + 1), ui->selectGlyphComboBox2->count() - 1) );
}


void MainWindow::on_selectGlyphArrowLeft1_clicked()
{
	ui->selectGlyphComboBox1->setCurrentIndex( std::max( (ui->selectGlyphComboBox1->currentIndex() - 1), 0 ) );
}


void MainWindow::on_selectGlyphArrowRight1_clicked()
{
	ui->selectGlyphComboBox1->setCurrentIndex( std::min( (ui->selectGlyphComboBox1->currentIndex() + 1), ui->selectGlyphComboBox1->count() - 1) );
}


void MainWindow::on_selectGlyphComboBox2_currentIndexChanged(int index)
{
	ui->selectGlyphArrowLeft2->setEnabled( ui->selectGlyphComboBox2->currentIndex() > 0);
	ui->selectGlyphArrowRight2->setEnabled( ui->selectGlyphComboBox2->currentIndex() < (ui->selectGlyphComboBox2->count()-1)  );
	if( font_face != nullptr && index >= 0 ) {
		// Let's display the image!
		auto it = font_face->storedCharacters.find(glyphsVector[index]);
		if( it != std::end(font_face->storedCharacters)) {
			ui->glyphShowLabel->setPixmap( QPixmap::fromImage( QImage::fromData( it.value().sdf ) ) );
		}
	} else if ( vector_face != nullptr && index >= 0 )
	{
		ui->glyphShowLabel->setPixmap( QPixmap::fromImage( QImage::fromData( vector_face->mipmaps[index] ) ) );
	}
}


void MainWindow::on_selectGlyphComboBox1_currentIndexChanged(int index)
{
	ui->selectGlyphArrowLeft1->setEnabled( ui->selectGlyphComboBox1->currentIndex() > 0);
	ui->selectGlyphArrowRight1->setEnabled( ui->selectGlyphComboBox1->currentIndex() < (ui->selectGlyphComboBox1->count()-1)  );
	if(index >= 0) {
		if(font_face) {
		auto it = font_face->storedCharacters.find(glyphsVector[index]);
		if( it != std::end(font_face->storedCharacters)) {
			ui->openGLWidget->addTexture(QImage::fromData(it->sdf));
			repaintGl();
		}
		} else if(vector_face) {
			ui->openGLWidget->addTexture(QImage::fromData(vector_face->mipmaps[index]));
			repaintGl();
		}
	}
}


void MainWindow::on_enableHlslBtn_checkStateChanged(const Qt::CheckState &arg1)
{
	switch(arg1) {
		case Qt::Unchecked: {
			ui->hlsl2GlslBtn->setEnabled(false);
			ui->hlslEditor->setEnabled(false);
			break;
		}
		case Qt::Checked: {
			ui->hlsl2GlslBtn->setEnabled(true);
			ui->hlslEditor->setEnabled(true);
			break;
		}
		default: break;
	}
}


void MainWindow::on_glslCompileBtn_clicked()
{
	QString errorLog;
	if(!ui->openGLWidget->addFragmentShader(ui->glslEditor->toPlainText(), errorLog)) {
		QMessageBox::critical(this, "Error!", errorLog);
	} else {
		repaintGl();
	}
}


void MainWindow::on_horizontalSlider_2_valueChanged(int value)
{
	this->clr.setGreenF(static_cast<float>(value) / static_cast<float>(ui->horizontalSlider_2->maximum() ) );
	ui->openGLWidget->setBackgroundCLr(clr);
	repaintGl();
}


void MainWindow::on_horizontalSlider_valueChanged(int value)
{
	this->clr.setRedF(static_cast<float>(value) / static_cast<float>(ui->horizontalSlider->maximum() ) );
	ui->openGLWidget->setBackgroundCLr(clr);
	repaintGl();
}


void MainWindow::on_horizontalSlider_3_valueChanged(int value)
{
	this->clr.setBlueF(static_cast<float>(value) / static_cast<float>(ui->horizontalSlider_3->maximum() ) );
	ui->openGLWidget->setBackgroundCLr(clr);
	repaintGl();
}


void MainWindow::on_horizontalSlider_4_valueChanged(int value)
{
	this->clr.setAlphaF(static_cast<float>(value) / static_cast<float>(ui->horizontalSlider_4->maximum() ) );
	ui->openGLWidget->setBackgroundCLr(clr);
	repaintGl();
}

void MainWindow::repaintGl()
{
	this->ui->openGLWidget->update();
}
