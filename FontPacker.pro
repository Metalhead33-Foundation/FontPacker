QT += core gui opengl widgets openglwidgets core5compat

CONFIG += c++2a cmdline

INCLUDEPATH += /usr/include/freetype2
INCLUDEPATH += I/usr/include/harfbuzz
QMAKE_CXXFLAGS += -fopenmp
QMAKE_CFLAGS += -fopenmp
QMAKE_LFLAGS += -fopenmp
LIBS += -lfreetype -lharfbuzz -lsvgtiny -fopenmp

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        CQTOpenGLLuaSyntaxHighlighter.cpp \
        ConstStrings.cpp \
        FontOutlineDecompositionContext.cpp \
        GlHelpers.cpp \
        HugePreallocator.cpp \
        MainWindow.cpp \
        OpenGLCanvas.cpp \
        PreprocessedFontFace.cpp \
        SDFGenerationArguments.cpp \
        SdfGenerationContext.cpp \
        SdfGenerationContextSoft.cpp \
        SdfGenerationGL.cpp \
        StoredCharacter.cpp \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    .gitignore \
    LICENSE \
    README.md \
    msdf_fixer.glsl \
    screen.vert.glsl \
    shader1.glsl \
    shader3.glsl \
    shader3_msdf.glsl \
    shader_msdf1.glsl

HEADERS += \
    CQTOpenGLLuaSyntaxHighlighter.hpp \
    ConstStrings.hpp \
    FontOutlineDecompositionContext.hpp \
    GlHelpers.hpp \
    HugePreallocator.hpp \
    MainWindow.hpp \
    Mallocator.hpp \
    OpenGLCanvas.hpp \
    PreprocessedFontFace.hpp \
    RGBA8888.hpp \
    SDFGenerationArguments.hpp \
    SdfGenerationContext.hpp \
    SdfGenerationContextSoft.hpp \
    SdfGenerationGL.hpp \
    StoredCharacter.hpp

RESOURCES += \
    resources.qrc

FORMS += \
    MainWindow.ui
