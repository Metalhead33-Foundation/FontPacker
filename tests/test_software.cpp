#include <catch2/catch_test_macros.hpp>

#include "SdfGenerationContextSoft.hpp"

#include <QImage>

#include <cstdint>
#include <optional>
#include <span>

namespace {

SDFGenerationArguments makeArgs(SDFType type, DistanceType distance = Euclidean)
{
	SDFGenerationArguments args;
	args.mode = SOFTWARE;
	args.type = type;
	args.distType = distance;
	args.internalProcessSize = 32;
	args.intendedSize = 0;
	args.padding = 4;
	args.samples_to_check_x = 0;
	args.samples_to_check_y = 0;
	args.char_min = 0;
	args.char_max = 1;
	args.msdfgenColouring = false;
	args.invert = false;
	args.imageFormat = QByteArrayLiteral("PNG");
	args.forceRaster = false;
	args.gammaCorrect = false;
	args.maximizeInsteadOfAverage = false;
	args.midpointAdjustment = std::nullopt;
	args.createMipmaps = false;
	return args;
}

void addLine(FontOutlineDecompositionContext& context, float x1, float y1, float x2, float y2)
{
	EdgeSegment edge{};
	edge.type = EdgeType::LINEAR;
	edge.contourId = 0;
	edge.clr = 0xFFFFFF00u;
	edge.points[0] = glm::fvec2(x1, y1);
	edge.points[1] = glm::fvec2(x2, y2);
	context.edges.push_back(edge);
}

FontOutlineDecompositionContext rectangleOutline()
{
	FontOutlineDecompositionContext context;
	addLine(context, 8.0f, 8.0f, 24.0f, 8.0f);
	addLine(context, 24.0f, 8.0f, 24.0f, 24.0f);
	addLine(context, 24.0f, 24.0f, 8.0f, 24.0f);
	addLine(context, 8.0f, 24.0f, 8.0f, 8.0f);
	return context;
}

/// Two single-edge contours, the second one ending on the very last edge of the outline.
/// Such a contour used to map to an empty span because the upper bound was clamped to
/// `edges.size() - 1` instead of `edges.size()`.
FontOutlineDecompositionContext trailingSingleEdgeContours()
{
	FontOutlineDecompositionContext context;
	addLine(context, 8.0f, 8.0f, 24.0f, 8.0f);
	context.edges.back().contourId = 0;
	addLine(context, 8.0f, 24.0f, 24.0f, 24.0f);
	context.edges.back().contourId = 1;
	return context;
}

}

TEST_CASE("Software outline SDF generates inside and outside values", "[software]")
{
	SdfGenerationContextSoft context;
	const QImage image = context.produceOutlineSdf(rectangleOutline(), makeArgs(SDF));
	REQUIRE(image.size() == QSize(32, 32));
	CHECK(image.format() == QImage::Format_Grayscale8);
	CHECK(qGray(image.pixel(16, 16)) > 128);
	CHECK(qGray(image.pixel(2, 2)) < 128);
}

TEST_CASE("Software outline MSDF and MSDFA generate RGBA images", "[software]")
{
	SdfGenerationContextSoft context;
	const QImage msdf = context.produceOutlineSdf(rectangleOutline(), makeArgs(MSDF));
	const QImage msdfa = context.produceOutlineSdf(rectangleOutline(), makeArgs(MSDFA));
	REQUIRE(msdf.size() == QSize(32, 32));
	REQUIRE(msdfa.size() == QSize(32, 32));
	CHECK(msdf.format() == QImage::Format_RGBA8888);
	CHECK(msdfa.format() == QImage::Format_RGBA8888);
	CHECK(msdfa.pixelColor(16, 16).alpha() > 128);
}

TEST_CASE("Software bitmap SDF honours value transforms", "[software]")
{
	QImage source(32, 32, QImage::Format_Grayscale8);
	source.fill(0);
	for(int y = 0; y < 32; ++y) {
		uchar* row = source.scanLine(y);
		for(int x = 0; x < 32; ++x) if(x + y >= 24) row[x] = 255;
	}

	SdfGenerationContextSoft context;
	const SDFGenerationArguments normalArgs = makeArgs(SDF);
	SDFGenerationArguments invertedArgs = normalArgs;
	invertedArgs.invert = true;
	const QImage normal = context.produceBitmapSdf(source, normalArgs);
	const QImage inverted = context.produceBitmapSdf(source, invertedArgs);
	REQUIRE(normal.size() == inverted.size());
	CHECK(normal != inverted);
}

TEST_CASE("Contour spans keep the last edge of the outline", "[outline]")
{
	const FontOutlineDecompositionContext context = trailingSingleEdgeContours();
	REQUIRE(context.edges.size() == 2);

	const std::span<const EdgeSegment> first = context.getEdgeSegmentsForContour({0, 0});
	const std::span<const EdgeSegment> last = context.getEdgeSegmentsForContour({1, 1});
	REQUIRE(first.size() == 1);
	REQUIRE(last.size() == 1);
	CHECK(&last.front() == &context.edges.back());
}

TEST_CASE("Contour spans are empty for degenerate definitions", "[outline]")
{
	const FontOutlineDecompositionContext context = trailingSingleEdgeContours();
	CHECK(context.getEdgeSegmentsForContour({1, 0}).empty());
	CHECK(context.getEdgeSegmentsForContour({2, 5}).empty());

	FontOutlineDecompositionContext empty;
	CHECK(empty.getEdgeSegmentsForContour({0, 0}).empty());
}

TEST_CASE("MSDF edge colouring handles a contour ending on the last edge", "[outline]")
{
	FontOutlineDecompositionContext context = trailingSingleEdgeContours();
	REQUIRE_NOTHROW(context.assignColoursMsdfgen());
	for(const EdgeSegment& edge : context.edges)
		CHECK(edge.clr != 0u); // Palette colours are never BLACK here.
}
