#include <QApplication>
#include <QVariant>
#include <QTextStream>
#include <QFile>
#include "ConstStrings.hpp"
#include "ProcessFonts.hpp"

QVariantMap parseArguments(int argc, char *argv[]);
int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	auto args = parseArguments(argc,argv);
	if(args.contains(QStringLiteral("nogui"))) {
		QTextStream strm(stdout);
		for(auto it = std::begin(args); it != std::end(args); ++it) {
			strm << it.key() << ' ' << it.value().toString() << '\n';
		}
		strm.flush();
		PreprocessedFontFace fontface;
		if( args.contains( IN_FONT_KEY ) ) {
			fontface.processFonts(args);
		}
		else if( args.contains( IN_BIN_KEY ) ) {
			QFile fil(args.value(IN_BIN_KEY).toString());
			if(fil.open(QFile::ReadOnly)) {
				QDataStream binF(&fil);
				binF.setVersion(QDataStream::Qt_4_0);
				binF.setByteOrder(QDataStream::BigEndian);
				fontface.fromData(binF);
			}
		}
		else if( args.contains( IN_CBOR_KEY ) ) {
			QFile fil(args.value(IN_CBOR_KEY).toString());
			if(fil.open(QFile::ReadOnly)) {
				QCborValue cborv = QCborValue::fromCbor(fil.readAll());
				fil.close();
				fontface.fromCbor(cborv.toMap());
			}
		}

		if(args.contains( OUT_FONT_KEY )) {
			fontface.outToFolder( args.value(OUT_FONT_KEY).toString() );
		}
		if(args.contains( OUT_BIN_KEY )) {
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
			QFile fil(args.value(OUT_CBOR_KEY).toString());
			if(fil.open(QFile::WriteOnly)) {
				QCborValue cbor = fontface.toCbor();
				fil.write(cbor.toCbor());
				fil.flush();
				fil.close();
			}
		}
		return 0;
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
