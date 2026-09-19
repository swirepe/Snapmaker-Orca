# Native Fuzzy Skin for Horizontal Surfaces

## Status

Implementation specification for `codex/topfuzz`.

## Summary

Extend fuzzy skin from two-dimensional wall-path displacement to native, non-planar fuzzy top surfaces and supported lower surfaces. The implementation borrows the useful ideas from Fuzzyficator—constant-length path subdivision, Z displacement, boundary reconnection, and extrusion-length compensation—but operates inside the slicer rather than rewriting finished G-code.

The feature must work with Classic and Arachne walls, object and volume overrides, and the existing paint-on fuzzy-skin workflow. It must also offer an opt-in ironing pass that follows the same fuzzy height field and a calibration generator for selecting practical settings.

## Goals

- Add positive Z texture to top-surface extrusion paths.
- Add safely bounded negative Z texture to supported external bottom-bridge paths.
- Reuse the existing fuzzy-skin thickness, point distance, noise type, feature size, octaves, and persistence.
- Reuse fuzzy-skin painting and normal region/volume configuration overrides.
- Keep all new printing behavior disabled by default for profile compatibility.
- Optionally compensate extrusion for the longer three-dimensional toolpath.
- Optionally iron a fuzzy top surface by following the same deterministic height field.
- Provide connected and breakaway calibration artifacts with physically printed labels.

## Non-goals

- Importing image displacement maps.
- Changing the existing XY wall-displacement algorithm.
- Applying Z fuzz to internal bridge infill, unsupported bridges, overhang perimeters, sparse infill, solid internal infill, or ordinary bottom surfaces.
- Guaranteeing that extreme settings are mechanically safe for every nozzle geometry. The UI and documentation will identify fuzzy ironing as experimental.
- Adding a dedicated fuzzy-surface speed. Existing top-surface, bridge, and ironing speeds remain authoritative.

## Terminology

- **Wall fuzz**: the existing XY displacement or extrusion-width variation on perimeter paths.
- **Horizontal fuzz**: the new Z displacement on top or supported lower-surface paths.
- **Nominal Z**: the ordinary Z coordinate of the current layer/path.
- **Height field**: a deterministic function of XY position, layer, and fuzzy-noise settings used to calculate normalized displacement.
- **Supported lower surface**: an external bottom bridge (`erBridgeInfill`) printed while object support generation is enabled and bridge support is not disabled.

## Configuration

All new options are process/region options so object, volume, height-range, and fuzzy-painted regions inherit or override them consistently.

| Key | UI label | Type / range | Default | Behavior |
| --- | --- | --- | --- | --- |
| `fuzzy_skin_top_surface` | Fuzzy skin on top surfaces | Boolean | Off | Applies positive Z fuzz to `erTopSolidInfill`. |
| `fuzzy_skin_lower_surface` | Fuzzy skin on supported lower surfaces | Boolean | Off | Applies negative Z fuzz to eligible external bottom bridges. |
| `fuzzy_skin_top_surface_first_layer` | Apply top-surface fuzzy skin to first layer | Boolean | Off | Separately allows top-surface Z fuzz on layer zero. It does not alter the existing wall-first-layer option. |
| `fuzzy_skin_bed_surface` | Fuzzy skin on bed-facing surfaces | Boolean | Off | Applies positive-Z texture to `erBottomSurface` on the first layer, capped at 25% of initial layer height. |
| `fuzzy_skin_connect_walls` | Connect fuzzy surface boundaries | Boolean | On | Keeps the first and last point of each affected extrusion path at nominal Z. |
| `fuzzy_skin_compensate_extrusion` | Compensate fuzzy-surface extrusion | Boolean | On | Scales segment extrusion for three-dimensional path length. |
| `fuzzy_skin_bridge_compensation_multiplier` | Lower-surface extrusion compensation | Float, 0–10 | 3.0 | Exponent applied to the geometric compensation ratio for supported lower surfaces, matching Fuzzyficator's bridge treatment. |
| `fuzzy_skin_min_support_distance` | Minimum support-interface distance | Float, 0–5 mm | 0.10 mm | Clearance that must remain between downward fuzz and the configured top support interface. |
| `fuzzy_skin_ironing` | Iron fuzzy surfaces | Boolean | Off | Makes eligible ironing paths follow the same top-surface height field. Experimental. |

