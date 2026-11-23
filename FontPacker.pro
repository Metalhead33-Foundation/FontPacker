TEMPLATE = subdirs
SUBDIRS += lib app tests

lib.file = lib/FontPackerLib.pro
app.file = app/FontPackerApp.pro
app.depends = lib

tests.file = tests/FontPackerTests.pro
tests.depends = lib
