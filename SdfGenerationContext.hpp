/**
 * @file SdfGenerationContext.hpp
 * @brief Abstract base class for generating signed distance fields from fonts and SVG files.
 * 
 * This class provides the interface and common functionality for SDF generation.
 * Implementations include software-based (CPU) and OpenGL compute shader-based (GPU) rendering.
 */

#ifndef SDFGENERATIONCONTEXT_HPP
#define SDFGENERATIONCONTEXT_HPP
#include <QImage>
#include <glm/glm.hpp>
#include "SDFGenerationArguments.hpp"
#include "PreprocessedFontFace.hpp"
#include "StoredCharacter.hpp"
#include "FontOutlineDecompositionContext.hpp"
#include <harfbuzz/hb-ft.h>
#include <ft2build.h>
#include FT_FREETYPE_H
#include <freetype/ftoutln.h>

struct FT_Bitmap_;
struct svgtiny_shape;

/**
 * @brief Abstract base class for SDF generation contexts.
 * 
 * Provides common functionality for processing fonts and SVG files into signed distance fields.
 * Subclasses implement the actual SDF generation algorithms (software or GPU-based).
 */
class SdfGenerationContext
{
private:
	/**
	 * @brief Finalize outline glyph processing and generate SDF.
	 * @param output Output character structure to populate.
	 * @param args Generation arguments.
	 * @param flipY Whether to flip Y coordinates (default: true).
	 */
	void processOutlineGlyphEnd(StoredCharacter& output, const SDFGenerationArguments& args, bool flipY = true);
	
protected:
	FT_Library library;                                    ///< FreeType library instance
	FontOutlineDecompositionContext decompositionContext;  ///< Context for decomposing font outlines
	
public:
	/**
	 * @brief Constructor - initializes FreeType library.
	 */
	SdfGenerationContext();
	
	/**
	 * @brief Virtual destructor.
	 */
	virtual ~SdfGenerationContext();
	
	/**
	 * @brief Downsample image by averaging pixel values.
	 * @param src Source image to downsample.
	 * @return Downsampled image.
	 */
	static QImage downsampleImageByAveraging(const QImage& src);
	
	/**
	 * @brief Downsample image by taking maximum pixel values.
	 * @param src Source image to downsample.
	 * @return Downsampled image.
	 */
	static QImage dowsanmpleImageByMaxing(const QImage& src);
	
