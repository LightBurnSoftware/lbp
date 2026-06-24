# Simulator (lbp-simulator)
This simulator is a Qt Desktop application, developed simultaneously with the protocol specification and lbp library,
both to assist in development of the protocol itself, and as a sample of the lbp software library in action.

## Overview
While the simulator *is* a desktop application and does make use of Qt and the C++ standard library,
its architecture adopts a layered approach.
Usage of Qt and aspects of C++ that are not friendly to embedded contexts are limited to
the "upper" layers of the software - the main window, the debug logger, the
transportation layer, and configuration storage.

The actual simulation loop written to be embedded-friendly, with no heap allocations and no use of Qt.
(See `firmwaresim.h/cpp`, `movement.h/cpp`). It's not actual firmware, but it is written to be separate and clean enough from the
desktop application concerns to serve as an example for `lbp` library usage.

## Simulator classes
- **MainWindow:** The Qt entrance point for the desktop application software. It displays the global log messages, connection status, and a simulation view.
- **SimView:** A widget displaying a simple view of the current laser position. It preserves cuts. (Press c to clear.)
- **Transport:** A Qt base class that provides an interface for the transport layer, allowing communication with LightBurn. 
- **SerialTransport:** An implemention of Transport that uses `QSerialPort` for serial communication.
- **TCPTransport:** An implemention of Transport that uses `QTCPSocket` for TCP communication.
- **FirmwareSim:** This is where the simulation `loop` is implemented. It parses **Payloads** from **Connection**, processes them, and sends output **Messages** back through the **Connection**.
- **MovementSim:** This is where the physical state of the machine is simulated. It is responsible for all movement and laser commands. It maintains its own **Queue** of **CmdPayloads** which it executes in order.
- **FileSystem:** This class is responsible for receiving, concatenating, and parsing files sent in `cmd_file_chunk`. As more file-related commands are implemented, its capabilites are expected to expand.
- **Configuration:** This class is responsible for processing commands related to `cfg_` messages. It stores configuration data between sessions using `json`.
