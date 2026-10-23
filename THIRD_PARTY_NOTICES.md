# Third-party components

NetScope v1.1 links dynamically to Qt 6.8.3 and is distributed under GPL-3.0-only.

* Qt Core, GUI, Widgets, Network and platform plugins: The Qt Company Ltd. and
  contributors; GPL/LGPL/commercial licensing as described in the supplied sources.
* Qt WebSockets and Qt SVG: The Qt Company Ltd. and contributors; see module licenses.
* Qt MQTT: The Qt Company Ltd. and contributors; GPL-3.0-only (or commercial).
  This package uses the GPL option.
* MinGW GCC 13.1 runtime: GCC contributors; GPL with the GCC Runtime Library
  Exception, where applicable. MinGW-w64/winpthreads retain their own licenses.
* Third-party code embedded in Qt (including image/font/compression libraries)
  retains the notices in the source tree and module LICENSES folders.
* Microsoft deployment DLLs copied by Qt's deployment tool retain their own terms.

The portable package contains license texts under `licenses/`. The matching
`NetScope-v1.1-sources.zip` contains NetScope source, build/packaging scripts and
the installed Qt 6.8.3 source modules used for deployment: qtbase, qtsvg,
qtimageformats, qtwebsockets and qtmqtt, including bundled third-party source.
Provide both archives to recipients. No Qt source modifications were made.

Upstream sources and build tools:

* https://github.com/PsiXich/NetScope_D_bug
* https://download.qt.io/archive/qt/6.8/6.8.3/submodules/
* https://code.qt.io/cgit/qt/qtmqtt.git/tag/?h=v6.8.3
* https://gcc.gnu.org/releases.html (GCC 13.1.0)
* https://www.mingw-w64.org/

Qt was installed with the Qt Online Installer (MinGW 13.1.0, 64-bit).
The bundled compiler runtime license directory is copied from that installation.
