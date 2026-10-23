# NetScope Debug

> A lightweight network debugging tool for TCP, WebSocket, UDP and MQTT development.

NetScope Debug started as a personal project created to simplify day-to-day network debugging tasks. During development I often needed a small utility that could quickly connect to a TCP or WebSocket endpoint, inspect incoming traffic, send custom messages and test server behavior without launching heavyweight tools.

Over time the project evolved into a standalone desktop application built with Qt and C++, combining client and server functionality in a single interface.

Whether you're debugging a backend service, testing a WebSocket API, experimenting with custom protocols or simply learning networking concepts, NetScope Debug aims to provide a fast and convenient workspace for exploring network communication.

---

## Features

### Supported Protocols

* TCP Client
* TCP Server
* WebSocket Client
* WebSocket Server

* UDP endpoint (send, receive and broadcast)
* MQTT 3.1.1 client (subscribe/publish, QoS 0/1/2, retain)

### Appearance and Filtering

* Dark violet theme with readable status colors
* Journal filters by connection, protocol, direction and payload text
* MQTT topic column and topic filters with `+` / `#` wildcards

### Connection Management

Create and manage multiple connections simultaneously through a unified interface.

* Multiple active connections
* Connection monitoring
* Automatic reconnection for TCP and WebSocket clients
* Centralized connection manager

### Traffic Inspection

Observe network traffic in real time while testing your applications.

* Live message logging
* Incoming and outgoing traffic visualization
* Hexadecimal data view
* Timestamped events
* System activity tracking

### Data Transmission

* Text messages
* Binary payloads
* WebSocket text frames
* WebSocket binary frames
* Broadcast messaging for server instances

### Built With

* C++14
* Qt Widgets
* Qt Network
* Qt WebSockets
* Qt MQTT
* CMake
* Qt Test

---

## Getting Started

### Download Release

