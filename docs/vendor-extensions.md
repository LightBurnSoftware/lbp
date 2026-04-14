# Vendor Extensions

The `0xD000` MSN range is reserved for manufacturer-specific commands. This allows vendors to extend the protocol for hardware features that are not covered by the standard LBP command set.

---

## Purpose

Every laser platform has unique capabilities — proprietary calibration routines, specialized sensors, custom homing sequences, or diagnostic tools. The vendor extension range provides a structured way to support these features within the LBP framework, without conflicting with standard commands.

---

## Rules

1. **Vendor commands MUST use command codes in the range `0xD000`–`0xDFFF`.** This is 4,096 possible command codes — more than enough for any vendor's needs.

2. **Vendor commands MUST follow standard LBP framing.** Every vendor command is a normal LBP message: DRGN header, size field, payload (command code + arguments), CRC16. No exceptions.

3. **Vendor commands SHOULD be documented internally.** Even though vendor commands are proprietary, maintaining internal documentation of the argument format and behavior will help with firmware development and debugging.

4. **LightBurn will ignore unrecognized vendor commands.** If LightBurn receives a response containing a vendor-specific command code it doesn't know about, it will skip it without error. Vendor commands will never cause protocol-level failures.

---

## Use Cases

- **Proprietary calibration:** Commands that trigger vendor-specific alignment or calibration procedures.
- **Custom sensors:** Reading proprietary sensor data (e.g., custom temperature probes, chiller status, material sensors).
- **Diagnostic commands:** Factory test modes, self-check routines, firmware update triggers.
- **Machine-specific features:** Unique hardware capabilities not covered by the standard protocol (e.g., custom door interlocks, bed leveling, air filtration control).

---

## Command Code Assignment

Within the `0xD000`–`0xDFFF` range, vendors are free to assign command codes however they see fit. We recommend:

- Organize by function using the second nibble (e.g., `0xD1xx` for calibration, `0xD2xx` for sensors).
- Use the axis flag convention in the LSN if applicable (x=0x1, y=0x2, z=0x4, u=0x8).
- Document the argument format for each command.

---

## Standardization Path

If a vendor develops a command that would be broadly useful across multiple manufacturers, it can be proposed for inclusion in the standard protocol. Commands promoted to standard status would move to the `0xE000` range (Extended Future Capabilities), which is reserved for this purpose.

Contact LightBurn to discuss standardization of vendor commands.

---

## Reserved Ranges

| Range | Purpose | Status |
|-------|---------|--------|
| `0xD000`–`0xDFFF` | Vendor-specific extensions | Available for use |
| `0xE000`–`0xEFFF` | Extended future capabilities | Reserved — do not use |
| `0xF000`–`0xFFFF` | Unused | Reserved — do not use |

Vendors must NOT use the `0xE000` or `0xF000` ranges. These are reserved for future protocol revisions.
