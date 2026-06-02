QT       += core gui network

contains(QT_ARCH, arm)|contains(QMAKE_CC, arm-linux.*)|contains(QMAKE_CXX, arm-linux.*) {
    QMAKE_SYSROOT = /home/hun/buildroot-2020.02.12/output/host/arm-buildroot-linux-gnueabihf/sysroot
    QMAKE_PKG_CONFIG = /home/hun/buildroot-2020.02.12/output/host/bin/pkg-config
    QMAKE_CFLAGS += --sysroot=$$QMAKE_SYSROOT
    QMAKE_CXXFLAGS += --sysroot=$$QMAKE_SYSROOT
    QMAKE_LFLAGS += --sysroot=$$QMAKE_SYSROOT
    QMAKE_LIBDIR += $$QMAKE_SYSROOT/lib $$QMAKE_SYSROOT/usr/lib
    INCLUDEPATH += $$QMAKE_SYSROOT/usr/include
}

force_mplayer {
    DEFINES += USE_MPLAYER_BACKEND
    message(Video backend: mplayer)
} else:qtHaveModule(multimedia):qtHaveModule(multimediawidgets) {
    QT += multimedia multimediawidgets
    DEFINES += USE_QT_MULTIMEDIA_BACKEND
    contains(QT_ARCH, arm)|contains(QMAKE_CC, arm-linux.*)|contains(QMAKE_CXX, arm-linux.*) {
        CONFIG += link_pkgconfig
        PKGCONFIG += gstreamer-1.0 gstreamer-app-1.0 gstreamer-video-1.0
        DEFINES += USE_GSTREAMER_VIDEO_BACKEND USE_MUSIC_MPLAYER_BACKEND
    }
    message(Video backend: QtMultimedia)
} else {
    DEFINES += USE_MPLAYER_BACKEND
    message(Video backend: mplayer fallback; Qt multimedia modules were not found)
}

opencv_camera {
    DEFINES += USE_OPENCV_CAMERA
    contains(QT_ARCH, arm)|contains(QMAKE_CC, arm-linux.*)|contains(QMAKE_CXX, arm-linux.*) {
        CONFIG += link_pkgconfig
        PKGCONFIG += opencv
    } else:win32 {
        INCLUDEPATH += C:/opencv/build/include
        LIBS += -LC:/opencv/build/x64/mingw/lib \
                -lopencv_core \
                -lopencv_imgproc \
                -lopencv_videoio \
                -lopencv_imgcodecs
    } else {
        CONFIG += link_pkgconfig
        PKGCONFIG += opencv4
    }
    message(Camera backend: OpenCV)
} else {
    contains(QT_ARCH, arm)|contains(QMAKE_CC, arm-linux.*)|contains(QMAKE_CXX, arm-linux.*) {
        DEFINES += USE_V4L2_CAMERA
        message(Camera backend: V4L2)
    } else {
        message(Camera backend: disabled; add CONFIG+=opencv_camera to enable OpenCV camera)
    }
}

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++11

win32-g++ {
    QMAKE_CC = D:/Qt/Tools/mingw730_32/bin/gcc.exe
    QMAKE_CXX = D:/Qt/Tools/mingw730_32/bin/g++.exe
    QMAKE_LINK = D:/Qt/Tools/mingw730_32/bin/g++.exe
    QMAKE_LINK_C = D:/Qt/Tools/mingw730_32/bin/gcc.exe
    QMAKE_AR = D:/Qt/Tools/mingw730_32/bin/ar.exe cqs
    QMAKE_OBJCOPY = D:/Qt/Tools/mingw730_32/bin/objcopy.exe
    QMAKE_STRIP = D:/Qt/Tools/mingw730_32/bin/strip.exe
    QMAKE_RC = D:/Qt/Tools/mingw730_32/bin/windres.exe
    LIBS += -luser32
}

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0

SOURCES += \
    ap3216c.cpp \
    arcgraph.cpp \
    bootanimationpage.cpp \
    calculator.cpp \
    camerapage.cpp \
    glowtext.cpp \
    mappage.cpp \
    main.cpp \
    mainwindow.cpp \
    musicpage.cpp \
    musicplayer.cpp \
    musicprogressslider.cpp \
    swipeview.cpp \
    videopage.cpp \
    videoplayer.cpp \
    vinyldisc.cpp \
    weatherpage.cpp \
    wavegauge.cpp

HEADERS += \
    ap3216c.h \
    arcgraph.h \
    bootanimationpage.h \
    calculator.h \
    camerapage.h \
    glowtext.h \
    mappage.h \
    mainwindow.h \
    musicpage.h \
    musicplayer.h \
    musicprogressslider.h \
    swipeview.h \
    videopage.h \
    videoplayer.h \
    vinyldisc.h \
    weatherpage.h \
    wavegauge.h

# 界面由代码构建
# FORMS += mainwindow.ui

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    pic.qrc
