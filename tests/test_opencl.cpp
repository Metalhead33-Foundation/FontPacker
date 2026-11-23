#include <catch2/catch_test_macros.hpp>

#include "SdfGenerationOpenCL.hpp"

#include <QImage>
#include <QString>

#include <exception>
#include <optional>

TEST_CASE("OpenCL bitmap backend produces a non-empty SDF", "[opencl]")
{
	SDFGenerationArguments args;
	args.mode = OPENCL;
	args.type = SDF;
	args.distType = Euclidean;
	args.internalProcessSize = 16;
	args.intendedSize = 0;
	args.padding = 2;
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

	QImage source(16, 16, QImage::Format_Grayscale8);
	source.fill(0);
	for(int y = 4; y < 12; ++y) {
		uchar* row = source.scanLine(y);
		for(int x = 4; x < 12; ++x) row[x] = 255;
	}

	QImage result;
	try {
		SdfGenerationOpenCL context(args);
		result = context.produceBitmapSdf(source, args);
	} catch(const std::exception& error) {
		const QString message = QString::fromUtf8(error.what());
		if(message.contains(QStringLiteral("No OpenCL"))) {
			SKIP("OpenCL runtime/device is not available: " << message.toStdString());
		}
		throw;
	}

	REQUIRE(result.size() == QSize(16, 16));
	CHECK(result.format() == QImage::Format_Grayscale8);
	CHECK(qGray(result.pixel(8, 8)) > 128);
	CHECK(qGray(result.pixel(0, 0)) < 128);
}
