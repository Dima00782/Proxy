TEMPLATE = app
CONFIG += console c++14
CONFIG -= app_bundle
CONFIG -= qt

SOURCES += main.cpp \
    proxy.cpp \
    httpparser.cpp \
    ipaddress.cpp \
    selector.cpp \
    tcpsocket.cpp \
    logger.cpp

HEADERS += \
    proxy.hpp \
    httpparser.hpp \
    ipaddress.hpp \
    selector.hpp \
    tcpsocket.hpp \
    logger.hpp
