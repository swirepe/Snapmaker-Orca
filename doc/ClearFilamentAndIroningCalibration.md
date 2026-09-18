# Clear Filament and Ironing Calibration

## Purpose

Snapmaker Orca shall provide two related calibration generators:

1. **Clear filament calibration** screens process variables that affect optical clarity.
2. **Ironing calibration** screens ironing behavior independently while using the same pane, layout, label, and experiment-design machinery.

Both generators create an editable project containing individually configured rectangular panes. The generated project remains a normal project: panes may be moved, settings may be inspected or changed, and the project may be saved before slicing.

## Entry points and project behavior

- Add **Clear filament** and **Ironing** entries to the Calibration menu on every platform.
- Opening either entry displays a modal calibration dialog.
- Generating a test uses the existing new-project confirmation and replaces the current project only after confirmation succeeds.
- The dialog remembers its last-used values. Invalid or unsupported combinations disable generation and display an explanatory error.
- Numeric fields use the application's normal locale-aware validation.

## Experiment designs

The dialogs share three designs:

- **Linear series:** exactly one enabled factor. The factor may use 2–10 evenly spaced levels.
- **2D grid:** exactly two enabled factors. Each factor may use 2–10 levels; every pair is generated.
- **Taguchi orthogonal array:** two or more enabled factors use one common level count of 2, 3, or 4. The smallest supported array with enough columns is selected:
  - two levels: L4, L8, or L16;
  - three levels: L9, L18, or L27;
  - four levels: L16.

The dialog previews the selected array and final pane count. It rejects a design that exceeds the available array columns or bed capacity.

Numeric levels include both endpoints and are linearly interpolated. Categorical levels are defined by the factor. Pane order follows experiment row order and is preserved in the object list and G-code.

## Factors

### Clear filament calibration

The default design is a four-factor, four-level L16 array:

| Factor | Default levels |
| --- | --- |
| Nozzle temperature | 260, 265, 270, 275 °C |
| Print speed | 25, 33.3, 41.7, 50 mm/s |
| Flow ratio | 0.96, 1.00, 1.04, 1.08 |
| Layer height | 0.10, 0.20, 0.30, 0.40 mm |

Additional factors are available but initially disabled:

- maximum/general part-cooling fan;
- wall fan override, covering inner and outer walls;
- ironing fan;
- auxiliary fan when supported by the selected printer;
- ironing mode;
- ironing flow;
- ironing direction;
- line width;
- ironing speed;
- ironing spacing.

Print speed applies to outer walls, inner walls, solid infill, and top/bottom surfaces. It does not replace first-layer or ironing speed. Flow rate means the existing per-object flow ratio, not maximum volumetric flow.

### Ironing calibration

The default design is a four-factor, four-level L16 array:

| Factor | Default range/levels |
| --- | --- |
| Ironing mode | None; all top surfaces; every other layer; every layer |
| Ironing flow | 5–20% |
| Ironing speed | 10–30 mm/s |
| Ironing spacing | 0.05–0.20 mm |

Ironing direction and ironing fan are available as additional initially-disabled factors. Direction levels are relative to the aligned-rectilinear fill direction:

- 2 levels: parallel, perpendicular;
- 3 levels: parallel, 45°, perpendicular;
- 4 levels: parallel, 45°, perpendicular, 135°.

For fewer than four Taguchi levels, ironing-mode levels are:

- 2 levels: none, every layer;
- 3 levels: none, all top surfaces, every layer;
- 4 levels: none, all top surfaces, every other layer, every layer.

Normal process settings retain the existing topmost-only ironing option, but the calibration factor does not use it.

## Pane geometry and layout

- Default clear-filament pane: 30 × 30 × 1 mm.
- Default ironing pane: 30 × 30 × 2 mm.
- Width, depth, height, and gap are configurable; the default gap is 5 mm.
- Panes are laid out in a centered, row-major grid on the active build plate while avoiding excluded areas.
- The generated object list uses the same row-major order.
- Each pane uses one wall, 100% aligned-rectilinear sparse infill, aligned-rectilinear internal solid infill, and aligned-rectilinear top and bottom surfaces.
- Pane geometry uses the selected pane extruder.
- The requested physical pane height is constant. If it is not divisible by a selected layer height, precise-Z slicing uses a shorter final layer.
- Generation fails with a clear message when the panes cannot fit the active plate.

