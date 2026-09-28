# Fuzzy Skin

Fuzzy skin randomly perturbs the wall path to produce a deliberately rough, matte appearance on the model surface.  
These settings control where the effect is applied, how the noise is generated, and how aggressive the displacement or extrusion modulation is.

Useful for creating a textures or hide surface imperfections but will increase print time and will affect dimensional accuracy.

- [Fuzzy Skin Mode](#fuzzy-skin-mode)
  - [Contour](#contour)
  - [Contour and Hole](#contour-and-hole)
  - [All Walls](#all-walls)
  - [Fuzzy Skin Generator Mode](#fuzzy-skin-generator-mode)
  - [Displacement](#displacement)
  - [Extrusion](#extrusion)
  - [Combined](#combined)
- [Noise Type](#noise-type)
  - [Classic](#classic)
  - [Perlin](#perlin)
  - [Billow](#billow)
  - [Ridged Multifractal](#ridged-multifractal)
  - [Voronoi](#voronoi)
- [Point distance](#point-distance)
- [Skin thickness](#skin-thickness)
- [Skin feature size](#skin-feature-size)
- [Skin Noise Octaves](#skin-noise-octaves)
- [Skin Noise Persistence](#skin-noise-persistence)
- [Apply fuzzy skin to first layer](#apply-fuzzy-skin-to-first-layer)
- [Horizontal fuzzy surfaces](#horizontal-fuzzy-surfaces)
- [Fuzzy ironing](#fuzzy-ironing)
- [Calibration](#calibration)
- [Credits](#credits)

## Fuzzy Skin Mode

Choose which parts of the model receive the fuzzy-skin effect.

### Contour

Apply fuzzy skin only to the outermost contour (external perimeter) of the model.  
Useful for creating a textured edge while keeping the inner surfaces smooth.

### Contour and Hole

Apply fuzzy skin to both the outer contour and interior holes. Useful when you want the rough texture to appear on negative features as well.

### All Walls

Apply fuzzy skin to every wall (external and internal). This gives the strongest overall textured appearance but will increase slicing and print time considerably.

### Fuzzy Skin Generator Mode

Select the underlying method used to produce the fuzzy effect. Each mode has different trade-offs for strength, speed and mechanical load.

### Displacement

![Fuzzy-skin-Displacement-mode](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-Displacement-mode.png?raw=true)

The classic method is when the pattern on the walls is achieved by shifting the printhead perpendicular to the wall.  
It gives a predictable result, but decreases the strength entire shells and open the pores inside the walls. It also increases the mechanical stress on the kinematics of the printer. The speed of general printing is slowing down.

### Extrusion

![Fuzzy-skin-Extrusion-mode](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-Extrusion-mode.png?raw=true)

The fuzzy skin condition is obtained by changing the amount of extruded plastic as the print head moves linearly. There is no extra load on the kinematics, there is no decrease in the printing speed, the pores do not open, but the drawing turns out to be smoother by a factor of 2. It is suitable for creating "loose" walls to reduce internal stress into extruded plastic, or masking printing defects on the side walls - a matte effect.

> [!CAUTION]
> The "Fuzzy skin thicknesses" parameter cannot be more than about 70%-125% (selected individually for different conditions) of the nozzle diameter! This is a complex condition that also depends on the height of the layer, and determines how thin the lines can be extruded.  
> [Arachne](quality_settings_wall_generator#arachne) wall generator mode should also be enabled.

### Combined

![Fuzzy-skin-Combined-mode](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-Combined-mode.png?raw=true)

This is a combination of Displacement and Extrusion modes. The clarity of the drawing is the same in the classic mode, but the walls remain strong and tight. The load on the kinematics is 2 times lower. The printing speed is faster than in Displacement mode, but the elapsed time will still be longer.

> [!WARNING]
> The limits on line thickness are the same as in the Extrusion mode.

## Noise Type

Select the noise algorithm used to generate the random offsets. Different noise types produce distinct visual textures.

### Classic

Simple uniform random noise. Produces a coarse, irregular texture.

![Fuzzy-skin-classic](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-classic.png?raw=true)

### Perlin

[Perlin noise](https://en.wikipedia.org/wiki/Perlin_noise) generates smooth, natural-looking variations with coherent structure.

![Fuzzy-skin-perlin](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-perlin.png?raw=true)

### Billow

Billow noise is similar to Perlin noise, but has a clumpier appearance. It can create more pronounced features and is often used for natural textures.

![Fuzzy-skin-billow](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-billow.png?raw=true)

### Ridged Multifractal

Creates sharp, jagged features and high-contrast detail. Useful for stone- or marble-like textures.

![Fuzzy-skin-ridged-multifractal](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-ridged-multifractal.png?raw=true)

### Voronoi

[Voronoi noise](https://en.wikipedia.org/wiki/Worley_noise) divides the surface into Voronoi cells and displaces each cell independently, creating a patchwork or cellular texture.

![Fuzzy-skin-voronoi](https://github.com/SoftFever/OrcaSlicer/blob/main/doc/images/Fuzzy-skin/Fuzzy-skin-voronoi.png?raw=true)

## Point distance

Average distance between random sample points along each line segment.  
Smaller values add more detail and increase computation; larger values produce coarser, faster results.

## Skin thickness

Maximum lateral width (in mm) over which points can be displaced. This defines how far the wall can be jittered.  
Keep this below or near your outer wall line width and within nozzle/flow limits for reliable prints.

## Skin feature size

Base size of coherent noise features, in mm. Larger values yield bigger, more prominent structures; smaller values give fine-grained texture.

## Skin Noise Octaves

The number of octaves of coherent noise to use. Higher values increase the detail of the noise, but also increase computation time.

## Skin Noise Persistence

Controls how amplitude decays across octaves. Lower persistence results in smoother noise; higher persistence keeps finer-scale detail stronger.

## Apply fuzzy skin to first layer

Enable to apply fuzzy skin to the first layer.

> [!CAUTION]
> Can impact bed adhesion and surface contact.

## Horizontal fuzzy surfaces

**Fuzzy skin on top surfaces** converts top-solid infill into coordinated non-planar XYZ moves. It reuses the normal skin thickness, point distance, and noise controls. **Apply top-surface fuzzy skin to first layer** is intentionally separate from the wall-first-layer option and still applies only to top-solid paths.

**Fuzzy skin on bed-facing surfaces** is a separate opt-in for first-layer bottom-surface paths. It uses positive Z displacement to vary the material's squish against the build plate, never commands the nozzle below the nominal first-layer height, and is capped at 25% of the initial layer height to preserve clearance for the next layer.

**Fuzzy skin on supported lower surfaces** applies downward texture only to external bridge infill when support is enabled. The displacement is limited by the configured support top-Z gap, while **Minimum support-interface distance** reserves clearance above the support. Ordinary bottom surfaces, internal bridges, unsupported bridges, and overhang walls are unchanged.

**Connect fuzzy surface boundaries** returns the ends of each affected path to its nominal layer height, reducing steps where horizontal texture meets walls. **Compensate fuzzy-surface extrusion** accounts for the extra length of the three-dimensional path. Lower surfaces can use a stronger compensation exponent because bridge extrusion behaves differently from supported top infill.

Start conservatively: 0.1 mm thickness and 0.4 mm point distance are suitable first trials for a typical 0.4 mm nozzle. Inspect generated XYZ moves in Preview and avoid amplitudes that approach the nozzle clearance or support gap.

## Fuzzy ironing

**Iron fuzzy surfaces** is experimental. When enabled together with ordinary top-surface ironing, the ironing toolpath follows the same deterministic height field as the underlying fuzzy skin. It polishes the texture instead of flattening it. When disabled, normal ironing is skipped over fuzzy top regions to prevent collisions with peaks.

The nozzle footprint can still contact nearby peaks even when its center follows the field. Use small thickness, conservative ironing flow, and generous point distance for initial tests.

## Calibration

Open **Calibration → Fuzzy skin** to generate one of three labeled artifacts:

- **Texture matrix** varies thickness by column and point distance by row.
- **Ironing comparison** produces matched raw and fuzzy-ironed samples for every matrix cell.
- **Supported underside** creates supported bridge canopies for inspecting downward texture and is available as breakaway coupons.

Connected panels use smooth row and column headers. Breakaway coupons carry their own physical `T`, `D`, and, where relevant, `I` label. The generator checks the active build-plate size before replacing the current project, and all tested values remain object or volume overrides rather than modifying the selected preset.

Every matrix and ironing card includes a circular parameter-modifier region on its bed-facing side using the same thickness and distance as that card. Supported-underside cards place the circle over a bed-contacting leg. Supported cards are disconnected solids in one model object so automatic support is generated as one field without inter-object G-code path-conflict warnings.

## Credits

- **Generator Mode author:** [@pi-squared-studio](https://github.com/pi-squared-studio).

## Four-treatment cube calibration

Calibration → Fuzzy skin now offers a single four-treatment cube, a series, or a
nine-cube L9 array. Each cube has four complete outer walls and four triangular
quarters of its top: plain, fuzzy, ironed, and fuzzy + ironed. Physical wall labels
show `F=0/1`, `I=0/1`, thickness `T`, and point distance `D`; labels themselves stay
smooth. Ironed sections reuse the transparent calibration's all-solid-layer
ironing process, with a small inset to reach the wall edge on every layer. This
polishes the horizontal layer edges making up a wall; it does not drive the
nozzle sideways against the finished vertical face.

A single cube uses the two minimum values. A series uses all combinations in the
entered thickness and distance ranges. The L9 array uses minimum, midpoint, and
maximum for each factor, giving all nine balanced pairs of these two factors.
Step inputs do not apply to the single cube or L9 modes. Cubes print by layer,
with 100% infill so every internal layer is eligible for ironing. Buried ironing
stays planar; only ironing on an exposed fuzzy top follows the height field.

## Painting and fill patterns

Fuzzy paint on a top face enables top-surface fuzz in the painted region.
Ironing paint can overlap it to make the fuzzy + ironed treatment. Plain,
fuzzy-only, ironing-only, and overlapping areas retain their separate settings.

Horizontal fuzzy skin adds Z motion to the existing fill path, preserving its XY
pattern. Selecting concentric top or bottom fill therefore gives concentric
textured paths on eligible surfaces. This is intentional: the selected pattern
still controls line direction, coverage, and continuity. It does not emboss an
independent concentric height map. Ironing uses its own ironing-pattern setting;
supported undersides use the bridge paths generated for those surfaces.

## Bed and supported-surface bonding

Bed-facing and supported-lower fuzzy paths now return to nominal layer height at
regular distances, including their endpoints. Anchor spacing is four times the
fuzzy point distance, clamped to 0.4–2 mm, measured along the path even around
corners. On the bed these are normal first-layer squish/contact points. On a
supported underside they provide regularly spaced bonding points for the next
planar layer. These anchors remain enabled even if general boundary connection
is disabled.

Displacement on these surfaces is additionally capped to 25% of the current
extrusion height and, when present, the next layer's height. Bed texture therefore
stays below the next layer, including variable-height layers. Supported-lower
texture stays shallow enough to limit the gap to the following planar layer.
The existing minimum clearance from support remains enforced; the nozzle never
intentionally drives into the support interface. This is an anchoring and bounded
gap strategy, not a contour-following reconstruction of the next layer. Physical
print validation is still needed to choose settings for a particular material,
nozzle, and support interface.

Enable **Raise card labels on smooth pedestals** to lift coupon labels above the fuzzy texture on a continuous, smooth pad. The pad clears the selected texture amplitude by 0.4 mm and inherits the label’s disabled fuzzy skin and ironing settings. This option applies to cards and panel headers; cube wall labels remain directly embossed.
