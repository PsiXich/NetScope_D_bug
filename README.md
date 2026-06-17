# NetScope Debug

> A lightweight network debugging tool for TCP and WebSocket development.

NetScope Debug started as a personal project created to simplify day-to-day network debugging tasks. During development I often needed a small utility that could quickly connect to a TCP or WebSocket endpoint, inspect incoming traffic, send custom messages and test server behavior without launching heavyweight tools.

Over time the project evolved into a standalone desktop application built with Qt and C++, combining client and server functionality in a single interface.

Whether you're debugging a backend service, testing a WebSocket API, experimenting with custom protocols or simply learning networking concepts, NetScope Debug aims to provide a fast and convenient workspace for exploring network communication.

---

## ✨ Features

### 🌐 Supported Protocols

* TCP Client
* TCP Server
* WebSocket Client
* WebSocket Server

### 🔌 Connection Management

Create and manage multiple connections simultaneously through a unified interface.

* Multiple active connections
* Connection monitoring
* Automatic reconnection
* Centralized connection manager

### 📡 Traffic Inspection

Observe network traffic in real time while testing your applications.

* Live message logging
* Incoming and outgoing traffic visualization
* Hexadecimal data view
* Timestamped events
* System activity tracking

### 📨 Data Transmission

* Text messages
* Binary payloads
* WebSocket text frames
* WebSocket binary frames
* Broadcast messaging for server instances

### ⚙️ Built With

* C++14
* Qt Widgets
* Qt Network
* Qt WebSockets
* CMake
* Qt Test

---

## 🚀 Getting Started

### Download Release

If you only want to use the application, download the latest installer from the **Releases** section.

**Latest Release**

`NetScope_v1.0_Setup.exe`

The installer was created using Inno Setup and includes everything required to run the application on Windows.

### Build From Source

```bash
cmake -B build
cmake --build build
```

Requirements:

* CMake 3.16+
* Qt 5.15+ or Qt 6.x
* C++14 compatible compiler

---

## 📂 Project Structure

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

## 🧪 Automated Tests

The project includes tests for the most important components:

* ConnectionManager
* TCP Client
* TCP Server
* WebSocket Client
* WebSocket Server
* MessageLogModel

---

## 🎯 Why I Built This

The goal of this project was not only to create a useful debugging utility but also to deepen my understanding of:

* Qt application architecture
* TCP networking
* WebSocket communication
* Model/View design patterns
* Automated testing
* Cross-platform desktop development

---

## 🔮 Future Plans

* UDP support
* MQTT support
* Session persistence
* Traffic export/import
* Packet filtering
* Protocol analyzers

---

## 📦 Release Notes

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

This project is available for educational, learning and development purposes.
