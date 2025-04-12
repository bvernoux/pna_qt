Informations to do a static build of Qt 5.15.2(x64) with Visual Studio 2019
* http://doc.qt.io/qt-5/windows-building.html
* https://doc.qt.io/qt-5/windows-requirements.html
For new version of Qt search 5.15.2 & Qt5_15_2 and replace by new version (potentially also replace 5.15 ...)

Warning:
* This version of Qt requires Python3.8 to be installed (except if built with -skip qtdeclarative like done)
* The installation path is intended to be in `C:\Qt`

Steps:

1) Download and extract qt-everywhere-src-5.15.2 from http://download.qt.io/official_releases/qt/5.15/5.15.2/single/qt-everywhere-src-5.15.2.zip
In this example extract it to C:\Qt\Qt5_15_2-msvc2019_x64_static_build\qt-everywhere-src-5.15.2

2) Download and extract jom from https://download.qt.io/official_releases/jom/

* `copy jom.exe to C:\Qt\Qt5_15_2-msvc2019_x64_static_build\qt-everywhere-src-5.15.2`

3) Go to C:\Qt\Qt5_15_2-msvc2019_x64_static_build\qt-everywhere-src-5.15.2\qtbase\mkspecs\common
and change msvc-desktop.conf like this (change all MD to MT to remove dependency on msvc dlls)

* Initial values:
```
	QMAKE_CFLAGS_RELEASE    = $$QMAKE_CFLAGS_OPTIMIZE -MD
	QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO += $$QMAKE_CFLAGS_OPTIMIZE -Zi -MD
	QMAKE_CFLAGS_DEBUG      = -Zi -MDd
```
* Should be changed to:
```
	QMAKE_CFLAGS_RELEASE    = $$QMAKE_CFLAGS_OPTIMIZE -MT
	QMAKE_CFLAGS_RELEASE_WITH_DEBUGINFO += $$QMAKE_CFLAGS_OPTIMIZE -Zi -MT
	QMAKE_CFLAGS_DEBUG      = -Zi -MTd
```

4) Build Qt5

* Launch windows command line (cmd) and type following commands:
```
cd C:\
SET PYTHONPATH=
CALL "C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Auxiliary\Build\vcvarsall.bat" x64
SET _ROOT=C:\Qt\Qt5_15_2-msvc2019_x64_static_build\qt-everywhere-src-5.15.2
SET PATH=%_ROOT%\qtbase\bin;%_ROOT%\gnuwin32\bin;%PATH%
REM Uncomment the below line when using a git checkout of the source repository
REM SET PATH=%_ROOT%\qtrepotools\bin;%PATH%
SET _ROOT=

cd C:\Qt\Qt5_15_2-msvc2019_x64_static_build\qt-everywhere-src-5.15.2
configure.bat -static -release -prefix "C:\Qt\Qt5.15.2_msvc2019_x64_static" -platform win32-msvc2019 -opensource -confirm-license -qt-zlib -qt-pcre -qt-libpng -qt-libjpeg -qt-freetype -opengl desktop -no-openssl -make libs -nomake tools -nomake examples -nomake tests -skip wayland -skip purchasing -skip serialbus -skip script -skip scxml -skip speech -skip qtwebengine -skip qtdeclarative
jom
jom install
```