	/**
	 * @brief Generate SDF from a bitmap image (pure virtual).
	 * @param source Source bitmap image.
	 * @param args Generation arguments.
	 * @return Generated SDF image.
	 */
	virtual QImage produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args) = 0;
	
	/**
	 * @brief Generate SDF from font outline decomposition (pure virtual).
	 * @param source Font outline decomposition context.
	 * @param args Generation arguments.
	 * @return Generated SDF image.
	 */
	virtual QImage produceOutlineSdf(const FontOutlineDecompositionContext& source, const SDFGenerationArguments& args) = 0;
	
	/**
	 * @brief Process a glyph from FreeType outline data.
	 * @param output Output character structure to populate.
	 * @param glyphSlot FreeType glyph slot containing outline data.
	 * @param args Generation arguments.
	 */
	void processOutlineGlyph(StoredCharacter& output, FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args);
	
	/**
	 * @brief Process a glyph from FreeType bitmap data.
	 * @param output Output character structure to populate.
	 * @param glyphSlot FreeType glyph slot containing bitmap data.
	 * @param args Generation arguments.
	 */
	void processBitmapGlyph(StoredCharacter& output, FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args);
	
	/**
	 * @brief Process an entire font file and generate glyphs.
	 * @param output Preprocessed font face to populate.
	 * @param args Generation arguments.
	 */
	void processFont(PreprocessedFontFace& output, const SDFGenerationArguments& args);
	
	/**
	 * @brief Process an SVG file and generate glyphs.
	 * @param output Preprocessed font face to populate.
	 * @param buff SVG file data.
	 * @param args Generation arguments.
	 */
	void processSvg(PreprocessedFontFace& output, const QByteArray& buff, const SDFGenerationArguments& args);
	
	/**
	 * @brief Process a single SVG shape into a glyph.
	 * @param output Output character structure to populate.
	 * @param shape SVG shape to process.
	 * @param args Generation arguments.
	 * @param isFirstShape Whether this is the first shape (affects shape ID assignment).
	 */
	void processSvgShape(StoredCharacter& output, const svgtiny_shape& shape, const SDFGenerationArguments& args, bool isFirstShape = false);
	
	/**
	 * @brief Process multiple SVG shapes into a single glyph.
	 * @param output Output character structure to populate.
	 * @param shapes Span of SVG shapes to process.
	 * @param args Generation arguments.
	 */
	void processSvgShapes(StoredCharacter& output, const std::span<const svgtiny_shape>& shapes, const SDFGenerationArguments& args);
	
	/**
	 * @brief Convert FreeType bitmap to QImage.
	 * @param bitmap FreeType bitmap structure.
	 * @param intended_width Target width for the image.
	 * @param intended_height Target height for the image.
	 * @return QImage representation of the bitmap.
	 */
	static QImage FTBitmap2QImage(const FT_Bitmap_& bitmap, unsigned int intended_width, unsigned int intended_height);
	
	/**
	 * @brief Create a 1-bit padded variant of an image.
	 * @param glyph Source image.
	 * @param padding Padding size in pixels.
	 * @return QBitArray representing the padded 1-bit image.
	 */
	static QBitArray producePaddedVariant1bit(const QImage& glyph, unsigned int padding);
	
	/**
	 * @brief Create a padded variant of an image.
	 * @param glyph Source image.
	 * @param padding Padding size in pixels.
	 * @return Padded QImage.
	 */
	static QImage producePaddedVariantOfImage(const QImage& glyph, unsigned int padding);
	
	/**
	 * @brief Convert FreeType 26.6 fixed-point value to double.
	 * @param fixedPointValue 26.6 fixed-point integer.
	 * @return Floating-point value.
	 */
	static double convert26_6ToDouble(int32_t fixedPointValue);
	
	/**
	 * @brief Convert FreeType 16.16 fixed-point value to double.
	 * @param fixedPointValue 16.16 fixed-point integer.
	 * @return Floating-point value.
	 */
	static double convert16_16ToDouble(int32_t fixedPointValue);
	
	// FreeType outline decomposition callbacks
	/**
	 * @brief FreeType outline move-to callback.
	 * @param to Target point.
	 * @param user User data pointer (FontOutlineDecompositionContext*).
	 * @return 0 on success.
	 */
	static int Outline_MoveToFunc(const FT_Vector* to, void* user);
	
	/**
	 * @brief FreeType outline line-to callback.
	 * @param to Target point.
	 * @param user User data pointer (FontOutlineDecompositionContext*).
	 * @return 0 on success.
	 */
	static int Outline_LineToFunc(const FT_Vector* to, void* user );
	
	/**
	 * @brief FreeType outline conic (quadratic) curve-to callback.
	 * @param control Control point.
	 * @param to Target point.
	 * @param user User data pointer (FontOutlineDecompositionContext*).
	 * @return 0 on success.
	 */
	static int Outline_ConicToFunc(const FT_Vector* control, const FT_Vector*  to, void* user);
	
	/**
	 * @brief FreeType outline cubic curve-to callback.
	 * @param control1 First control point.
	 * @param control2 Second control point.
	 * @param to Target point.
	 * @param user User data pointer (FontOutlineDecompositionContext*).
	 * @return 0 on success.
	 */
	static int Outline_CubicToFunc(const FT_Vector* control1, const FT_Vector* control2, const FT_Vector* to, void* user);
	
	/**
	 * @brief Decompose an SVG shape into edge segments.
	 * @param decompositionContext Context to populate with edge segments.
	 * @param shape SVG shape to decompose.
	 * @param isFirstShape Whether this is the first shape.
	 */
	static void decomposeSvgShape(FontOutlineDecompositionContext& decompositionContext, const svgtiny_shape& shape, bool isFirstShape = false);
};

#endif // SDFGENERATIONCONTEXT_HPP
