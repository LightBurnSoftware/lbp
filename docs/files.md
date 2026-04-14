# File Transfer

Commands in the `0x4000` MSN category handle file uploads, storage management, and file execution. File transfer is an **optional, advanced feature** — basic LBP support streams commands in real-time and does not require file storage on the controller.

---

## Command Summary

### File Upload (Core)

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `begin_file` | `0x4404` | file_size | int32 bytes | No |
| `file_chunk` | `0x44FC` | data | raw bytes (up to 502B) | No |
| `end_file` | `0x4405` | none | — | No |
| `execute` | `0x0C66` | — | — | No |

### File Management

| Command | Code | Arguments | Types & Units | Required |
|---------|------|-----------|---------------|----------|
| `get_filename` | `0x4401` | file_index | int16 | DRAFT |
| `set_filename` | `0x4402` | file_index | int16 | DRAFT |
| `delete_file` | `0x4403` | file_index | int16 | DRAFT |
| `new_file` | `0x4406` | — | — | DRAFT |
| `file_count` | `0x4407` | — | — | DRAFT |
| `file_time` | `0x4408` | — | — | DRAFT |
| `calc_file_time` | `0x4409` | file_index | int16 | DRAFT |

### Storage Queries

| Command | Code | Arguments | Response | Required |
|---------|------|-----------|----------|----------|
| `flash_available` | `0x45FA` | none | int64 bytes | DRAFT |
| `mainboard_version` | `0x45B0` | none | — | DRAFT |

---

## File Upload Sequence

To upload a file to the controller, LightBurn uses a three-step sequence:

### 1. Begin File (`0x4404`)

Start the upload. The argument is the total file size in bytes as an int32.

**Payload size:** 6 bytes (2 cmd + 4 file_size)

The controller should allocate a buffer for the incoming file.

### 2. File Chunks (`0x44FC`)

Send the file data in chunks. Each chunk is an LBP message with a fixed total size of **512 bytes**:

- 4 bytes: DRGN header
- 2 bytes: size field
- 2 bytes: command code (`0x44FC`)
- Up to 502 bytes: file data
- 2 bytes: CRC16

The controller appends each chunk to its file buffer in order.

### 3. End File (`0x4405`)

Mark the upload as complete. No arguments.

The controller should finalize the file in storage.

### Upload Sequence Diagram

```
LightBurn                     Controller
    |                              |
    |--- begin_file (size=65000) ->|  allocate buffer
    |--- file_chunk (498 bytes) -->|  append
    |--- file_chunk (498 bytes) -->|  append
    |--- ...                       |  ...
    |--- file_chunk (remaining) -->|  append
    |--- end_file ---------------->|  finalize
    |                              |
```

---

## File Execution

### `execute` (`0x0C66`)

Note that `execute` uses code `0x0C66`, placing it in the Fundamentals category (`0x0000`) rather than the Files category. This is because file execution is part of the core job control flow — see also [Job Control](job-control.md).

> **DRAFT** — The argument format for `execute` is not yet finalized. The command may take a file index or execute the most recently uploaded file.

---

## File Management

> **DRAFT** — The following commands are planned but not yet finalized. Argument formats and return types may change.

- **`get_filename`** (`0x4401`) — Retrieve the name of a stored file by index.
- **`set_filename`** (`0x4402`) — Set the name of a stored file by index.
- **`delete_file`** (`0x4403`) — Delete a stored file by index.
- **`new_file`** (`0x4406`) — Create a new empty file slot.
- **`file_count`** (`0x4407`) — Query the number of stored files.
- **`file_time`** (`0x4408`) — Query the execution time of a stored file.
- **`calc_file_time`** (`0x4409`) — Request the controller to calculate the execution time for a stored file.

---

## Storage Queries

> **DRAFT** — These queries are planned but not yet finalized.

- **`flash_available`** (`0x45FA`) — Query available flash storage in bytes. Expected response: int64.
- **`mainboard_version`** (`0x45B0`) — Query the controller's firmware/hardware version.
