# Thermal Surface Patterning

## Status

Implementation specification for a first-class Snapmaker Orca feature derived
from the behavior of `woodburn_orca.py`.

## Purpose

Thermal Surface Patterning creates repeatable decorative changes in color,
gloss, or texture by varying an enabled tool's nozzle-temperature target while
printing selected exposed surfaces. The feature is material-agnostic: it never
selects a filament by name or assumes that the material contains wood.

The implementation is native to Snapmaker Orca. It does not require an external
post-processing command or Python installation.

## User model

### Filament settings

Each filament/tool is independently eligible. A filament profile stores:

- `Enable thermal surface patterning` (off by default).
- The calibrated temperature increment per pattern level.
- The calibrated maximum patterning temperature.

Any number of tools may be enabled in one print. Tool selection is always
explicit; filament names are not inspected.

The configured patterning ceiling may exceed the filament profile's declared
maximum temperature. Slicing emits a prominent, deduplicated warning for an
affected filament/temperature pair, and changing the filament or ceiling causes
the revised warning to be evaluated again. The target may never exceed the
active printer/nozzle's hardware temperature limit.

### Process settings

The process profile contains a **Thermal Surface Patterning** group under
**Others**.

Normal controls:

- Application mode: `Disabled`, `All eligible surfaces`, or `Painted surfaces`.
- Preset: `Subtle`, `Natural`, or `Dramatic`.
- Seed.
- Apply to outer walls.
- Apply to top surfaces.

Advanced controls expose the behavior currently provided by
`woodburn_orca.py`: band dimensions and narrowing, transition weights,
darkness bias, grain trend, accent rings, top-surface grouping, heat/cool time
constants, tolerance, surface heat credit, base dwell, maximum preheat,
strict/thermal internal-temperature policy, risky-feature protection, and
outer-wall speed assistance.

Preset defaults are equivalent to the script's Subtle, Natural, and Dramatic
presets. Selecting a preset applies its advanced values, which remain
individually editable afterward.

### Object and regional control

Application mode and all process-owned options are normal region settings and
therefore support:

- Whole-object overrides.
- Part/volume overrides.
- Modifier meshes.
- Height-range modifiers.

A dedicated **Paint-on thermal patterning** gizmo provides brush, sphere,
triangle, smart-fill, clipping, erase, erase-all, and undo behavior consistent
with paint-on fuzzy skin.

Painting is binary. Painted facets mark where the generated thermal pattern is
eligible; they do not encode a requested temperature or shade. Thermal paint is
stored independently from fuzzy-skin paint, and the two features may overlap.
Paint is serialized in 3MF projects.

Painted segmentation includes outer walls and upward-facing top surfaces. It
does not make internal walls, sparse infill, support, or support interface
eligible.

### Pattern behavior

- Each object receives a deterministic random pattern derived from the process
  seed and a stable object identifier.
- Re-slicing an unchanged project produces the same pattern.
- Different objects receive different patterns even at the same Z height.
- Outer-wall levels are selected in vertical bands using the seeded Markov
  walk, slow trend, darkness bias, variable band widths, and optional accent
  rings from the script.
- Top surfaces use deterministic line groups so broad rasters can vary in shade
  without issuing a heater change for every extrusion line.
- Temperature level zero means the slicer's unmodified local nozzle target.
- Positive levels add `level * calibrated increment`, clamped to the filament
  patterning ceiling and printer/nozzle hardware limit.
- First-layer extrusion is always protected and remains at the slicer's normal
  target.
- Bridges, overhangs, and support interfaces are protected by default. Expert
  users may disable that protection.
- With strict internal-temperature policy, positive pattern targets are active
  only across eligible exposed extrusion. With thermal policy, a positive target
  may be carried for the configured bounded interval through otherwise safe
  non-exposed moves.

### Small painted regions and thermal lag

