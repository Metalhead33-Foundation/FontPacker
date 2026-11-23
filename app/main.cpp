/**
 * @file main.cpp
 * @brief Main entry point for FontPacker application.
 * 
 * Provides both GUI and command-line interfaces for generating signed distance fields
 * from fonts and SVG files. Supports multiple output formats (binary, CBOR) and
 * multiple rendering modes (software CPU, OpenGL compute shaders).
 * 
 * Command-line usage:
 * - Use "nogui" argument to run in command-line mode
 * - Use --inspect <path> to print a parsed container summary
 * - Input: --infont <path>, --insvg <path>, --inbin <path>, --incbor <path>,
 *          --invectorbin <path>, or --invectorcbor <path>
 * - Output: --outbin <path>, --outcbor <path>, --outfont <pattern>,
 *           --outvectorbin <path>, or --outvectorcbor <path>
 * - See SDFGenerationArguments for all available options
 */

#include <QApplication>
#include <QVariant>
#include <QTextStream>
#include <QFile>
#include <QStringList>
#include <QDataStream>
#include <QBuffer>
#include <QImage>
#include <QCborValue>
#include <stdexcept>
#include "ConstStrings.hpp"
#include "SdfGenerationContext.hpp"
#include "SdfGenerationGL.hpp"
#include "SdfGenerationContextSoft.hpp"
#include "MainWindow.hpp"

namespace {

/**
 * @brief Options that only make sense while generating from a font or SVG.
 *
 * They cannot be applied to an already stored `.wodf`/`.wodi` because the
 * distance values and outlines are not present. Passing one of them with a
 * stored input is an error instead of being silently ignored.
 */
const QStringList& generationOnlyKeys()
{
	static const QStringList keys = {
		MODE_KEY, TYPE_KEY, DIST_KEY, INTERNAL_PROCESS_SIZE_KEY, PADDING_KEY,
		SAMPLES_TO_CHECK_X_KEY, SAMPLES_TO_CHECK_Y_KEY, CHAR_MIN_KEY, CHAR_MAX_KEY,
		FORCE_RASTER_KEY, INVERT_KEY, GAMMA_CORRECT_KEY, MIDPOINT_ADJUSTMENT_KEY,
		MAXIMIZE_INSTEAD_OF_AVERAGE_KEY, MSDFGEN_COLOURING
	};
	return keys;
}

QByteArray requestedImageFormat(const QVariantMap& args)
{
	if(!args.contains(IMAGE_FORMAT_KEY)) return {};
	return args.value(IMAGE_FORMAT_KEY).toString().trimmed().toUpper().toLatin1();
}

uint32_t requestedIntendedSize(const QVariantMap& args)
{
	if(!args.contains(INTENDED_SIZE_KEY)) return 0;
	return args.value(INTENDED_SIZE_KEY).toUInt();
}

void validateLoadedInputOptions(const QVariantMap& args, bool isVectorImage)
{
	for(const QString& key : generationOnlyKeys()) {
		if(args.contains(key)) {
			throw std::runtime_error(QStringLiteral("Option --%1 requires --infont or --insvg; it cannot be applied to a stored .wodf/.wodi file.")
									 .arg(key).toStdString());
		}
	}
	if(args.contains(CREATE_MIPMAPS_KEY) && !isVectorImage) {
		throw std::runtime_error("Option --createmipmaps is only meaningful for --invectorbin/--invectorcbor input.");
	}
}

QStringList presentInputKeys(const QVariantMap& args)
{
	QStringList present;
	for(const QString& key : {IN_FONT_KEY, IN_SVG_KEY, IN_BIN_KEY, IN_CBOR_KEY, IN_VECTOR_BIN_KEY, IN_VECTOR_CBOR_KEY}) {
		if(args.contains(key)) present.push_back(key);
	}
	return present;
}
QString formatFloat(double value, int precision = 4)
{
	return QString::number(value, 'f', precision);
}

QString sdfTypeName(SDFType type)
{
	switch(type) {
		case SDF: return QStringLiteral("SDF");
		case MSDF: return QStringLiteral("MSDF");
		case MSDFA: return QStringLiteral("MSDFA");
	}
	return QStringLiteral("?");
}

QString distanceTypeName(DistanceType type)
{
	switch(type) {
		case Manhattan: return QStringLiteral("Manhattan");
		case Euclidean: return QStringLiteral("Euclidean");
	}
	return QStringLiteral("?");
}

QString codePointLabel(uint32_t codePoint)
{
	return QStringLiteral("U+%1").arg(codePoint, 4, 16, QChar('0')).toUpper();
}

void inspectFontFace(const QByteArray& bytes, QTextStream& out)
{
	QByteArray copy = bytes;
	QBuffer buffer(&copy);
	buffer.open(QIODevice::ReadOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);

	PreprocessedFontFace face;
	face.fromData(stream);

	out << "WODF version=" << face.version
		<< " family=\"" << face.fontFamilyName << "\""
		<< " type=" << sdfTypeName(face.type)
		<< " dist=" << distanceTypeName(face.distType)
		<< " S=" << face.bitmap_size
		<< " L=" << face.bitmap_logical_size
		<< " P=" << face.bitmap_padding
		<< " format=" << QString::fromLatin1(face.imageFormatBytes())
		<< " hasVert=" << (face.hasVert ? 1 : 0) << '\n';
	out << "face ascender=" << formatFloat(face.ascender)
		<< " descender=" << formatFloat(face.descender)
		<< " faceHeight=" << formatFloat(face.faceHeight)
		<< " maxAdvance=" << formatFloat(face.maxAdvance)
		<< " unitsPerEm=" << face.unitsPerEm << '\n';
	out << "distanceRange=(" << formatFloat(face.distanceRangeX) << ", " << formatFloat(face.distanceRangeY) << ")"
		<< " encodingFlags=0x" << QString::number(face.encodingFlags, 16).rightJustified(8, QChar('0'))
		<< " midpointAdjustment=" << formatFloat(face.midpointAdjustment) << '\n';
	out << "glyphs=" << face.storedCharacters.size() << '\n';
	for(auto it = std::begin(face.storedCharacters); it != std::end(face.storedCharacters); ++it) {
		const StoredCharacter& glyph = it.value();
		out << codePointLabel(it.key()) << ' ';
		if(!glyph.valid) {
			out << "invalid\n";
			continue;
		}
		out << "width=" << glyph.width
			<< " height=" << glyph.height
			<< " bearing=(" << glyph.bearing_x << ", " << glyph.bearing_y << ")"
			<< " advance=(" << formatFloat(glyph.advance_x) << ", " << formatFloat(glyph.advance_y) << ")"
			<< " metric=" << formatFloat(glyph.metricWidth) << 'x' << formatFloat(glyph.metricHeight)
			<< " @" << formatFloat(glyph.horiBearingX) << ',' << formatFloat(glyph.horiBearingY)
			<< " horiAdvance=" << formatFloat(glyph.horiAdvance)
			<< " sdf=" << glyph.sdf.size() << "B\n";
	}
}

void inspectFontFaceCbor(const QCborMap& map, QTextStream& out)
{
	PreprocessedFontFace face;
	face.fromCbor(map);
	QByteArray bytes;
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::WriteOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);
	face.toData(stream);
	buffer.close();
	inspectFontFace(bytes, out);
}