Existing settings retain these meanings:

- `fuzzy_skin` enables fuzzy behavior for the region. Any non-`None` value makes horizontal fuzz eligible; its contour/hole/all-wall distinction continues to affect wall fuzz only.
- `fuzzy_skin_thickness` is the maximum absolute horizontal displacement.
- `fuzzy_skin_point_distance` is the target maximum XY distance between generated fuzzy samples.
- `fuzzy_skin_noise_type`, `fuzzy_skin_scale`, `fuzzy_skin_octaves`, and `fuzzy_skin_persistence` define the height field.
- `fuzzy_skin_first_layer` continues to control wall fuzz only. It is independent of both top-first-layer and bed-facing texture.
- `fuzzy_skin_mode` continues to control wall generation. Horizontal fuzz always uses Z displacement when enabled, including when wall mode is Extrusion or Combined.

Profiles without the new keys load the defaults and produce the same G-code they did before this feature.

## UI behavior

The new controls appear in the existing **Others → Fuzzy Skin** group after the current wall/noise controls.
The same complete group is available in the object and part quick-settings menu for per-object, modifier, and painted-region workflows.

- Horizontal controls are enabled only when fuzzy skin is not `None`.
- First-layer and fuzzy-ironing controls are enabled only when top-surface fuzz is on.
- Minimum support distance is enabled only when lower-surface fuzz is on.
- Lower-surface compensation is enabled only when lower-surface fuzz and extrusion compensation are both on.
- Fuzzy ironing includes an experimental warning explaining that large displacement or tight spacing may let the nozzle scrape adjacent peaks.

## Eligibility

### Top surfaces

A path is fuzzed when all of the following are true:

1. The active region has fuzzy skin enabled.
2. `fuzzy_skin_top_surface` is on.
3. The path role is `erTopSolidInfill`.
4. The layer is not layer zero, unless `fuzzy_skin_top_surface_first_layer` is on.

### Bed-facing surfaces

A path receives positive-Z bed texture only when it is `erBottomSurface` on the first layer and `fuzzy_skin_bed_surface` is on. This is a separate opt-in from top-surface and wall first-layer controls. Positive displacement changes first-layer squish while guaranteeing that generated moves never go below the nominal first-layer height. Its displacement is capped at 25% of the initial layer height so a large general fuzzy-thickness value cannot create peaks near the following layer.

This applies to both Classic- and Arachne-generated objects because the decision is based on the final extrusion role and active region configuration.

### Supported lower surfaces

A path is fuzzed when all of the following are true:

1. The active region has fuzzy skin enabled.
2. `fuzzy_skin_lower_surface` is on.
3. The path role is external `erBridgeInfill`, not `erInternalBridgeInfill` or `erOverhangPerimeter`.
4. Object support generation is enabled.
5. The profile does not disable support beneath bridges.
6. The configured top support Z gap is greater than `fuzzy_skin_min_support_distance`.

As in Fuzzyficator, the whole eligible bridge feature is processed rather than clipping individual path fragments against support-interface lines.

The maximum downward amplitude is:

```text
min(fuzzy_skin_thickness,
    max(0, support_top_z_distance - fuzzy_skin_min_support_distance))
```

If that amplitude is zero, the path is emitted unchanged.

## Height field and path generation

Horizontal fuzz is produced during G-code path emission, while extrusion role, region configuration, layer, and nominal Z are all known.

1. Split every original XY segment into equal subsegments whose XY length is no greater than `fuzzy_skin_point_distance`.
2. Evaluate a normalized deterministic height field at every generated XY point.
3. Map the normalized value into the applicable displacement range.
4. Emit coordinated XYZ extrusion moves and bypass arc fitting for the affected path.
5. Restore nominal Z before a subsequent non-fuzzy extrusion or layer transition.