Nozzle temperature cannot change instantaneously. The slicer uses the same
first-order heating/cooling model as the script and displays predicted achieved
temperature in its generated metadata/preview.

Short wall fragments do not create independent random samples: they inherit the
object's current vertical band. If an outer-wall span cannot approach its target,
optional speed assistance may slow only that eligible outer-wall extrusion,
bounded by both the configured maximum factor and minimum speed. Short top lines
are merged into deterministic time/line-bounded groups rather than slowed line
by line.

The thermal transition may physically bleed beyond a painted boundary. The
preview represents the predicted temperature rather than implying an impossible
hard color boundary.

### G-code and preview

- Native generation emits non-blocking temperature commands for the addressed
  tool only.
- The active filament profile's normal nozzle temperature is the level-zero
  baseline; first-layer temperatures are never overridden.
- Commands are annotated with `THERMAL_PATTERN` comments containing tool,
  object, level, requested target, and predicted temperature.
- Feed-rate edits are annotated and restore the slicer's prior feed rate.
- Strict mode restores the normal target before ineligible extrusion. Thermal
  mode restores after its bounded carry interval. Tool changes and the end of
  the print always restore the affected tool.
- Temperature-colored G-code preview exposes the resulting pattern. A separate
  preview color scheme is not required for the initial implementation.

## Calibration

The Calibration menu contains **Thermal Surface Patterning**.

The calibration dialog:

1. Uses the active filament/extruder; its name is irrelevant.
2. Defaults the base temperature to that filament's normal nozzle temperature.
3. Accepts temperature increment, number of levels, maximum target, and band
   height, constrained by the printer/nozzle hardware limit.
4. Warns, but allows continuation, when the requested target exceeds the
   filament profile's declared maximum.
5. Creates a new project containing a bundled calibration model with a vertical
   outer-wall tower and horizontal top-surface swatches.
6. Applies deterministic levels `0..N`, labels every level in G-code metadata,
   and keeps the first layer and risky features at normal temperature.
7. After physical inspection, accepts the highest usable level and saves the
   increment and resulting ceiling to a newly named derived filament preset,
   with thermal patterning enabled.

Calibration uses the active tool and does not alter unrelated filament
presets.

## Compatibility and persistence

- Defaults are disabled, so existing profiles and projects slice identically.
- Unknown thermal-pattern settings are ignored by older builds according to
  existing profile compatibility behavior.
- New painted-facet data uses a dedicated 3MF attribute. Loading a project in a
  build without this feature may discard only the thermal paint, not geometry or
  other paint types.
- Thermal patterning and fuzzy skin may be active on the same facets.
- Multi-material and tool-changing prints protect inactive tools and schedule
  each enabled tool independently.

## Acceptance criteria

1. Enabling a non-wood-named filament produces patterned native G-code without
   a post-processing script.
2. Two enabled tools are independently patterned and no command targets the
   wrong tool.
3. Disabled defaults produce byte-equivalent extrusion and temperature behavior
   apart from unrelated existing nondeterminism.
4. Object, modifier, height-range, and binary paint selection restrict eligible
   outer walls and top surfaces as displayed in preview.
5. Thermal paint and fuzzy-skin paint survive a 3MF save/load round trip and may
   overlap.
6. A fixed seed is repeatable; distinct objects receive distinct patterns.
7. First layers remain unmodified. Protected bridges, overhangs, and support
   interfaces remain at normal target by default.
8. Targets may exceed the filament maximum only after the specified warning and
   never exceed hardware limits.
9. Strict mode restores the local slicer target across ineligible extrusion.
10. Speed assistance respects both bounds and restores feed rate afterward.
11. Calibration builds both wall and top-surface specimens, addresses the
    active tool, and can save a derived filament preset.
12. Unit tests cover deterministic band generation, target clamping, speed
    bounds, and both native and project-3MF paint serialization. Slicer-level
    regression checks cover protected roles, multi-tool state, and painted
    segmentation.
