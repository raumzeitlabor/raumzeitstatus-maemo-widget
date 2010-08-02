TEMPLATE = app

QT += network

SOURCES += main.cpp rzlwidget.cpp
HEADERS += rzlwidget.h
CONFIG += link_pkgconfig
PKGCONFIG += glib-2.0 conic
TARGET = raumzeitlabor-status

include(../qmaemo5homescreenadaptor/qmaemo5homescreenadaptor.pri)

desktop.path = /usr/share/applications/hildon-home
desktop.files = rzl-status.desktop

data.path = /usr/share/raumzeitlabor-status-widget
data.files = unklar.png auf.png zu.png

target.path = /usr/lib/hildon-desktop
INSTALLS += target desktop data
