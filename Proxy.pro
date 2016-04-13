TEMPLATE = app
CONFIG += console c++11
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    proxy.cpp \
    httpparser.cpp \
    logger.cpp

HEADERS += \
    proxy.hpp \
    httpparser.hpp \
    logger.hpp