Top-surface displacement is in `[0, fuzzy_skin_thickness]`. Supported-lower displacement is in `[-bounded_amplitude, 0]`.

When boundary connection is enabled, the first and last point of each extrusion path are forced to nominal Z. Intermediate points still sample the field. When it is disabled, endpoints sample the field normally, but nominal Z restoration remains mandatory when leaving the fuzzy path.

The field must be stable for a given model position, layer, and configuration. Coherent noise types use the existing libnoise modules and parameters. Classic noise uses a deterministic XY lattice/hash field rather than the current thread-local random wall generator so a later ironing path can reproduce the same surface. The field has no user-visible seed in this version.

Object copies may share the same local texture; repeatability and top/ironing agreement take precedence over randomizing each copy.

## Extrusion compensation

For a generated segment:

```text
geometric_ratio = length_xyz / length_xy
```

- With compensation off, retain the original extrusion per XY millimeter.
- On top surfaces and fuzzy ironing, multiply segment extrusion by `geometric_ratio`.
- On supported lower surfaces, multiply segment extrusion by `pow(geometric_ratio, fuzzy_skin_bridge_compensation_multiplier)`.
- Preserve the original path's flow, role, speed selection, acceleration behavior, and force-no-extrusion flag.
- Degenerate or zero-length XY segments are ignored without division.

## Fuzzy ironing

Fuzzy ironing is experimental and is meaningful only when top-surface fuzz and ordinary ironing are enabled for the region.

- If top-surface fuzz is on and `fuzzy_skin_ironing` is off, conventional ironing paths over that fuzzy region are skipped so they cannot flatten or collide with the texture.
- If `fuzzy_skin_ironing` is on, `erIroning` paths sample the exact same deterministic top-surface height field at their own XY coordinates.
- Ironing retains its configured pattern, spacing, speed, inset, flow, fan, and extrusion role.
- Boundary connection and extrusion compensation apply to fuzzy ironing just as they do to the underlying top surface.
- Ironing outside fuzzy-enabled regions remains unchanged.

The result is a polished textured surface, not a flattened surface. Documentation must caution that the nozzle footprint can still touch a nearby peak even when the nozzle center follows the field.

## Paint-on behavior

The existing fuzzy-skin painter remains the only painting UI. Painted facets already create a derived print region with fuzzy skin enabled and the parent region's remaining settings. The new region options therefore apply automatically:

- Painted top facets can generate fuzzy top infill.
- Painted supported underside facets can generate fuzzy bridge infill.
- Unpainted derived regions remain unchanged.
- No new painter state or mesh annotation is introduced.

## Preview and G-code

- Coordinated XYZ extrusion moves must be visible to the existing G-code processor/preview as non-planar segments.
- Existing role, width, height, and volumetric metadata tags remain present.
- Affected paths do not use G2/G3 arc output.
- Both relative and absolute extrusion modes are supported because compensation is applied before passing extrusion to `GCodeWriter`.
- Travel/retraction logic remains owned by the normal G-code generator.

## Calibration tool

Add **Calibration → Fuzzy skin** to both menu variants. Opening it shows a dialog that generates a new calibration project using the current printer, process, and filament presets.

### Inputs

- Test mode: Texture matrix, Ironing comparison, or Supported underside.
- Layout: Connected panel or Breakaway coupons where applicable.
- Thickness start/end/step, default `0.05 / 0.50 / 0.15 mm`.
- Point-distance start/end/step, default `0.20 / 2.00 / 0.60 mm`.

Inputs must be finite and positive, starts must not exceed ends, and steps must produce at least one value. Each axis is limited to eight values and the total matrix to 64 parameter combinations. Before replacing the current project, the dialog estimates the artifact footprint against the active printable area and asks the user to reduce the range if it cannot fit.

### Texture matrix

