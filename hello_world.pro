#-------------------------------------------------
#
# Project created by QtCreator 2025-06-30T11:51:24
#
#-------------------------------------------------

QT       += core gui widgets


greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = hello_world
TEMPLATE = app


SOURCES += main.cpp\
        mainwindow.cpp\
        gamewindow.cpp\
        gameoverdialog.cpp\
        rankingdialog.cpp\
        playerdialog.cpp

HEADERS  += mainwindow.h\
        gameoverdialog.h\
        gamewindow.h\
        rankingdialog.h\
        playerdialog.h

FORMS    += mainwindow.ui

DISTFILES += \
#    main.qml
