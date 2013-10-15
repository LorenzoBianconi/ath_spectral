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


LIBS += -L$$PWD/../qwt/lib/ -lqwt
INCLUDEPATH += $$PWD/../qwt/src
DEPENDPATH += $$PWD/../qwt/src
