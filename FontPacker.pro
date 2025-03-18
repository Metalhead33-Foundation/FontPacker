QT += core gui opengl widgets openglwidgets

CONFIG += c++2a cmdline

INCLUDEPATH += /usr/include/freetype2
INCLUDEPATH += I/usr/include/harfbuzz
QMAKE_CXXFLAGS += -fopenmp
QMAKE_CFLAGS += -fopenmp
QMAKE_LFLAGS += -fopenmp
LIBS += -lfreetype -lharfbuzz -fopenmp

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        ConstStrings.cpp \
        GlHelpers.cpp \
        MainWindow.cpp \
        OpenGLCanvas.cpp \
        ProcessFonts.cpp \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    .gitignore \
    LICENSE \
    README.md \
    screen.vert.glsl \
    shader1.glsl \
    shader2.glsl

HEADERS += \
    ConstStrings.hpp \
    GlHelpers.hpp \
    MainWindow.hpp \
    OpenGLCanvas.hpp \
    ProcessFonts.hpp

RESOURCES += \
    resources.qrc

FORMS += \
    MainWindow.ui
