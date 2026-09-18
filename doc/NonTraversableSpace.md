# Non-traversable space specification

## Purpose

A non-traversable space is a model volume that forbids selected extruders from
moving through its three-dimensional interior. Its primary use is protecting
openings where a travel move would leave damaging stringing, such as the bore of
a TPU tube.

## User experience

- `Non-traversable space` is a first-class model-volume type alongside parts,
  negative parts, modifiers, support blockers, and support enforcers.
- The object context menu can add a primitive or imported mesh directly as a
  non-traversable space. An existing volume can be changed to this type.
- The volume is shown as a distinct translucent keep-out volume and is not
  printed, subtracted from the model, or used to generate support.
- The volume follows its parent object and is duplicated and transformed with
  every instance of that object.
- Each volume has a `Blocked extruders` control. Its default is `All
  extruders`. The user may instead select an explicit non-empty subset of the
  printer's extruders.
- `All extruders` is a live rule: it also includes extruders added after the
  volume was created. An explicit subset does not automatically include newly
  added extruders.
- The volume type and blocked-extruder rule are preserved in project 3MF files.

## Routing behavior

- The restriction applies to slicer-planned, non-extruding XY motion, including
  moves to and from a purge tower. Arbitrary motion emitted by user-supplied
  custom G-code is outside the feature's scope.
- A restriction is evaluated for the extruder active during that move. Travel
  to a tool change uses the outgoing extruder; travel after the tool change uses
  the incoming extruder.
- Collision is evaluated against the exact transformed mesh at the Z height of
  the travel. Only the numerical margin needed for robust polygon operations
  and G-code coordinate quantization is applied; there is no configurable
  clearance.
- A move already above the top of a volume is unrestricted. This version does
  not introduce an extra Z hop to clear a volume. When a collision is found,
  the planner finds an XY route around the union of applicable keep-out slices
  at the travel Z.
- The keep-out route is mandatory. It is not controlled by `Avoid crossing
  walls` and is not limited by that feature's maximum-detour setting.
- Every emitted segment of a routed move is checked against the keep-out area.
  Touching or entering the forbidden area is considered a collision.
- If the travel start or destination is in a forbidden area, or no legal XY
  route exists, G-code generation fails with an actionable slicing error. It
  must never silently fall back to the direct unsafe move.

## Data and compatibility

- Existing numeric model-volume type values remain unchanged; the new type is
  appended to the enum.
- Older projects without this volume type behave exactly as before.
- Readers that do not recognize the new 3MF subtype may not enforce the
  restriction; the subtype therefore uses an explicit descriptive string and
  is not serialized as a printable part.
- Changing, adding, deleting, transforming, instancing, or editing the
  extruder rule of a non-traversable volume invalidates G-code generation.

## Validation

Automated coverage must include:

- volume-type string conversion and project round-trip data;
- all-extruder and explicit-subset selection semantics;
- transformed and duplicated keep-out geometry;
- direct travel with no collision;
- XY detouring around a collision;
- unrestricted travel above a volume;
- different results for blocked and unblocked extruders;
- failures for endpoints inside the keep-out and for an impossible route; and
- routing that is active even when `Avoid crossing walls` is disabled.

Manual validation should confirm the add/change-type UI, translucent rendering,
extruder-selection UI, previewed travel paths, and a purge-tower round trip.
