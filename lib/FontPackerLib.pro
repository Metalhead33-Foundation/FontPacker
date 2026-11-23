# Core FontPacker library: everything except the GUI and the command line.
QT += core gui opengl
CONFIG += c++2a staticlib
TEMPLATE = lib
TARGET = FontPackerCore
DESTDIR = $$OUT_PWD

INCLUDEPATH += $$PWD /usr/include/freetype2 /usr/include/harfbuzz
QMAKE_CXXFLAGS += $$system(pkg-config --cflags OpenCL) -fopenmp
QMAKE_CFLAGS += -fopenmp
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000

SOURCES += \
    ConstStrings.cpp \
    FontOutlineDecompositionContext.cpp \
    GlHelpers.cpp \
    HugePreallocator.cpp \
    PreprocessedFontFace.cpp \
    QDataStreamHelper.cpp \
    SDFGenerationArguments.cpp \
    SdfGenerationContext.cpp \
    SdfGenerationContextSoft.cpp \
    SdfGenerationGL.cpp \
    SdfGenerationOpenCL.cpp \
    StoredCharacter.cpp \
    StoredVectorImage.cpp

HEADERS += \
    ConstStrings.hpp \
    FontOutlineDecompositionContext.hpp \
    GlHelpers.hpp \
    HugePreallocator.hpp \
    Mallocator.hpp \
    PreprocessedFontFace.hpp \
    QDataStreamHelper.hpp \
    RGBA8888.hpp \
    SDFGenerationArguments.hpp \
    SdfGenerationContext.hpp \
    SdfGenerationContextSoft.hpp \
    SdfGenerationGL.hpp \
    SdfGenerationOpenCL.hpp \
    StoredCharacter.hpp \
    StoredVectorImage.hpp

# The OpenGL backend loads its compute shaders from these resources. They are
# compiled into the library; executables should call Q_INIT_RESOURCE(resources).
RESOURCES += resources.qrc
