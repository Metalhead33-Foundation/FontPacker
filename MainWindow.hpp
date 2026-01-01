/**
 * @file MainWindow.hpp
 * @brief Main GUI window for FontPacker application.
 * 
 * Provides a Qt-based graphical user interface for:
 * - Loading fonts and SVG files
 * - Configuring SDF generation parameters
 * - Previewing generated glyphs
 * - Saving preprocessed font data in various formats
 */

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP
#include <QMainWindow>
#include "ConstStrings.hpp"
#include "PreprocessedFontFace.hpp"
#include <memory>
#include <vector>
#include "CQTOpenGLLuaSyntaxHighlighter.hpp"

namespace Ui {
class MainWindow;
}

/**
 * @brief Application state enumeration.
 * @enum MainWindowState
 */
enum MainWindowState {
	INITIAL,                    ///< Initial state, no font loaded
	FONT_PATH_SET,             ///< Font path has been set
	PREPROCESSED_FONT_LAODED  ///< Preprocessed font has been loaded
};

/**
 * @brief Main application window class.
 * @class MainWindow
 */
class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	explicit MainWindow(QWidget *parent = nullptr);
	~MainWindow();

signals:
	void changeState(int newState);
	void fontHasBeenLoaded();

private slots:

	void on_loadPathBtn_clicked();

	void on_saveBtnPath_clicked();

	void on_loadPathEdit_textChanged(const QString &arg1);

	void on_savePathEdit_textChanged(const QString &arg1);

	void on_loadBtn_clicked();

	void onStateChange(int newState);
	void onFontHasBeenLoaded();

	void on_selectGlyphArrowLeft2_clicked();

	void on_selectGlyphArrowRight2_clicked();

	void on_selectGlyphArrowLeft1_clicked();

	void on_selectGlyphArrowRight1_clicked();

	void on_selectGlyphComboBox2_currentIndexChanged(int index);

	void on_selectGlyphComboBox1_currentIndexChanged(int index);

	void on_enableHlslBtn_checkStateChanged(const Qt::CheckState &arg1);

	void on_glslCompileBtn_clicked();

	void on_horizontalSlider_2_valueChanged(int value);

	void on_horizontalSlider_valueChanged(int value);

	void on_horizontalSlider_3_valueChanged(int value);

	void on_horizontalSlider_4_valueChanged(int value);

	void repaintGl();

private:
	std::vector<uint32_t> glyphsVector;
	Ui::MainWindow *ui;
	MainWindowState state = INITIAL;
	std::unique_ptr<PreprocessedFontFace> font_face;
	void loadCborFont(const QString& filepath);
	void loadBinFont(const QString& filepath);
	QColor clr;
	argos::CQTOpenGLLuaSyntaxHighlighter* highlighter;
};

#endif // MAINWINDOW_HPP