void inspectVectorImage(const QByteArray& bytes, QTextStream& out)
{
	QByteArray copy = bytes;
	QBuffer buffer(&copy);
	buffer.open(QIODevice::ReadOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);

	StoredVectorImage image;
	image.fromData(stream);

	out << "WODI version=" << image.version
		<< " L=" << image.processingSize
		<< " S=" << image.actualSize
		<< " P=" << image.padding
		<< " format=" << QString::fromLatin1(image.imageFormatBytes())
		<< " type=" << sdfTypeName(image.type)
		<< " dist=" << distanceTypeName(image.distType) << '\n';
	out << "logical=(" << formatFloat(image.logicalX) << ", " << formatFloat(image.logicalY)
		<< ", " << formatFloat(image.logicalWidth) << ", " << formatFloat(image.logicalHeight) << ")"
		<< " aspect=" << formatFloat(image.aspectRatio, 6) << '\n';
	out << "bounds=(" << formatFloat(image.minX) << ", " << formatFloat(image.maxX)
		<< ", " << formatFloat(image.minY) << ", " << formatFloat(image.maxY) << ")" << '\n';
	out << "distanceRange=(" << formatFloat(image.distanceRangeX) << ", " << formatFloat(image.distanceRangeY) << ")"
		<< " encodingFlags=0x" << QString::number(image.encodingFlags, 16).rightJustified(8, QChar('0'))
		<< " midpointAdjustment=" << formatFloat(image.midpointAdjustment) << '\n';
	out << "mipmaps=" << image.mipmaps.size() << '\n';
	for(qsizetype i = 0; i < image.mipmaps.size(); ++i) {
		const QImage level = QImage::fromData(image.mipmaps.at(i));
		out << '[' << i << "] " << image.mipmaps.at(i).size() << 'B';
		if(!level.isNull()) out << ' ' << level.width() << 'x' << level.height();
		out << '\n';
	}
}

