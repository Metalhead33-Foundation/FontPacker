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
	QApplication a(argc, argv);
	setlocale(LC_ALL, "C");
	auto args = parseArguments(argc,argv);
	if(args.contains(QStringLiteral("nogui"))) {
		try {
			QTextStream strm(stdout);
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
