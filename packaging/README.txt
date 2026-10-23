NetScope Debug v1.1 - Windows x64

QUICK START
1. Extract the complete NetScope-v1.1-windows-x64.zip archive.
2. Open the extracted folder and run netscope.exe.
3. Use + TCP Client / + TCP Server / + WebSocket / + WS Server / + UDP / + MQTT.
No Qt, Qt Creator or compiler installation is needed.
Keep all DLLs and plugin subfolders beside netscope.exe.

MQTT
Broker: enter hostname, port (default 1883) and optional credentials.
Subscriptions: subscribe to a topic/filter, for example netscope/#.
Publish: send a message to netscope/test to see outgoing and incoming traffic.
Use your own broker. The application does not install or start an MQTT broker.
MQTT uses plain TCP, clean sessions and manual reconnect. TLS is not implemented.

FEEDBACK
Please report your Windows version, protocol, steps, expected/actual behavior,
and a screenshot if the problem is visual. Remove passwords and private traffic.
Settings/logs are stored in your Windows user profile (NetScopeDebug).

DISTRIBUTION
Provide the matching NetScope-v1.1-sources.zip with this binary archive.
NetScope is GPL-3.0-only; see LICENSE and THIRD_PARTY_NOTICES.md.
This build is unsigned; Windows may require a trusted publisher under its local
application-control policy. An installer does not change that signing status.

BUILD
Run scripts/package-windows.ps1 from the source checkout. Defaults expect
D:/Qt/6.8.3/mingw_64 and D:/Qt/Tools/mingw1310_64. Paths can be overridden.
The script creates a new timestamped output directory on each run.
Test the package on a separate Windows 10/11 x64 computer before public release.
