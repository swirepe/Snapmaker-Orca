# Embedded 3MF Projects in G-code

## Summary

Snapmaker Orca can optionally append an editable 3MF project to an exported text G-code file. The payload is represented entirely by
G-code comment lines so firmware that ignores comments can continue to print the file. When Snapmaker Orca opens such a G-code file, it
offers to open the embedded project or to continue in G-code preview mode.

## Preferences

The General > Project section contains these persistent application preferences:

- **Embed project in exported G-code**: disabled by default. When enabled, raw G-code written to a local or removable drive and raw G-code
  prepared for a printer-host upload contain an embedded 3MF. Existing `.gcode.3mf` archive uploads are unchanged.
- **Project contents**: `All plates` by default, or `Current plate`. This controls the plate selection passed to the normal 3MF
  project exporter.

The project snapshot is created when export or upload is scheduled. Failure to create or append the snapshot fails the requested export
rather than silently producing a G-code without the requested project.

Post-processing scripts and optional G-code line numbering run before the payload is appended. This prevents those transformations from
invalidating the embedded data.

## On-disk format

Version 1 uses a slicer-family-neutral, ASCII, line-oriented trailer. Every trailer line starts with `; ` and is therefore a G-code
comment. The `SLIC3R` namespace allows the format to be shared by Slic3r-derived applications without tying files to a specific fork.

```text
; SLIC3R_EMBEDDED_3MF_BEGIN 1
; SLIC3R_EMBEDDED_3MF_SIZE <decimal byte count>
; SLIC3R_EMBEDDED_3MF_SHA256 <lowercase hexadecimal SHA-256>
; SLIC3R_EMBEDDED_3MF_DATA
; <Base64 data, at most 76 characters per line>
; ...
; SLIC3R_EMBEDDED_3MF_END
```

The encoded bytes are the complete 3MF ZIP produced by Snapmaker Orca's normal project exporter. Because a 3MF is already compressed, no
additional compression is applied. A newline is inserted before the begin marker if the G-code does not already end with one.

Readers validate all of the following:

- recognized format version;
- complete and correctly ordered markers and metadata;
- valid Base64 with correct padding;
- decoded byte count equal to `SIZE`;
- decoded bytes' SHA-256 equal to `SHA256`.

Parsing and encoding are streaming operations so the G-code and project do not have to be loaded wholly into memory. If multiple begin
markers are present, the file is treated as corrupt rather than guessing which payload the user intended.

## Opening G-code

**File > Open G-code...** provides an explicit picker in the editor. All paths into `Plater::load_gcode` share the same behavior,
including that menu item, drag and drop, command-line/file association, recent-file handling, and reload:

1. A G-code without the begin marker opens directly in preview-only mode, exactly as before.
2. A valid embedded project produces a prompt with **Open Project** and **View G-code**.
3. A payload that can be decoded but fails validation produces a warning describing the integrity problem, with **Open Anyway** and
   **View G-code**.
4. A malformed payload that yields no project bytes produces a warning and opens as G-code only.

Choosing either project-opening action loads the extracted 3MF through the normal project loader. The recovered project is marked modified
and named `Untitled`, forcing an explicit Save As rather than exposing or later overwriting a temporary extraction path. Temporary snapshot
and extraction files are removed after use.

Choosing **View G-code** retains the existing preview-only behavior. If opening the extracted 3MF fails, the project loader reports its
normal error; the original G-code is not modified.

## Compatibility and security

- The original printable G-code portion is unchanged.
- Writers emit the `SLIC3R_EMBEDDED_3MF_*` namespace. Readers also accept the legacy `SNAPMAKER_ORCA_EMBEDDED_3MF_*` namespace emitted by
  early builds of this feature.
- Other slicers and firmware are not expected to understand the trailer, but can ignore it as comments.
- Enabling the preference intentionally increases file and upload sizes by roughly the Base64 expansion of the 3MF and includes editable
  geometry, settings, presets, and other project content normally saved in the selected 3MF scope.
- The checksum detects accidental corruption; it is not a signature and does not establish trust. Embedded projects receive the same
  validation and security treatment as ordinary local 3MF files.

## Tests

Unit tests cover canonical marker output, legacy marker compatibility, round trips, files lacking a final newline, absent and empty payloads,
malformed/truncated Base64, size mismatches, checksum mismatches, unsupported versions, duplicate markers, and multi-chunk payloads.
Integration-facing code keeps project serialization separate from the format codec so the codec tests remain deterministic and do not
require the GUI.
