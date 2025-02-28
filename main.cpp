#include <QCoreApplication>
#include <QVariant>
#include <QTextStream>
#include "ProcessFonts.hpp"

QVariantMap parseArguments(int argc, char *argv[]);
int main(int argc, char *argv[])
{
	auto args = parseArguments(argc,argv);
	if(args.contains(QStringLiteral("nogui"))) {
		processFonts(args);
		return 0;
	} else {
		QCoreApplication a(argc, argv);

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
