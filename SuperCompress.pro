T       += core gui
greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

TARGET = SuperCompress
TEMPLATE = app

CONFIG += c++11 openmp

QMAKE_CXXFLAGS += -fopenmp
QMAKE_LFLAGS += -fopenmp

win32-msvc* {
    QMAKE_CXXFLAGS -= -fopenmp
    QMAKE_CXXFLAGS += /openmp
    QMAKE_LFLAGS -= -fopenmp
}

SOURCES += \
    main.cpp \
    mainwindow.cpp

HEADERS += \
    mainwindow.h

FORMS += \
    mainwindow.ui
