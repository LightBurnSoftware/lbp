# LBP Library
This library is provided to assist in firmware development.

## Simple Build Example
```
cmake -B build
cmake --build build -j
./build/TestLBP
```

## Install Example

```
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release -j
cmake --install build --prefix /tmp/lbp-install
```

## Brief Overview

- `spec.h`: defines the specification for LBP messages and all command and configuration codes.
- `payload.h`/`payload.cpp`: defines a the Payload class, a helpful container for the payload section of successfully parsed lbp messages.
- `parser.h`: defines a buffered Parser class, which can parse incoming bytes into lbp Payloads.
- `ringbuffer.h`: defines a Ring Buffer, which provides the buffer space for a Parser.
- `message.h`: defines the Message class, provided to help assemble outgoing lbp messages for transmission.
- `beio.h`/`beio.cpp`: defines helper functions for reading and writing Big-Endian values to byte buffers.
- `queue.h`: a templated FIFO queue. Useful for queueing up movement commands or preparing outgoing messages for transmission.
- `checksum.h`/`checksum.cpp`: provides the crc16 implementation.

This library is intended to be firmware-ready and embedded-friendly, so it requires no heap allocations. All container classes
are templated for maximum capacity with helpful specializations provided in the headers.

These classes are documented in the headers with usage examples, but it's worth discussing **Payload** and **Message**
in particular, since it may seem odd that we `lbp` provides two separate helper classes for messages.

### The Payload class

The **Payload** class contains the payload section of a parsed LBP message. It is designed to be constructed by a Parser.
**Payload** provides helpful methods for reading arguments. Think of it as the **input class** of a LBP message.
The header checksum are not included, as they are not useful after the payload has been parsed.

The **Payload** class is templated by maximum capacity. Two template specializations are provided, which should be all that is necessary:
- `CmdPayload` is large enough to fit all commands that are not `cmd_file_chunk`, as none are expected to require more than four 32-bit integers.
- `MaxPayload` is large enough to accommodate the maximum possible payload, being a `cmd_file_chunk`. The **Parser** outputs one of these at a time.

(See `lbp/spec.h` for the definitions of these dimensions).

### The Message class

The **Message** class assists in the construction of a full LBP message to prepare for transport. It provides
convenient constructors for common message types, along with helper methods for writing arguments.
Additionally, the **Message** class automatically computes the checksum once all expected arguments are written.
Think of it as the **output class** of a LBP message.

Like the **Payload** class, **Message** is also templated by maximum capacity.
- `CmdMsg` is large enough for all commands that are not `cmd_file_chunk`
- `MaxMsg` is large enough to accommodate the largest possible output message.

(See `lbp/spec.h` for the definitions of these dimensions).