### Mouse ears

- Mouse ears are optional and disabled by default.
- When enabled, four 5 mm diameter, one-first-layer-thick discs overlap the pane corners.
- Mouse ears are part of their pane, use the pane extruder, and do not change the measured pane area above the first layer.

## Labels

- Labels are optional and disabled by default.
- Labels are real editable model geometry and are visible in Prepare view.
- The default monospaced glyph height is 2 mm; the default relief protrudes 2 mm above the pane.
- Labels contain only factors varied by the experiment, use stable abbreviated keys, and wrap over multiple lines.
- The generator validates that the label fits the pane and reports when a larger pane or smaller glyph size is required.
- Pane and label extruders are independently selectable.
- With the same extruder, each pane and its label print together. There is one temperature stabilization wait before the pane; the label inherits that pane's temperature and fan state but uses normal text speed, line width, and flow.
- With different extruders, all pane bodies print in experiment order, the printer changes tools once, and all labels are printed in a layer-wise second phase. The generated calibration job disables the prime tower so that it cannot break this ordering.

## Sequential printing and temperature transitions

- Pane bodies are printed completely and sequentially in experiment order.
- Before a sequential object or independently configured model part begins, an effective temperature override emits a waiting temperature command (`M109` or flavor-equivalent).
- When entering an override region created by a modifier or layer range, temperature changes are nonblocking (`M104` or flavor-equivalent).
- The UI warns that small or interleaved regions may finish before a requested temperature is physically reached.
- Temperature overrides use the filament temperature when unset and never rewrite the underlying filament preset.

## General process settings

The following capabilities are normal process settings, not calibration-only metadata:

- nozzle-temperature override;
- general part-cooling fan override;
- inner/outer wall fan override;
- ironing fan override;
- auxiliary-fan override where supported;
- ironing every other supported solid layer;
- ironing every supported solid layer.

They support global process configuration and, where spatially meaningful, object, part, modifier, and layer-range overrides. Temperature and fan controls are not paintable.

Ironing enablement is paintable in the same manner as fuzzy skin. Painting selects where an enabled ironing mode may operate; flow, spacing, speed, fan, and direction are inherited from the owning object/region.

## Ironing semantics

- **All top surfaces:** preserve current behavior for upward-facing top surfaces.
- **Every other layer:** iron supported solid material on object layers 2, 4, 6, and so on. Always iron the final upward-facing top surface, even when it has odd parity.
- **Every layer:** iron the supported solid footprint of each object layer.
- With 100% infill, every/alternate-layer modes may sweep the complete supported cross-section, including perimeters and solid infill.
- With non-solid infill, ironing is restricted to supported solid regions and must not sweep unsupported sparse gaps, bridges, internal voids, or support material.
- Painted ironing further intersects the calculated ironing area.

## Fan semantics

- The general fan override controls normal infill and surface extrusion.
- The wall fan override applies to inner and outer walls.
- The ironing fan override applies during ironing.
- The auxiliary fan override is available only for printers that expose an auxiliary fan.
- Bridge, overhang, and support-interface fan speeds are not calibration factors because the generated panes contain none of those features.

## Validation and safety

- Temperatures outside the active filament's declared range require explicit acknowledgement in the dialog.
- Layer height and line width are checked against the active nozzle and normal slicer limits.
- Fan and percentage values are clamped by validation to their documented ranges, never silently at G-code generation.
- Sequential clearance uses the full pane-plus-label height.
- A generated job must not mutate saved process, filament, or printer presets; changes are project/object overrides.
- The G-code preview must show pane order, tool changes, temperature commands, fan changes, ironing paths, mouse ears, and labels.

## Tests

Automated coverage shall include:

- evenly spaced numeric levels;
- supported orthogonal-array dimensions and balance;
- factor-to-pane assignment and stable row order;
- categorical level mapping for ironing mode and direction;
- every-layer and alternate-layer selection, including mandatory final-top ironing;
- exclusion of sparse/bridge/void areas from repeated ironing;
- configuration serialization and backward compatibility;
- label formatting and fit validation;
- same-extruder and two-phase different-extruder scheduling;
- per-region temperature/fan marker generation.

Manual validation shall cover both menu entries, dialog persistence, generation on rectangular and non-rectangular beds, Prepare-view label geometry, G-code preview, and a dual-extruder two-phase label print.
