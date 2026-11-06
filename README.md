# DC2PHP Reference Manual

## Overview

`dc2php` is a high-performance, lightweight native Win32 wrapper written in pure C (`C89/C99`). It serves as an automated execution bridge designed to parse structured JSON configurations, manage recursive root path contexts, and orchestrate private background PHP worker sessions with real-time pipe-based I/O redirection and persistent metrics tracking.

---

## Architectural Specifications

* **Core Binary**: `dc2php.exe` (Win64 Console Subsystem)
* **Language & Standard**: C89/C99, Native Win32 API (`CreateProcessA`, anonymous pipes)
* **Build Toolchain**: Microsoft Visual Studio MSVC (`cl.exe`, `rc.exe`), MSYS2 UCRT64 compatibility
* **Memory & Handle Tracking**: Integrated with `DX-TRACK` (`Tracker.c`/`Tracker.h`) for safe resource allocation management
* **JSON Processing**: Powered by lightweight embedded `cJSON` parser
* **Metadata & Resource Binding**: Bound via Win32 resource script (`resource.rc`) providing standard file versioning, internal naming, and publisher attribution (`Mercwar AI`)

---

## Command-Line Interface (CLI) Reference

```text
dc2php.exe [command] [arguments...]

```

### Supported Flags

| Flag | Argument | Description |
| --- | --- | --- |
| `-json` | `<config.json>` | Parses and executes a recursive script pipeline from a JSON configuration file. |
| `-bat` | `<script.bat> [args...]` | Executes a Windows batch script wrapper through the pipeline. |
| `-exe` | `<script.exe> [args...]` | Executes a compiled executable file wrapper through the pipeline. |
| `-pass` | `[php_flags...]` | Direct passthrough mode forwarding all subsequent parameters straight to `php.exe`. |
| `-h`, `--help` | *None* | Displays the built-in usage manual reference. |

---

## JSON Configuration Schema

The execution pipeline supports recursive root path changes, object script mappings, and query string parameter tokenization. Metadata keys such as `"project"`, `"name"`, `"version"`, and `"author"` are automatically bypassed using a whitelisted task container approach (`"scripts"`, `"pipeline"`, `"tasks"`, `"actions"`, `"steps"`).

### Example (`tests.json`)

```json
{
  "root": ".",
  "project": "Mercwar Automation Workspace",
  "version": "1.1.0",
  "scripts": [
    "test.php?step=boot&mode=clean",
    "test.php?step=verify&mode=deep",
    "test.php?step=finalize&mode=sync"
  ]
}

```

---

## State Persistence & Telemetry (`metrics.json`)

Every execution of `dc2php.exe` updates a global `metrics.json` state file located in the working execution directory. This tracks cumulative usage across application launches and active sessions:

```json
{
    "application_hits": 47,
    "session_hits": 47
}

```

---

## Compilation & Build Pipeline

The project is compiled using a standardized automated batch builder (`build.bat`) that provisions the MSVC x64 environment, compiles Win32 resource metadata blocks (`rc.exe`), and links the standalone binary:

```bat
@echo off
build.bat

```
