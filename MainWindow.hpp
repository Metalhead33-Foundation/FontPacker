#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP
#include <QMainWindow>
#include "ConstStrings.hpp"
#include "ProcessFonts.hpp"
#include <memory>
#include <vector>

namespace Ui {
class MainWindow;
}

enum MainWindowState {
	INITIAL,
	FONT_PATH_SET,
	PREPROCESSED_FONT_LAODED
};

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
};

#endif // MAINWINDOW_HPP
