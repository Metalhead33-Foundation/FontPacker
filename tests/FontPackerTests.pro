QT += core gui opengl
CONFIG += c++2a console testcase
CONFIG -= app_bundle
TEMPLATE = app
TARGET = FontPackerTests
DESTDIR = $$OUT_PWD

INCLUDEPATH += $$PWD/../lib $$PWD/../app /usr/include/freetype2 /usr/include/harfbuzz
QMAKE_CXXFLAGS += $$system(pkg-config --cflags catch2) $$system(pkg-config --cflags OpenCL) -I/usr/local/include -fopenmp
QMAKE_LFLAGS += -fopenmp

# Core library built by lib/FontPackerLib.pro. The extra -L paths cover both
# shadow and in-source builds.
LIBS += -L$$OUT_PWD/../lib -L$$OUT_PWD -L$$PWD/../lib -lFontPackerCore
LIBS += $$system(pkg-config --libs catch2)
LIBS += -L/usr/local/lib64 -lspirv-cross-glsl -lspirv-cross-core -lglslang -lSPIRV -lglslang-default-resource-limits $$system(pkg-config --libs OpenCL) -lfreetype -lharfbuzz -lsvgtiny -fopenmp

SOURCES += \
    main.cpp \
    test_containers.cpp \
    test_opencl.cpp \
    test_gl_shaders.cpp \
    test_hlsl.cpp \
    test_software.cpp \
    test_highlighter.cpp \
    ../app/CQTOpenGLShaderSyntaxHighlighter.cpp

HEADERS += \
    test_helpers.hpp \
    ../app/CQTOpenGLShaderSyntaxHighlighter.hpp
