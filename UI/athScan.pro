#-------------------------------------------------
#
# Project created by QtCreator 2013-09-21T18:33:53
#
#-------------------------------------------------

QT       += core gui

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = athScan
TEMPLATE = app


SOURCES += main.cpp\
        athscan.cpp

HEADERS  += athscan.h

FORMS    += athscan.ui

INCLUDEPATH += /usr/local/qwt-6.1.0/include
LIBS += /usr/local/qwt-6.1.0/lib/libqwt.so.6
