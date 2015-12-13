TEMPLATE = app
CONFIG -= console app_bundle qt
CONFIG += link_pkgconfig

PKGCONFIG += sdl2
LIBS += -lSDL2_image -lSDL2_ttf

RC_FILE = resource.rc

SOURCES += main.cpp