- Columns vary thickness and rows vary point distance.
- Each cell is a raised coupon with a vertical wall and top face so both wall and top texture can be inspected.
- Connected layout joins all cells to a thin, smooth base.
- The connected base has smooth embossed column headers for thickness and row headers for point distance.
- Breakaway layout creates individually removable coupons with abbreviated smooth embossed labels such as `T=.20 D=.40`.
- Calibration geometry uses per-volume/process overrides so every cell receives its requested thickness and distance while base, labels, and tabs have fuzzy skin disabled.
- A circular first-layer parameter modifier under every cell enables only bed-facing fuzz, allowing the printed underside to be compared without adding coplanar model geometry. The matrix thickness values are mapped proportionally into the 25%-of-first-layer safety cap, so every circle remains distinct without saturating at the cap.

### Ironing comparison

- Generates paired samples for every parameter combination.
- One sample uses raw top fuzz and the other enables fuzzy ironing.
- Smooth embossed `RAW` and `IRON` identifiers accompany the numeric row/column labels.
- Other geometry and process settings match within each pair.

### Supported underside

- Generates labeled breakaway bridge-canopy samples with support enabled and ordinary external bridge roles. Connected layout is unavailable because a shared base would obstruct the tested underside.
- The disconnected canopies are volumes of one model object, so the slicer produces one coherent support field instead of reporting conflicts between independently generated support paths.
- Each canopy receives its own thickness/point-distance override and enables supported-lower fuzz.
- Labels are placed on smooth, upward-facing frame surfaces, never on the fuzzy underside.

### Calibration project behavior

- The generator creates a new named project after the standard unsaved-project confirmation.
- Generated objects are centered and arranged within the active bed.
- It does not permanently modify the selected presets; calibration values are object/volume overrides in the generated project.
- Object/volume names also contain the numeric values for accessibility and slicer inspection even though physical labels are required.

## Slicing invalidation

Changes to horizontal-fuzz settings must invalidate G-code generation. Geometry need not be resliced because subdivision and Z emission occur after extrusion paths are produced. Calibration geometry changes use the normal model-change invalidation flow.

## Documentation

Update the fuzzy-skin settings guide with:

- Top and supported-lower examples and safety limits.
- The independence of wall-first-layer and top-first-layer settings.
- Extrusion compensation behavior.
- Fuzzy-ironing warning and recommended conservative starting values.
- Calibration modes and how to read their labels.
- The fact that lower-surface fuzz requires support and available support-interface clearance.

New English UI strings are added to the localization catalog/source in the repository's normal format; existing translations may remain untranslated for this feature branch.

## Verification and acceptance criteria

### Automated tests

- Height-field results are deterministic and stay within `[0, 1]` for every noise type.
- Top and ironing evaluation return the same height at the same XY/layer/configuration.
- Segment subdivision never exceeds the configured point distance, preserves endpoints, and ignores degenerate segments.
- Boundary connection forces only path endpoints to nominal Z.
- Top and supported-lower displacement stay inside their calculated ranges.
- Support-gap clearance correctly disables or clamps lower displacement.
- Geometric and bridge-multiplied extrusion compensation produce expected values.
- Eligibility covers top role, external bridge role, internal bridge exclusion, support-disabled exclusion, and independent first-layer controls.
- Conventional ironing is skipped only for fuzzy top regions; fuzzy ironing follows the field.
- New options serialize, deserialize, and default to backward-compatible values.
- Calibration ranges generate the expected matrix dimensions, labels, overrides, and footprint rejection.

### Integration checks

- Slice representative Classic and Arachne models with top fuzz and inspect XYZ G-code and preview.
- Slice paint-on top and underside regions and verify only painted areas receive Z motion.
- Slice supported and unsupported bridge cases and confirm only the supported configuration is eligible.
- Compare raw and fuzzy-ironed calibration samples in preview.
- Confirm projects/presets created before this feature produce unchanged G-code when all new toggles are off.
- Build the application and relevant tests on macOS Release configuration.

## Delivery

1. Implement and test on `codex/topfuzz`, based on `main`.
2. Commit the specification and implementation on that branch.
3. Merge `codex/topfuzz` into the clean `codex/dev` branch without rewriting unrelated development history.
4. Start the macOS Release build from the `codex/dev` worktree and report its process/log location and immediate startup status.
