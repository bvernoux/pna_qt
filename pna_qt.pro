QT += core gui widgets printsupport svg

CONFIG += c++17 // Use C++17 features

# Check for Qt6 and MinGW
greaterThan(QT_MAJOR_VERSION, 5) {
    # We're using Qt6
    win32-g++ {
        # We're using MinGW on Windows
        QMAKE_CXXFLAGS += -Wa,-mbig-obj
        message("Adding -mbig-obj flag for MinGW with Qt6")
    }
}

TARGET = pna_qt
TEMPLATE = app

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API for details on how to replace it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if you use deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.#
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    main.cpp \
    phasenoiseanalyzerapp.cpp \
    utils.cpp \
    qcustomplot.cpp

HEADERS += \
    phasenoiseanalyzerapp.h \
    constants.h \
    resources.rc \
    utils.h \
    qcustomplot.h \
    version.h

RESOURCES += phasenoiseanalyzerapp.qrc

RC_FILE = resources.rc

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target