Download the [latest NetScope release](https://github.com/PsiXich/NetScope_D_bug/releases/latest)
for Windows 10/11 x64. Run `NetScope_v1.1_Setup.exe` to install or update v1.0.
Qt, Qt Creator and MinGW are not required on the recipient's computer.
The release includes the matching source archive and SHA-256 checksums.

A portable ZIP can also be built with the script below: extract the entire archive
and run `netscope.exe`. Keep the DLLs and plugin folders next to the executable.
Settings and logs are stored in the user's Windows profile. The executable and
installer are unsigned.

### Create the Windows package

With Qt 6.8.3 MinGW and Qt MQTT installed, run:

```powershell
./scripts/package-windows.ps1
```

The script builds Release, deploys Qt and MinGW dependencies with `windeployqt`,
and writes the portable ZIP, source ZIP and SHA-256 checksums under `build/packages`.
See `packaging/README.txt` for distribution and testing notes.

### Build From Source

```bash
cmake -B build
cmake --build build
```

Requirements:

* CMake 3.16+
* Qt 5.15+ or Qt 6.x
* C++14 compatible compiler

### Windows: Qt 6.8.3 + MinGW

Open the root `CMakeLists.txt` in Qt Creator and select **Desktop Qt 6.8.3
MinGW 64-bit** (GCC 13.1). The `llvm-mingw` kit is a separate toolchain and
requires its own Qt MQTT build. After installing MQTT, run CMake again; if
the old configuration persists, clear the CMake configuration and reconfigure.

Required Qt modules: Core, Gui, Widgets, Network, WebSockets, Mqtt and Test.
Qt 6 uses C++17, which its CMake targets enable automatically.

The example below assumes Qt MQTT 6.8.3 Debug and Release libraries in
`D:/Qt/6.8.3/mingw_64`. To rebuild the module using the sources included with Qt,
run the following in PowerShell from the project directory:

```powershell
$env:PATH = "D:\Qt\Tools\mingw1310_64\bin;D:\Qt\Tools\Ninja;D:\Qt\6.8.3\mingw_64\bin;D:\Qt\Tools\CMake_64\bin;" + $env:PATH
cmake -S D:/Qt/6.8.3/Src/qtmqtt -B build/mqtt-rebuild -G "Ninja Multi-Config" -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/mingw_64 -DCMAKE_INSTALL_PREFIX=D:/Qt/6.8.3/mingw_64 "-DCMAKE_CONFIGURATION_TYPES=Debug;Release" -DQT_BUILD_TESTS=OFF -DQT_BUILD_EXAMPLES=OFF
cmake --build build/mqtt-rebuild --config Release --parallel 6
cmake --build build/mqtt-rebuild --config Debug --parallel 6
cmake --install build/mqtt-rebuild --config Release
cmake --install build/mqtt-rebuild --config Debug

cmake -S . -B build/verified-qt683 -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=D:/Qt/6.8.3/mingw_64
cmake --build build/verified-qt683 --parallel 6
ctest --test-dir build/verified-qt683 --output-on-failure
```

### MQTT

Use **+ MQTT** to create a connection. The panel contains three tabs:

* **Broker**: hostname/IP, port (1883 by default), optional Client ID and credentials.
  An empty Client ID generates a new ID. Connect/Disconnect remain visible on every tab.
* **Subscriptions**: enter an MQTT filter such as `home/#` or `sensor/+/temperature`,
  select QoS 0/1/2 and subscribe. The list shows broker-confirmed subscriptions.
  Select a row to unsubscribe. Broker rejections appear below the panel.
* **Publish**: enter a topic without wildcards and a UTF-8 message or HEX byte pairs.
  QoS 0/1/2, retained messages and empty payloads are supported. An empty retained
  payload clears the retained value on the broker. Publishing reports that a message
  is queued; it does not imply delivery to a subscriber.

Traffic appears in the shared journal with a **Topic** column and MQTT topic filtering.
Each MQTT connection is independent. This version uses MQTT 3.1.1 over plain TCP
and clean sessions: after reconnecting, subscribe again. TLS, persistent sessions
and automatic reconnect are not implemented. Passwords are not saved to settings.

For a local smoke test, connect to your broker, subscribe to `netscope/#`, then
publish `Hello MQTT` to `netscope/test`. The journal should contain both the outgoing
and incoming message. Automated `tst_mqttclient` tests use their own local MQTT peer
and need no installed broker or internet access.

---

## Project Structure

```text
src/
├── core/        # Networking logic
├── models/      # Qt data models
├── ui/          # Application interface
├── utils/       # Logging and helper utilities
└── main.cpp

tests/
└── Automated Qt Test suite
```

---

## Automated Tests

The project includes tests for the most important components:

* ConnectionManager
* TCP Client
* TCP Server
* WebSocket Client
* WebSocket Server
* MessageLogModel
* UDP endpoint
* MQTT lifecycle, broker rejections and panel workflows

---

## Why I Built This

The goal of this project was not only to create a useful debugging utility but also to deepen my understanding of:

* Qt application architecture
* TCP networking
* WebSocket communication
* Model/View design patterns
* Automated testing
* Cross-platform desktop development

---

## Future Plans

* Session persistence
* Traffic export/import
* Protocol analyzers
* Advanced packet filtering beyond the existing journal filters
* Selectable color themes
* MQTT TLS, persistent sessions and automatic reconnect

---

## Release Notes

### v1.1

* Add UDP endpoint controls and tests.
* Complete the MQTT client panel, confirmed subscriptions, publishing and error handling.
* Add MQTT topic display and filtering; retain journal text/protocol/direction filters.
* Introduce the dark violet application theme.
* Provide a Windows x64 installer and portable packaging script with Qt and MinGW runtime dependencies.
* License NetScope under GPL-3.0-only; include source and third-party notices.

### v1.0

Initial public release.

Features included:

* TCP Client
* TCP Server
* WebSocket Client
* WebSocket Server
* Logging system
* Hex viewer
* Automated tests
* Windows installer package

---

## License

Copyright (c) 2026 Artyom (PsiXich).

NetScope Debug is licensed under **GNU GPL version 3 only (GPL-3.0-only)**.
See [LICENSE](LICENSE). You may use, modify and redistribute it under those terms.
Qt and other bundled components retain their respective licenses. Distribute the
matching source archive with the binary package; see [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