void inspectVectorImageCbor(const QCborMap& map, QTextStream& out)
{
	StoredVectorImage image;
	image.fromCbor(map);
	QByteArray bytes;
	QBuffer buffer(&bytes);
	buffer.open(QIODevice::WriteOnly);
	QDataStream stream(&buffer);
	stream.setVersion(QDataStream::Qt_4_0);
	stream.setByteOrder(QDataStream::BigEndian);
	image.toData(stream);
	buffer.close();
	inspectVectorImage(bytes, out);
}

void inspectPath(const QString& path, QTextStream& out)
{
	QFile file(path);
	if(!file.open(QFile::ReadOnly)) {
		throw std::runtime_error(QStringLiteral("Failed to open %1 for reading.").arg(path).toStdString());
	}
	const QByteArray bytes = file.readAll();
	file.close();
	if(bytes.size() < 4) {
		throw std::runtime_error("File is too short to contain a container header.");
	}

	const QByteArray magic = bytes.left(4);
	if(magic == QByteArray(PreprocessedFontFace::BINARY_MAGIC.data(), PreprocessedFontFace::BINARY_MAGIC.size())) {
		inspectFontFace(bytes, out);
		return;
	}
	if(magic == QByteArray(StoredVectorImage::BINARY_MAGIC.data(), StoredVectorImage::BINARY_MAGIC.size())) {
		inspectVectorImage(bytes, out);
		return;
	}

	QCborParserError error{};
	const QCborValue value = QCborValue::fromCbor(bytes, &error);
	if(error.error != QCborError::NoError || !value.isMap()) {
		throw std::runtime_error("Unsupported container: not WODF, WODI or CBOR.");
	}
	const QCborMap map = value.toMap();
	const QString containerType = map.value(CBOR_CONTAINER_TYPE_KEY).toString();
	if(containerType == CBOR_STORED_VECTOR_IMAGE_TYPE) {
		inspectVectorImageCbor(map, out);
		return;
	}
	if(containerType.isEmpty() || containerType == CBOR_PREPROCESSED_FONT_FACE_TYPE) {
		inspectFontFaceCbor(map, out);
		return;
	}
	throw std::runtime_error(QStringLiteral("Unsupported CBOR container type: %1").arg(containerType).toStdString());
}


}

/**
 * @brief Parse command-line arguments into a QVariantMap.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Map of argument keys to values.
 */
QVariantMap parseArguments(int argc, char *argv[]);

/**
 * @brief Main entry point.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return Exit code (0 on success).
 */
