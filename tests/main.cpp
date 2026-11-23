#include <catch2/catch_session.hpp>
#include <QResource>

int main(int argc, char* argv[])
{
	Q_INIT_RESOURCE(resources);
	return Catch::Session().run(argc, argv);
}
