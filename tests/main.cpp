#include <catch2/catch_session.hpp>
#include <QResource>
#include <QGuiApplication>
#include <QByteArray>

int main(int argc, char* argv[])
{
	if(qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY")) {
		qputenv("QT_QPA_PLATFORM", QByteArrayLiteral("offscreen"));
	}
	Q_INIT_RESOURCE(resources);
	QGuiApplication app(argc, argv);
	return Catch::Session().run(argc, argv);
}