int main(int argc, char *argv[])
{
	Q_INIT_RESOURCE(resources);
	QApplication a(argc, argv);
	setlocale(LC_ALL, "C");
	auto args = parseArguments(argc,argv);
	const bool commandLineMode = args.contains(QStringLiteral("nogui")) || args.contains(INSPECT_KEY);
	if(commandLineMode) {
		try {
			QTextStream strm(stdout);
			if(args.contains(INSPECT_KEY)) {
				for(auto it = std::begin(args); it != std::end(args); ++it) {
					if(it.key() != INSPECT_KEY && it.key() != QStringLiteral("nogui")) {
						throw std::runtime_error(QStringLiteral("Option --%1 cannot be combined with --inspect.").arg(it.key()).toStdString());
					}
				}
				const QVariant pathValue = args.value(INSPECT_KEY);
				if(pathValue.typeId() != QMetaType::QString || pathValue.toString().trimmed().isEmpty()) {
					throw std::runtime_error("--inspect requires a container path.");
				}
				inspectPath(pathValue.toString(), strm);
				strm.flush();
				return 0;
			}
			for(auto it = std::begin(args); it != std::end(args); ++it) {
				strm << it.key() << ' ' << it.value().toString() << '\n';
			}
			strm.flush();
			const QStringList inputs = presentInputKeys(args);
			if(inputs.size() > 1) {
				throw std::runtime_error(QStringLiteral("Exactly one input option is supported, but %1 were supplied.")
										 .arg(inputs.join(QStringLiteral(", "))).toStdString());
			}
			const bool loadedInput = args.contains(IN_BIN_KEY) || args.contains(IN_CBOR_KEY)
									 || args.contains(IN_VECTOR_BIN_KEY) || args.contains(IN_VECTOR_CBOR_KEY);
			const bool loadedVectorInput = args.contains(IN_VECTOR_BIN_KEY) || args.contains(IN_VECTOR_CBOR_KEY);
			if(loadedInput) validateLoadedInputOptions(args, loadedVectorInput);

			PreprocessedFontFace fontface;
			StoredVectorImage vectorImage;
			bool hasFontFace = false;
			bool hasVectorImage = false;
			if( args.contains( IN_FONT_KEY ) ) {
				SDFGenerationArguments sdfArgs;
				sdfArgs.fromArgs(args);
				std::unique_ptr<SdfGenerationContext> ctx;
				switch (sdfArgs.mode) {
					case SOFTWARE: ctx = std::make_unique<SdfGenerationContextSoft>(); break;
					case OPENGL_COMPUTE: ctx = std::make_unique<SdfGenerationGL>(sdfArgs); break;
					case OPENCL: throw std::runtime_error("Unsupported mode!");
						break;
				}
				ctx->processFont(fontface,sdfArgs);
				hasFontFace = true;
			}
			if( args.contains( IN_SVG_KEY ) ) {
				auto svgpath = args[IN_SVG_KEY].toString();
				SDFGenerationArguments sdfArgs;
				sdfArgs.fromArgs(args);
				std::unique_ptr<SdfGenerationContext> ctx;
				switch (sdfArgs.mode) {
					case SOFTWARE: ctx = std::make_unique<SdfGenerationContextSoft>(); break;
					case OPENGL_COMPUTE: ctx = std::make_unique<SdfGenerationGL>(sdfArgs); break;
					case OPENCL: throw std::runtime_error("Unsupported mode!");
						break;
				}
				QFile svgFile(svgpath);
				if( svgFile.open(QFile::ReadOnly) ) {
					const QByteArray svgData = svgFile.readAll();
					const bool wantsVectorImage = args.contains(OUT_VECTOR_BIN_KEY) || args.contains(OUT_VECTOR_CBOR_KEY);
					const bool wantsFontFace = args.contains(OUT_BIN_KEY) || args.contains(OUT_CBOR_KEY) || args.contains(OUT_FONT_KEY) || !wantsVectorImage;
					if(wantsFontFace) {
						ctx->processSvg(fontface, svgData, sdfArgs);
						hasFontFace = true;
					}
					if(wantsVectorImage) {
						ctx->processSvg(vectorImage, svgData, sdfArgs);
						hasVectorImage = true;
					}
				}
			}
			else if( args.contains( IN_VECTOR_BIN_KEY ) ) {
				QFile fil(args.value(IN_VECTOR_BIN_KEY).toString());
				if(fil.open(QFile::ReadOnly)) {
					QDataStream binF(&fil);
					binF.setVersion(QDataStream::Qt_4_0);
					binF.setByteOrder(QDataStream::BigEndian);
					vectorImage.fromData(binF);
					hasVectorImage = true;
				}
			}
			else if( args.contains( IN_VECTOR_CBOR_KEY ) ) {
				QFile fil(args.value(IN_VECTOR_CBOR_KEY).toString());
				if(fil.open(QFile::ReadOnly)) {
					QCborValue cborv = QCborValue::fromCbor(fil.readAll());
					fil.close();
					vectorImage.fromCbor(cborv.toMap());
					hasVectorImage = true;
				}
			}
			else if( args.contains( IN_BIN_KEY ) ) {
				QFile fil(args.value(IN_BIN_KEY).toString());
				if(fil.open(QFile::ReadOnly)) {
					QDataStream binF(&fil);
					binF.setVersion(QDataStream::Qt_4_0);
					binF.setByteOrder(QDataStream::BigEndian);
					fontface.fromData(binF);
					hasFontFace = true;
				}
			}
			else if( args.contains( IN_CBOR_KEY ) ) {
				QFile fil(args.value(IN_CBOR_KEY).toString());
				if(fil.open(QFile::ReadOnly)) {
					QCborValue cborv = QCborValue::fromCbor(fil.readAll());
					fil.close();
					fontface.fromCbor(cborv.toMap());
					hasFontFace = true;
				}
			}

			if(loadedInput && (args.contains(IMAGE_FORMAT_KEY) || args.contains(INTENDED_SIZE_KEY)
						   || (loadedVectorInput && args.contains(CREATE_MIPMAPS_KEY)))) {
				const QByteArray repackFormat = requestedImageFormat(args);
				const uint32_t repackSize = requestedIntendedSize(args);
				if(hasFontFace) {
					fontface.repack(repackFormat, repackSize);
				}
				if(hasVectorImage) {
					vectorImage.repack(repackFormat, repackSize, args.contains(CREATE_MIPMAPS_KEY));
				}
			}

			if(args.contains( OUT_FONT_KEY )) {
				if(!hasFontFace) throw std::runtime_error("No preprocessed font face was loaded or generated.");
				fontface.outToFolder( args.value(OUT_FONT_KEY).toString() );
			}
			if(args.contains( OUT_BIN_KEY )) {
				if(!hasFontFace) throw std::runtime_error("No preprocessed font face was loaded or generated.");
				QFile fil(args.value(OUT_BIN_KEY).toString());
				if(fil.open(QFile::WriteOnly)) {
					QDataStream binF(&fil);
					binF.setVersion(QDataStream::Qt_4_0);
					binF.setByteOrder(QDataStream::BigEndian);
					fontface.toData(binF);
					fil.flush();
					fil.close();
				}
			}
			if(args.contains( OUT_CBOR_KEY )) {
				if(!hasFontFace) throw std::runtime_error("No preprocessed font face was loaded or generated.");
				QFile fil(args.value(OUT_CBOR_KEY).toString());
				if(fil.open(QFile::WriteOnly)) {
					QCborValue cbor = fontface.toCbor();
					fil.write(cbor.toCbor());
					fil.flush();
					fil.close();
				}
			}
			if(args.contains( OUT_VECTOR_BIN_KEY )) {
				if(!hasVectorImage) throw std::runtime_error("No stored vector image was loaded or generated.");
				QFile fil(args.value(OUT_VECTOR_BIN_KEY).toString());
				if(fil.open(QFile::WriteOnly)) {
					QDataStream binF(&fil);
					binF.setVersion(QDataStream::Qt_4_0);
					binF.setByteOrder(QDataStream::BigEndian);
					vectorImage.toData(binF);
					fil.flush();
					fil.close();
				}
			}
			if(args.contains( OUT_VECTOR_CBOR_KEY )) {
				if(!hasVectorImage) throw std::runtime_error("No stored vector image was loaded or generated.");
				QFile fil(args.value(OUT_VECTOR_CBOR_KEY).toString());
				if(fil.open(QFile::WriteOnly)) {
					QCborValue cbor = vectorImage.toCbor();
					fil.write(cbor.toCbor());
					fil.flush();
					fil.close();
				}
			}
			return 0;
		} catch(const std::exception& exp) {
			QTextStream err(stderr);
			err << "Error: " << exp.what() << '\n';
			err.flush();
			return 1;
		}
	} else {

		// Set up code that uses the Qt event loop here.
		// Call a.quit() or a.exit() to quit the application.
		// A not very useful example would be including
		// #include <QTimer>
		// near the top of the file and calling
		// QTimer::singleShot(5000, &a, &QCoreApplication::quit);
		// which quits the application after 5 seconds.

		// If you do not need a running Qt event loop, remove the call
		// to a.exec() or use the Non-Qt Plain C++ Application template.

		QSurfaceFormat fmt;
		fmt.setDepthBufferSize(24);
		if (QOpenGLContext::openGLModuleType() == QOpenGLContext::LibGL) {
			qDebug("Requesting OpenGL 4.3 core context");
			fmt.setVersion(4, 3);
			fmt.setProfile(QSurfaceFormat::CoreProfile);
		} else {
			qDebug("Requesting OpenGL ES 3.2 context");
			fmt.setVersion(3, 2);
		}
		QSurfaceFormat::setDefaultFormat(fmt);
		MainWindow* mainWin = new MainWindow();
		mainWin->show();
		return a.exec();
	}
}

QVariantMap parseArguments(int argc, char *argv[]) {
	QStringList argsRaw;
	QVariantMap parsedArgs;
	for(int i = 1; i < argc; ++i) {
		argsRaw.push_back(QString::fromLocal8Bit(argv[i]));
	}
	QVariantMap::iterator lastInserted = parsedArgs.end();
	for(const auto& it : argsRaw) {
		if(it.startsWith(QStringLiteral("--"))) {
			QString lastDecl = it;
			lastDecl = lastDecl.toLower().mid(2,-1);
			lastInserted = parsedArgs.insert(lastDecl,true);
		} else if(lastInserted != std::end(parsedArgs)) {
			lastInserted.value() = it;
			/*if(lastInserted.value().typeId() == QMetaType::QStringList) {
				QStringList stringList = lastInserted.value().value<QStringList>();
				stringList.append(it);
				lastInserted.value().setValue(stringList);
			} else {
				lastInserted.value() = QStringList{ it };
			}*/
		}
	}
	return parsedArgs;
}
