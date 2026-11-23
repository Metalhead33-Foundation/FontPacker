/**
 * @file MainWindow.hpp
 * @brief Qt GUI for loading fonts/vector images and previewing their SDF data.
 *
 * The window keeps a `PreprocessedFontFace` and/or a `StoredVectorImage` loaded
 * from CBOR or binary files, exposes their glyphs through combo boxes, and feeds
 * the selected image plus a user-editable fragment shader to an OpenGLCanvas.
 */

#ifndef MAINWINDOW_HPP
#define MAINWINDOW_HPP
#include <QMainWindow>
#include "ConstStrings.hpp"
#include "PreprocessedFontFace.hpp"
#include "StoredVectorImage.hpp"
#include <memory>
#include <vector>

class QComboBox;
class QPushButton;
#include "CQTOpenGLShaderSyntaxHighlighter.hpp"

namespace Ui {
class MainWindow;
}

/**
 * @brief UI state of the main window.
 * @enum MainWindowState
 */
enum MainWindowState {
	INITIAL,                    ///< Nothing loaded yet.
	FONT_PATH_SET,              ///< A font path has been entered but not loaded.
	PREPROCESSED_FONT_LAODED    ///< A preprocessed font or vector image is loaded (name kept for source compatibility).
};

/**
 * @brief Main application window.
 * @class MainWindow
 */
class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	/**
	 * @brief Construct the window and connect its UI signals.
	 * @param parent Parent widget.
	 */
	explicit MainWindow(QWidget *parent = nullptr);

	/// @brief Destructor.
	~MainWindow();

signals:
	/**
	 * @brief Emitted when the UI state machine should move to another state.
	 * @param newState One of the MainWindowState values.
	 */
	void changeState(int newState);

	/// @brief Emitted after a PreprocessedFontFace has been loaded successfully.
	void fontHasBeenLoaded();

	/// @brief Emitted after a StoredVectorImage has been loaded successfully (name kept for source compatibility).
	void vecetorHasBeenLoaded();

private slots:
	/// @brief Browse for the input file and put its path in the load edit.
	void on_loadPathBtn_clicked();

	/// @brief Browse for the output file and put its path in the save edit.
	void on_saveBtnPath_clicked();

	/**
	 * @brief React to manual editing of the input path.
	 * @param arg1 New text of the edit.
	 */
	void on_loadPathEdit_textChanged(const QString &arg1);

	/**
	 * @brief React to manual editing of the output path.
	 * @param arg1 New text of the edit.
	 */
	void on_savePathEdit_textChanged(const QString &arg1);

	/// @brief Load the file referenced by the input path.
	void on_loadBtn_clicked();

	/**
	 * @brief Apply a state transition to the UI controls.
	 * @param newState Target state.
	 */
	void onStateChange(int newState);

	/// @brief Enable the font-related parts of the UI after a font load.
	void onFontHasBeenLoaded();

	/// @brief Enable the vector-related parts of the UI after a vector-image load.
	void onVectorHasBeenLoaded();

	/// @brief Select the previous glyph in the second glyph selector.
	void on_selectGlyphArrowLeft2_clicked();

	/// @brief Select the next glyph in the second glyph selector.
	void on_selectGlyphArrowRight2_clicked();

	/// @brief Select the previous glyph in the first glyph selector.
	void on_selectGlyphArrowLeft1_clicked();

	/// @brief Select the next glyph in the first glyph selector.
	void on_selectGlyphArrowRight1_clicked();

	/**
	 * @brief Switch the second preview to the selected glyph index.
	 * @param index Index into the glyph selector.
	 */
	void on_selectGlyphComboBox2_currentIndexChanged(int index);

	/**
	 * @brief Switch the first preview to the selected glyph index.
	 * @param index Index into the glyph selector.
	 */
	void on_selectGlyphComboBox1_currentIndexChanged(int index);

	/**
	 * @brief Toggle HLSL/GLSL mode of the shader editor.
	 * @param arg1 New check state.
	 */
	void on_enableHlslBtn_checkStateChanged(const Qt::CheckState &arg1);

	/// @brief Compile the shader editor contents and upload the fragment shader.
	void on_glslCompileBtn_clicked();

	/// @brief Open a shader file into the GLSL or HLSL editor.
	void onLoadShaderBtnClicked();

	/// @brief Save the active shader editor contents to a file.
	void onSaveShaderBtnClicked();

	/// @brief Show the short in-application shader tutorial.
	void onShaderTutorialBtnClicked();

	/**
	 * @brief Load the selected built-in shader example into both editors.
	 * @param index Example index.
	 */
	void onShaderExampleComboBoxCurrentIndexChanged(int index);

	/**
	 * @brief React to the second preview's zoom slider.
	 * @param value New slider value.
	 */
	void on_horizontalSlider_2_valueChanged(int value);

	/**
	 * @brief React to the first preview's zoom slider.
	 * @param value New slider value.
	 */
	void on_horizontalSlider_valueChanged(int value);

	/**
	 * @brief React to another preview slider.
	 * @param value New slider value.
	 */
	void on_horizontalSlider_3_valueChanged(int value);

	/**
	 * @brief React to another preview slider.
	 * @param value New slider value.
	 */
	void on_horizontalSlider_4_valueChanged(int value);

	/// @brief Re-upload the current texture and repaint the OpenGL preview.
	void repaintGl();

	/// @brief Transpile the HLSL editor contents into the GLSL editor.
	void onHlsl2GlslBtnClicked();

private:
	std::vector<uint32_t> glyphsVector;          ///< Code points of the loaded glyphs, in display order.
	Ui::MainWindow *ui;                          ///< Generated UI object.
	MainWindowState state = INITIAL;             ///< Current UI state.
	std::unique_ptr<PreprocessedFontFace> font_face; ///< Loaded font face, if any.
	std::unique_ptr<StoredVectorImage> vector_face;  ///< Loaded vector image, if any.
	QColor clr;                                  ///< Colour currently passed to the preview shader.
	argos::CQTOpenGLShaderSyntaxHighlighter* glslHighlighter = nullptr; ///< GLSL editor highlighter.
	argos::CQTOpenGLShaderSyntaxHighlighter* hlslHighlighter = nullptr; ///< HLSL editor highlighter.

	QComboBox* shaderExampleComboBox = nullptr; ///< Built-in example selector.
	QPushButton* shaderTutorialBtn = nullptr;   ///< Short tutorial popup button.
	QPushButton* shaderLoadBtn = nullptr;       ///< Shader file load button.
	QPushButton* shaderSaveBtn = nullptr;       ///< Shader file save button.

	/** @brief Create the shader-tab helper controls programmatically. */
	void setupShaderTools();

	/**
	 * @brief Load a font face from a CBOR file.
	 * @param filepath Path to the `.cbor` file.
	 */
	void loadCborFont(const QString& filepath);

	/**
	 * @brief Load a font face from a `.wodf` binary file.
	 * @param filepath Path to the `.wodf` file.
	 */
	void loadBinFont(const QString& filepath);
};

#endif // MAINWINDOW_HPP
