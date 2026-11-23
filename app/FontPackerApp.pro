# GUI and command-line front end.
QT += core gui widgets opengl openglwidgets core5compat
CONFIG += c++2a cmdline
TEMPLATE = app
TARGET = FontPacker

INCLUDEPATH += $$PWD $$PWD/../lib /usr/include/freetype2 /usr/include/harfbuzz
QMAKE_CXXFLAGS += $$system(pkg-config --cflags OpenCL) -I/usr/local/include -fopenmp
QMAKE_CFLAGS += -fopenmp
QMAKE_LFLAGS += -fopenmp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

# Static library produced by lib/FontPackerLib.pro. The extra -L paths cover
# both shadow and in-source builds.
LIBS += -L$$OUT_PWD/../lib -L$$OUT_PWD -L$$PWD/.. -lFontPackerCore
LIBS += -L/usr/local/lib64 -lspirv-cross-glsl -lspirv-cross-core -lglslang -lSPIRV -lglslang-default-resource-limits $$system(pkg-config --libs OpenCL) -lfreetype -lharfbuzz -lsvgtiny -fopenmp

SOURCES += \
    CQTOpenGLLuaSyntaxHighlighter.cpp \
    MainWindow.cpp \
    OpenGLCanvas.cpp \
    main.cpp

HEADERS += \
    CQTOpenGLLuaSyntaxHighlighter.hpp \
    MainWindow.hpp \
    OpenGLCanvas.hpp

FORMS += \
    MainWindow.ui

DISTFILES += \
    ../BINARY_FORMAT.md \
    ../LICENSE \
    ../README.md
