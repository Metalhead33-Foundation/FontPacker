QT += core gui

CONFIG += c++2a cmdline

INCLUDEPATH += /usr/include/freetype2
INCLUDEPATH += I/usr/include/harfbuzz
LIBS += -lfreetype -fopenmp

# You can make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
        ProcessFonts.cpp \
        main.cpp

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

DISTFILES += \
    .gitignore \
    LICENSE \
    README.md

HEADERS += \
    ProcessFonts.hpp
