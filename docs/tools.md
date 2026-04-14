# Tool Control

Commands in the `0x7000` MSN category control auxiliary tools and peripherals. Currently, this category includes air assist control. Additional tool commands may be added in future protocol revisions.

---

## Command Summary

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `air_on` | `0x7FF1` | none | — | No |
| `air_off` | `0x7FF0` | none | — | No |

---

## Air Assist

### `air_on` (`0x7FF1`)

Activate compressed air flow to the cutting head. LightBurn sends this before cutting operations on layers where air assist is enabled.

### `air_off` (`0x7FF0`)

Deactivate compressed air flow. LightBurn sends this after cutting operations complete for the layer.

Both commands take **no arguments**. Payload is exactly 2 bytes (command code only).

### Example: Air On

```
Offset  Bytes           Description
------  --------------  ---------------------
 0      44 52 47 4E     DRGN header
 4      00 02           Payload size: 2 bytes
 6      7F F1           Command: air_on
 8      XX XX           CRC16
```

### Implementation Notes

Air assist is optional. Many machines handle air assist independently of software control (e.g., always on, or controlled by a physical switch). If the controller does not support software-controlled air assist, it can safely ignore these commands.

---

## Future Tools

The `0x7000` range is reserved for tool control. Possible future additions include:

- Water cooling control
- Exhaust fan control
- Rotary chuck control
- Other auxiliary peripherals

These will be documented here as they are defined.
