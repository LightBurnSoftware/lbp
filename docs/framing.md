# Message Framing

Every LBP message uses the same wire format. This page defines that format in full. **Read this page before any other spec page** — all command documentation assumes you understand the framing described here.

## Message Structure

```
+----------------+--------+---------+-------+
| Header (4B)    | Size   | Payload | CRC16 |
| 0x4452474E     | (2B)   | (var)   | (2B)  |
+----------------+--------+---------+-------+
```

An LBP message is a contiguous sequence of bytes consisting of four fields, transmitted in the order shown above. Each field is described below.

---

## Header

| Offset | Length | Value        |
|--------|--------|--------------|
| 0      | 4      | `0x4452474E` |

The header is the constant four-byte sequence `0x4452474E`, which is the ASCII encoding of `DRGN`. Every LBP message begins with these bytes.

The header serves as a synchronization marker. When a parser first connects to a byte stream — or after discarding a corrupted message — it scans byte-by-byte until it finds this pattern, then begins reading the rest of the message.

## Size

| Offset | Length | Encoding   | Range    |
|--------|--------|------------|----------|
| 4      | 2      | Big-endian | 2 -- 504 |

The size field is a 2-byte big-endian unsigned integer representing the length of the **payload only**. It does not include the header, the size field itself, or the CRC.

- **Minimum value: 2.** The smallest payload is a 2-byte command code with no arguments.
- **Maximum value: 504.** The largest payload occurs in file chunk messages (512-byte message minus 4 header, 2 size, 2 CRC).

## Payload

| Offset | Length        | Contents                          |
|--------|--------------|-----------------------------------|
| 6      | `size` bytes | Command code (2B) + arguments (0+B) |

The payload begins with a 2-byte command code, followed by zero or more argument bytes. The command code determines how many argument bytes follow and how to interpret them. See the individual command pages for argument formats.

## CRC16

| Offset         | Length | Encoding   |
|----------------|--------|------------|
| 6 + `size`     | 2      | Big-endian |

A CRC16-CCITT checksum is appended after the payload.

- **Polynomial:** `0x1021` (bit-reversed form: `0x8408`)
- **Initial value:** `0xFFFF`
- **Final XOR:** Inverted (`~crc`)
- **Input:** The payload bytes only. The header and size field are **not** included in the CRC calculation.
- **Output:** 2 bytes, big-endian.

The receiver computes the CRC over the received payload and compares it to the transmitted CRC. If they do not match, the message is discarded.

### Reference Implementation

```c
uint16_t crc16(const uint8_t *data_p, uint16_t len)
{
    const uint16_t POLY = 0x8408;
    uint8_t i;
    uint16_t data;
    uint16_t crc = 0xFFFF;

    if (len == 0)
        return (~crc);

    do {
        for (i = 0, data = (uint16_t)0xFF & *data_p++; i < 8; i++, data >>= 1) {
            if ((crc & 0x0001) ^ (data & 0x0001))
                crc = (crc >> 1) ^ POLY;
            else
                crc >>= 1;
        }
    } while (--len);

    crc = ~crc;
    data = crc;
    crc = (crc << 8) | (data >> 8 & 0xFF);

    return crc;
}
```

---

## Byte Order

All multi-byte integers in LBP are **big-endian** (network byte order). This applies to:

- The size field
- Command codes
- All command arguments
- The CRC

There are no exceptions. If a field is wider than one byte, the most significant byte is transmitted first.

---

## Message Sizes

| Message Type            | Total Size | Breakdown                                        |
|-------------------------|------------|--------------------------------------------------|
| Minimum message         | 10 bytes   | 4 header + 2 size + 2 command code + 2 CRC      |
| Maximum command message | 26 bytes   | 10 + 16 bytes of arguments                      |
| File chunk message      | 512 bytes  | Fixed size for bulk file transfers               |

- **Command messages** carry a 2-byte command code plus up to 16 bytes of arguments, for a maximum payload of 18 bytes.
- **File chunk messages** are always 512 bytes total. The payload is 504 bytes (2-byte command code + 502 bytes of file data).

---

## Parsing State Machine

An LBP parser is a four-state machine that reads from a byte stream:

```
  +------------+     found 0x4452474E     +------+
  | HeaderSync | -----------------------> | Size |
  +------------+                          +------+
       ^                                     |
       |                              read 2 bytes
       |                                     v
       |                                +------+
       |                                | Data |
       |                                +------+
       |                                     |
       |                          read `size` bytes
       |                                     v
       |                              +----------+
       |   CRC mismatch              | Checksum |
       +----------------------------- +----------+
                                           |
                                      CRC match
                                           v
                                    [deliver message]
                                           |
                                           v
                                    +------------+
                                    | HeaderSync |
                                    +------------+
```

### States

1. **HeaderSync** -- Scan the incoming byte stream one byte at a time, looking for the four-byte sequence `0x4452474E`. Bytes that do not complete this pattern are discarded. Once the full header is found, transition to **Size**.

2. **Size** -- Read the next 2 bytes and interpret them as a big-endian unsigned integer. This is the payload length. Transition to **Data**.

3. **Data** -- Read exactly `size` bytes of payload. Transition to **Checksum**.

4. **Checksum** -- Read the next 2 bytes and interpret them as a big-endian CRC16. Compute the CRC over the payload bytes collected in the Data state. If the computed CRC matches the received CRC, deliver the message to the application layer. If it does not match, discard the message. In either case, return to **HeaderSync**.

> **Implementation note:** On CRC failure, a robust parser should back up and re-scan the discarded bytes for a header pattern, in case a valid message was embedded within what appeared to be a corrupted one.

---

## Command Code Structure

Command codes are 2 bytes (16 bits). The most significant nibble (MSN) indicates the broad category of the command:

| MSN    | Category                          |
|--------|-----------------------------------|
| `0x0`  | Fundamentals (handshake, stop, pause, job structure) |
| `0x1`  | Laser control                     |
| `0x4`  | File system                       |
| `0x5`  | Per-job settings                  |
| `0x6`  | Movement                          |
| `0x7`  | Tool control                      |
| `0x8`  | Machine state and telemetry       |
| `0xC`  | Persistent configuration          |
| `0xD`  | Vendor-specific extensions (reserved) |
| `0xE`  | Future capabilities (reserved)    |

The remaining nibbles carry sub-category and axis information. The command code structure is designed for **human readability** — when examining LBP traffic in a hex viewer or tool like Wireshark, you can quickly identify what a command does by looking at its nibbles.

The least significant nibble (LSN) often indicates which axes a command applies to:

| Flag   | Axis |
|--------|------|
| `0x1`  | X    |
| `0x2`  | Y    |
| `0x4`  | Z    |
| `0x8`  | U    |

For example, a movement command with `0x3` in the LSN applies to both X and Y. This makes traffic easy to read at a glance.

> **Important:** The nibble structure is a naming convention for readability, not a runtime construction mechanism. You cannot assume that combining flags will produce a valid command. Only the command codes explicitly listed in this documentation are defined. Always use the exact codes from the spec pages — do not attempt to derive new commands by combining flags.

---

## Examples

### Example 1: Absolute XY Move

Move the laser head to position (10 mm, 12 mm) using the `move_abs_xy` command.

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 0A           Payload size: 10 bytes
 6      6A 03           Command: move_abs_xy (0x6A03)
 8      00 00 27 10     X position: 10000 um (10 mm)
12      00 00 2E E0     Y position: 12000 um (12 mm)
16      XX XX           CRC16 (computed over bytes 6-15)
```

**Total message size:** 18 bytes.

Breaking down the command code `0x6A03`:
- `0x6___` -- MSN = 6: movement category
- `0x_A__` -- `0x0A00`: absolute move sub-category
- `0x___3` -- LSN = 3: axis flags X (`0x1`) + Y (`0x2`)

The two arguments are signed 32-bit big-endian integers representing positions in micrometers.

### Example 2: Stop (No Arguments)

The simplest possible message -- a stop command with no arguments.

```
Offset  Bytes           Description
------  --------------  ------------------------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      0C FF           Command: stop (0x0CFF)
 8      XX XX           CRC16 (computed over bytes 6-7)
```

**Total message size:** 10 bytes (the minimum possible LBP message).

The payload contains only the 2-byte command code. There are no arguments. The CRC is computed over just those 2 payload bytes.
