# Binary Format Specification

This document specifies the binary containers written and read by FontPacker:

| Extension | Container | Produced from | One file contains |
|---|---|---|---|
| `.wodf` | `PreprocessedFontFace` | a TrueType/OpenType font, or an SVG | a whole font: many glyphs |
| `.wodi` | `StoredVectorImage` | an SVG | a single vector graphic (plus its mip chain) |

Both use `QDataStream` with the `Qt_4_0` stream version and **Big-Endian** byte order. The CBOR variants carry the same information and are described in the CBOR Mapping section below.

The layout is written field by field by `PreprocessedFontFace::toData`, `StoredCharacter::toData` and `StoredVectorImage::toData`; there is no in-memory struct dumped verbatim, so no C++ padding or alignment is ever present in the file.

Related documents:

- [README.md](README.md) - how a package is produced from the command line and what the options mean.
- [SdfGenerationContext.hpp](SdfGenerationContext.hpp), [FontOutlineDecompositionContext.hpp](FontOutlineDecompositionContext.hpp) - the code that produces the values described here.

---

## Quick summary for implementers and AI agents

Read this before the tables; it prevents the most common misreading of the format.

1. A `.wodf` stores **one independently encoded image per glyph**. Every glyph image is a full, square `S x S` tile. There is **no atlas** in the file and no per-glyph sub-rectangle: the engine packs tiles into an atlas itself.
2. Three different "sizes" exist and they are not interchangeable:
   - `bitmap_logical_size` (`L`) - the resolution at which the distance field was **generated**. Square. Padding is **inside** this size.
   - `bitmap_padding` (`P`) - the border, in **logical pixels**, added on all four sides. It is **part of** `L`, not added on top of it.
   - `bitmap_size` (`S`) - the resolution of the image **actually stored** after downsampling. Square. Writers always store the real value; a legacy version 1 file may contain `0`, which means "no downsampling was requested" (the stored images are `L x L`).
3. The glyph's content occupies the padded `(L - 2P) x (L - 2P)` box of the generation canvas, and it is **stretched anisotropically** to fill that box. The fit uses the **glyph metric box**: the FreeType metric box for fonts, and the SVG viewport for SVG input. The real aspect ratio comes back from `StoredCharacter::width` / `height` at runtime. All float metric fields keep their meaning in version 2 (see "Units and provenance").
4. For **font input**, the per-glyph metrics are **not in font units** and **not in final texels**. They are in the pixel space defined by FreeType's `em = L - P`; `advance_x` / `advance_y` are raw 26.6 fixed point, everything else is a float in pixels. For **SVG input**, all numeric fields are in SVG user units, and the integer advances are 26.6 fixed point of those units. See the "Units and provenance of the glyph metrics" section below.
5. `.wodf` **version 2** stores the nominal signed-distance ranges (`distanceRangeX`/`Y`), the baked-transform `encodingFlags` and `midpointAdjustment`; version 1 files do not. Both containers therefore expose the same metadata, but the range is only the nominal search radius, not an exact per-image scale (see "Distance value encoding").
6. All multi-byte integers and floats are Big-Endian. `bool` is one byte. Variable-length strings are a `uint32` byte count (including the terminating NUL) followed by UTF-8 bytes and the NUL.

---

## Data types and endianness

| Type | Size | Notes |
|---|---|---|
| `uint8_t` | 1 | unsigned; enum fields are written as this |
| `bool` | 1 | `QDataStream` writes `0x00`/`0x01` for `Qt_4_0` |
| `uint32_t` | 4 | unsigned, Big-Endian |
| `int32_t` | 4 | two's complement, Big-Endian |
| `float` | 4 | IEEE-754 binary32, Big-Endian |
| `char[4]` | 4 | raw ASCII magic, not NUL-terminated |
| `char[8]` | 8 | NUL-terminated ASCII, zero padded |
| UTF-8 string | 4 + n | `uint32` size **including** the NUL, then n bytes ending in `0x00` |

`QDataStream::setVersion(QDataStream::Qt_4_0)` and `setByteOrder(QDataStream::BigEndian)` must be applied before reading or writing. Note that `QDataStream` itself would encode a `QString` differently; the family name is written by `saveToDatastreamAsUtf8()` / `loadFromDatastreamAsUtf8()`, i.e. exactly as described above.

---

## Raster geometry: padding vs. logical size vs. stored size (IMPORTANT)

This is the part that is easiest to get wrong. All three quantities are single integers, and all three describe a **square** (`1:1`) texture.

### Notation

| Symbol | `.wodf` field | `.wodi` field | Meaning |
|---|---|---|---|
| `L` | `bitmap_logical_size` | `processingSize` | Generation resolution, in pixels. The distance field is evaluated on an `L x L` grid. |
| `S` | `bitmap_size` | `actualSize` | Stored resolution, in pixels. Level 0 of the image/mip chain is `S x S`. |
| `P` | `bitmap_padding` | `padding` | Border on every side, in pixels **of the `L` grid**. Part of `L`. |
| `k` | `S / L` | `S / L` | Downsample factor. Usually `1/2^n`. |
| `L_in` | `L - 2P` | `L - 2P` | Content box in logical pixels. |
| `P_final` | `P * S / L` | `P * S / L` | Padding expressed in stored (level-0) texels. May be fractional. |
| `S_in` | `L_in * S / L` | `L_in * S / L` | Content box in stored texels. May be fractional. |

### What the square actually looks like

```
bitmap_logical_size = L            (generation canvas)
+---------------------------------+
|                P                |
|    +-----------------------+    |
|    |                       |    |
|  P |   glyph content box   | P  |   content box = (L-2P) x (L-2P)
|    |   (stretched to fill)  |    |
|    |                       |    |
|    +-----------------------+    |
|                P                |
+---------------------------------+

Downsample by k = S / L  ->  a full S x S stored image.
```

### The six rules

1. **Padding is inside `L`.** The drawable area is `(L - 2P) x (L - 2P)`. Never compute `L + 2P`; that would be a canvas larger than the generated image.
2. **The stored image is square, not the glyph.** Aspect ratio is destroyed on purpose (per-axis stretch, see below) so that every GPU tile has a predictable shape. `width`/`height` restore it for both font and SVG input; for SVG they are the viewport size.
3. **`bitmap_size` is nonzero in files written by this version.** Writers resolve it to the real `S`, falling back to `L` when no downsampling happened. A legacy version 1 file may contain `0`; treat that as `S = L`. `.wodi` never stores 0 for `actualSize` because it is assigned from the final image width.
4. **`P` is in logical pixels.** In the stored image it becomes `P_final = P * S / L`, which is fractional unless `P` is a multiple of `L / S`. Choose `P` as a multiple of `L / S` (for example `L=1024`, `S=64`, `P` a multiple of 16) if you need texel-exact content edges. FontPacker does not enforce this.
5. **Square is a format invariant, power-of-two is not.** The format stores one integer per size, so both images must be square. Power-of-two sizes are only the default and the reason for the halving loop; non-power-of-two `S` is resampled to the exact value and is valid on any modern GPU.
6. **The stored values are not distances in this pixel space.** Both generators normalise the distance field per image so that `0.5` is the contour and `1.0`/`0.0` are that image's maximum inside/outside distances. `P` is the nominal search radius (an upper bound), not a guaranteed distance scale. See the "Distance value encoding" section below.

Why store both `L` and `S` instead of only the final size? `L` records how much SDF runway (gradient resolution) the tile was given, which is what a shader needs to derive the encoded range (`P_final = P * S / L`) and what a re-generation tool needs to reproduce or improve the asset. `S` is the resolution a runtime actually uploads. Keeping them separate tells a re-generation tool which resolution the gradient came from and which resolution was stored, so it can reproduce the pipeline or choose a different quality trade-off on the next run; it does not let you downsample the stored file again, because the full `L x L` image is not kept.

### How a glyph tile is produced (and where each field comes from)

Font input (`SdfGenerationContext::processFont`):

1. FreeType is configured with `FT_Set_Pixel_Sizes(face, L - P, L - P)`. This is the **em size of the metric space**: 1 em = `L - P` pixels. The face metrics are read here (`ascender`, `descender`, `faceHeight`, `maxAdvance`).
2. The outline is decomposed into line/quadratic/cubic segments (`FT_Outline_Decompose` into `FontOutlineDecompositionContext`).
3. The decomposed outline is fitted to the inner content box using the **FreeType metric box**:
   ```text
   minDim = (horiBearingX, horiBearingY - metricHeight)      // bottom-left of the metric box, relative to the baseline
   scaleX = (L - 2P) / metricWidth
   scaleY = (L - 2P) / metricHeight
   x_tex  = (x - minDim.x) * scaleX + P
   y_tex  = L - ((y - minDim.y) * scaleY + P)               // Y flipped once for the font path
   ```
   `processOutlineGlyphEnd` copies the four metric values into `double` locals before calling `translateToNewSize`, so the metric-based overload is selected and the members themselves are **not** modified. The scale is non-uniform: a narrow `i` and a wide `W` both fill the same `(L - 2P)^2` box. That is the deliberate texel-usage trade-off; the real aspect ratio survives in `width`/`height`, and the metric box is stored unchanged in `metricWidth`/`metricHeight`/`horiBearingX`/`horiBearingY`.
4. The SDF/MSDF/MSDFA is evaluated on the `L x L` grid (`SdfGenerationGL::produceOutlineSdf`, or the bitmap variant).
5. `downsampleToIntendedSize()` halves the image until the next power of two >= `S` is reached, then smooth-resamples to exactly `S` (if `S != 0`). The encoded result becomes `StoredCharacter::sdf` in `imageFormat`.
6. The layout metrics (`width`, `height`, `bearing_x`, `bearing_y`, `advance_x`, `advance_y`) are copied **unchanged** from FreeType (they were measured at step 1, before the stretch in step 3), and every float metric keeps its metric-box value. This is why `width`/`height` describe the intended layout and not the texture.

Bitmap input (`processBitmapGlyph`, used for bitmap fonts or `--forceraster`): the rasterised glyph is scaled to `(L - 2P)^2` ignoring aspect ratio, padded by `P` on every side, and then the SDF is generated from that `L x L` bitmap. All eight float metric fields keep their FreeType values; this path does not call the fit.

SVG input (`processSvg` / `processSvgShapes`): `width`/`height`, the float metric box (`metricWidth = viewport width`, `metricHeight = viewport height`, `horiBearingX = 0`, `horiBearingY = viewport height`) and `horiAdvance` are all the SVG **viewport**; `advance_x`/`advance_y` are the viewport dimensions in 26.6 fixed point. The fit therefore maps the whole viewport onto the content box, preserving the shape's position and relative size inside the canvas. Bearing fields are 0. A zero-sized viewport is rejected with an error.

### Worked example

`L = 1024`, `P = 100`, `S = 64` (the OpenGL example in the README):

| Quantity | Value |
|---|---|
| Content box in logical px | `L_in = 1024 - 200 = 824` |
| Downsample factor | `k = 64 / 1024 = 1/16` |
| Padding in stored texels | `P_final = 100 / 16 = 6.25` |
| Content box in stored texels | `S_in = 824 / 16 = 51.5` |
| Content box in UV space | `[P/L, (L-P)/L] = [0.09765625, 0.90234375]` on both axes |

For a glyph with intended box `W = 400`, `H = 500` (pixels, from `width`/`height`):

- one stored texel spans `W / S_in = 400 / 51.5 = 7.767` screen px horizontally and `H / S_in = 500 / 51.5 = 9.709` screen px vertically;
- the padding runway is `padX = W * P / (L - 2P) = 400 * 100 / 824 = 48.544` px and `padY = 500 * 100 / 824 = 60.680` px on **each** side;
- the complete quad (content + runway) is `497.087 x 621.359` px;
- equivalently, `padX` in stored texels is `P_final = 6.25`, which times 7.767 px/texel gives the same 48.544 px.

If `P` were 96 instead of 100 (`96 = 6 * 16`), `P_final` would be an integer 6 and the content edge would land exactly on a stored texel boundary.

### Restoring the glyph on screen

With a pen position at the baseline origin and a Y-down screen coordinate system, the padded tile is drawn as:

```
padX = width  * padding / (bitmap_logical_size - 2 * padding)
padY = height * padding / (bitmap_logical_size - 2 * padding)

quadTopLeft = (pen.x + bearing_x - padX, pen.y - bearing_y - padY)
quadSize    = (width + 2 * padX,           height + 2 * padY)

uv(0,0) = quadTopLeft                             // row 0 is the top of the tile
uv(1,1) = quadTopLeft + quadSize
```

The whole tile is mapped to the whole expanded quad. Because the quad was expanded by exactly the inverse of the content-box ratio, the glyph silhouette lands exactly on `width x height` at `bearing_x, bearing_y`, while the shader still has the `P_final`-texel runway for glows, shadows and bevels.

Points to be aware of:

- **Y-flip.** The font path calls `translateToNewSize(..., invertY = true)`, so row 0 of a glyph image is the top of the glyph and a Y-down quad is correct. The SVG path passes `invertY = false`; SVG user space is already Y-down, so row 0 is again the top. Both containers are stored top-row-first.
- **Anisotropy.** `scaleX != scaleY` in general, so a circular distance band in the texture is an ellipse in the source. Do **not** use a constant antialiasing width in normalized-distance units; use screen-space derivatives, which automatically account for the per-axis stretch:
  ```glsl
  float aaWidth = length(vec2(dFdx(d), dFdy(d))); // d = sampled distance value
  float alpha  = smoothstep(0.5 - aaWidth, 0.5 + aaWidth, d);
  ```
  If you must work in texture space, convert per axis: `aaTexelsX = aaPx * S_in / width`, `aaTexelsY = aaPx * S_in / height` with `S_in = (L - 2P) * S / L`.
- **Kerning and advances** are in the same `em = L - P` pixel space as `width`, so they can be used directly with the intended box sizes. They do not need to be rescaled into texel space.
- **SVG glyphs use the viewport.** For a `.wodf` produced from an SVG, `width`/`height` are the viewport, the metric box is the viewport, and the texture contains the whole viewport scaled into the content box. Draw the tile at the pen with the top-left of the viewport at `(bearing_x, -bearing_y)`; shapes keep their position inside the canvas. Use `--outvectorbin` if you need the shape bounds and logical rectangle under explicit names.
- The runtime should treat `width`, `height`, `bearing_x`, `bearing_y`, `advance_*` and the kerning `Vec2f` as the layout data, and the tile merely as a resolution-independent fill for the content box. For both font and SVG input that box is `width x height`.

### Distance value encoding

Both backends compute a raw distance in **`L`-grid pixels**, then normalise it **per image** (`SdfGenerationContextSoft::produceBitmapSdf`, `SdfGenerationGL::fetchSdfFromGPU`, `fetchMSDFFromGPU`):

- inside pixels are mapped to `[0.5, 1.0]`, outside pixels to `[0.0, 0.5]`, with `0.5` on the contour;
- the mapping divides by the maximum inside/outside distance **observed in that particular image**, not by a fixed range;
- optional transforms (`--invert`, `--gammacorrect`, `--midpointadjustment`) are applied afterwards and baked into the stored bytes **by the OpenGL backend**; the Software backend currently normalises only and ignores those flags.

Consequences:

- A shader can use a constant threshold of `0.5` and a screen-space antialiasing band regardless of glyph size.
- `.wodf` **version 2** stores the nominal `distanceRangeX`/`Y`, `encodingFlags` and `midpointAdjustment`, exactly like `.wodi`. The range is `(samples/2 or P) * S / L`, i.e. the search radius expressed in level-0 texels. Because each image is normalised by its own observed maximum, the actual value range can be smaller; use the stored range as an upper bound (`screenPxRange`) rather than an exact conversion.
- Version 1 files have no such fields and are **not** upgraded automatically. FontPacker reconstructs the nominal padding-based range (`P * S / L`) in memory for convenience, but keeps the file at version 1 when rewriting, because the old bounds-based glyph layout and plain-pixel SVG advances cannot be converted without regeneration. Regenerate to obtain version 2.
- Transforms that were baked into the bytes are recorded in `encodingFlags` (see the `.wodi` section for the bit values). The Software backend still ignores `--invert`, `--gammacorrect` and `--midpointadjustment`, so those flags are only set by the OpenGL backend.

---

## PreprocessedFontFace (`.wodf`)

### Top-level layout

```
PreprocessedFontFace {
    magic                   char[4]     "WODF"
    version                 uint32_t    2 (current); 1 is still readable
    familyNameSize          uint32_t    byte count including the NUL (minimum 1)
    familyName              uint8[]     UTF-8, familyNameSize bytes, ends with 0x00
    type                    uint8_t     SDFType
    distType                uint8_t     DistanceType
    bitmap_size             uint32_t    S (version 2: always the real size; version 1: 0 = same as bitmap_logical_size)
    bitmap_logical_size     uint32_t    L
    bitmap_padding          uint32_t    P, in pixels of the L grid
    hasVert                 bool        1 byte
    imageFormat             char[8]     NUL-terminated, zero padded, upper-case
    ascender                float       pixels at em = L - P
    descender               float       pixels at em = L - P (usually negative)
    faceHeight              float       pixels at em = L - P
    maxAdvance              float       pixels at em = L - P
    unitsPerEm              uint32_t    original font design units per EM
    distanceRangeX          float       version 2: nominal encoded radius, level-0 texels
    distanceRangeY          float       version 2: nominal encoded radius, level-0 texels
    encodingFlags           uint32_t    version 2: bitmask of baked transforms
    midpointAdjustment      float       version 2: baked midpoint divisor (if flagged)
    charCount               uint32_t    number of TOC entries == number of glyphs
    glyphTOC                GlyphTOC[charCount]
    kerning                 KerningMap
    glyphData               StoredCharacter[charCount]  (at absolute offsets from the TOC)
}
```

Absolute offsets, with `N = familyNameSize`:

| Offset | Field | Type | Notes |
|---|---|---|---|
| 0 | `magic` | `char[4]` | `"WODF"` |
| 4 | `version` | `uint32_t` | currently `2`; readers must accept `1` and `2`, and reject `0` and `> 2`. New generation writes 2; a loaded version 1 file is rewritten as 1. |
| 8 | `familyNameSize` | `uint32_t` | includes the NUL |
| 12 | `familyName` | `uint8[N]` | UTF-8 with NUL |
| `12+N` | `type` | `uint8_t` | see the "Type enumerations" section below |
| `12+N+1` | `distType` | `uint8_t` | see the "Type enumerations" section below |
| `12+N+2` | `bitmap_size` | `uint32_t` | stored/level-0 square size `S`; version 2 writers always write the real size, version 1 may contain `0` (= `L`) |
| `12+N+6` | `bitmap_logical_size` | `uint32_t` | generation square size `L` |
| `12+N+10` | `bitmap_padding` | `uint32_t` | border `P` in logical pixels, inside `L` |
| `12+N+14` | `hasVert` | `bool` | vertical metrics valid? |
| `12+N+15` | `imageFormat` | `char[8]` | e.g. `"PNG"`; at most 7 characters plus NUL |
| `12+N+23` | `ascender` | `float` | face ascender; font: px at `em = L - P`, SVG: viewport height |
| `12+N+27` | `descender` | `float` | font: px at `em = L - P` (usually negative), SVG: 0 |
| `12+N+31` | `faceHeight` | `float` | baseline-to-baseline distance; font px, SVG viewport height |
| `12+N+35` | `maxAdvance` | `float` | font px, SVG viewport width |
| `12+N+39` | `unitsPerEm` | `uint32_t` | font: original design units per EM; SVG: viewport height |
| `12+N+43` | `distanceRangeX` | `float` | **version 2 only**; absent from a version 1 file |
| `12+N+47` | `distanceRangeY` | `float` | **version 2 only** |
| `12+N+51` | `encodingFlags` | `uint32_t` | **version 2 only** |
| `12+N+55` | `midpointAdjustment` | `float` | **version 2 only** |
| `12+N+59` | `charCount` | `uint32_t` | |
| `12+N+63` | `glyphTOC` | `GlyphTOC[charCount]` | 8 bytes each |
| ... | `kerning` | `KerningMap` | see below |
| ... | `glyphData` | - | each entry starts at the absolute offset stored in the TOC |

In a version 1 file the four version-2 fields are absent, so `charCount` is at `12+N+43` and the TOC starts at `12+N+47`.

### Face metrics

For **font input**, `ascender`, `descender`, `faceHeight` and `maxAdvance` come from `FT_Size_Metrics` after `FT_Set_Pixel_Sizes(face, L - P, L - P)`, converted from 26.6 fixed point to `float`. They are therefore in **pixels at an em size of `L - P`**, not in font units and not in final texels.

For **SVG input** there is no FreeType face: `ascender` and `faceHeight` are the viewport height, `descender` is 0, `maxAdvance` is the viewport width, and `unitsPerEm` is set to the viewport height. These values are in SVG user units; do not apply the font conversion formula to them.

For font input, converting a stored pixel value to font design units:

```
value_in_font_units = value_px * unitsPerEm / (bitmap_logical_size - bitmap_padding)
```

To convert it to stored texels (only meaningful for layout that is meant to follow the texture):

```
value_in_texels = value_px * ((bitmap_logical_size - 2 * bitmap_padding) * S / L) / width
```

but normally the intended pixel space is the one to use; the texture is just a fill for it.

### Glyph TOC

The table of contents is written twice: first as zero placeholders, then overwritten with the real values after the glyph blobs have been written. Entries are emitted in ascending code-point order (`QMap` iteration order), but readers must not depend on ordering; use the offsets for random access.

```
GlyphTOC {
    codePoint   uint32_t   Unicode code point
    offset      uint32_t   absolute byte offset from the start of the file to the StoredCharacter
}
```

Each entry is 8 bytes. `charCount` is the number of entries, and it equals `storedCharacters.size()` rather than a `0x10FFFF`-style range size.

### KerningMap

Kerning is stored sparsely: only pairs with a non-zero offset appear.

```
KerningMap {
    size        uint32_t                 number of first-character entries
    entries     KerningMapEntry[size]
}

KerningMapEntry {
    firstChar   uint32_t                 first code point
    perChar     PerCharacterKerning      pairs beginning with firstChar
}

PerCharacterKerning {
    size        uint32_t                 number of pairs
    entries     PerCharKerningEntry[size]
}

PerCharKerningEntry {
    secondChar  uint32_t                 second code point
    kerning     Vec2f                    x, y offset
}

Vec2f {
    x           float
    y           float
}
```

`kerning` is in pixels at `em = L - P` (FreeType's `FT_Get_Kerning` with `FT_KERNING_DEFAULT`, converted from 26.6), i.e. the same space as `horiAdvance` and `width`/`height`. It is not in final texels.

---

## StoredCharacter (glyph payload)

Every TOC entry points at one of these. Offsets below are relative to the glyph's own offset.

```
StoredCharacter {
    valid           bool        @ 0
    [if valid:]
    width           uint32_t    @ 1
    height          uint32_t    @ 5
    bearing_x       int32_t     @ 9
    bearing_y       int32_t     @ 13
    advance_x       uint32_t    @ 17
    advance_y       uint32_t    @ 21
    metricWidth     float       @ 25
    metricHeight    float       @ 29
    horiBearingX    float       @ 33
    horiBearingY    float       @ 37
    horiAdvance     float       @ 41
    vertBearingX    float       @ 45
    vertBearingY    float       @ 49
    vertAdvance     float       @ 53
    sdfLength       uint32_t    @ 57
    sdf             uint8[]     @ 61, sdfLength bytes
}
```

**Size arithmetic (corrected):** after the 1-byte `valid`, there are 6 integer fields (4 x `uint32_t` + 2 x `int32_t` = 24 bytes) and 8 floats (32 bytes), so the fixed part is **56 bytes**, followed by `sdfLength` (4 bytes). Therefore a valid glyph occupies **61 bytes + `sdfLength`**; an invalid glyph occupies exactly **1 byte**. (Earlier revisions of this document incorrectly claimed 72 fixed bytes and 77 total.)

### Version 2 header fields

`distanceRangeX`, `distanceRangeY`, `encodingFlags` and `midpointAdjustment` have exactly the same meaning and bit layout as in `StoredVectorImage` (see "Distance and encoding fields" below). They describe the generation run that produced every glyph in the file:

- the ranges are the nominal search radius expressed in level-0 (`S`) texels;
- the flags record which post-processing transforms are baked into the pixels;
- `midpointAdjustment` is the divisor when `EncodingHasMidpointAdjustment` is set, otherwise `1.0`.

The backend still normalises each image by its own observed maximum distance, so the stored range is an upper bound, not an exact per-glyph scale. Version 1 files have no such fields; the reader reconstructs `P * S / L` in memory for convenience. Loading a version 1 file and writing it back keeps version 1 (and does not write the four fields), because the glyph layout cannot be converted without regeneration. `--format` re-encodes the stored images and `--intendedsize` rescales them (see README, "Repacking stored containers"); those are the only generation-like options that apply to a loaded file.


### Units and provenance of the glyph metrics

> **Version 1 legacy semantics.** Version 1 files were produced by the old fit
> path: for outline and SVG glyphs the four float metric fields contain the
> outline bounds (`minX`, `maxX`, `minY`, `maxY`), SVG `advance_x`/`advance_y`
> are plain viewport pixels rather than 26.6, SVG textures contain the shape
> bounds instead of the viewport, and `bitmap_size` may be 0. The table below
> describes **version 2** output. Font layout fields (`width`, `height`,
> bearings) remain usable for version 1, but version 1 SVG advances are plain
> pixels; do not assume the version 2 metric semantics for those files.

For **font input**, the single most important sentence is: **the float fields are in pixels, in the FreeType pixel space where 1 em = `bitmap_logical_size - bitmap_padding`; they are not in font units and not in stored texels.** The integer fields are the exception: `advance_x` and `advance_y` are raw 26.6 fixed point.

For **SVG input**, every numeric glyph field is in **SVG user units**: `width`/`height` and `horiAdvance` are the viewport size, the float metric box is the viewport, and `advance_x`/`advance_y` are the viewport size in 26.6 fixed point, the same convention as fonts.

| Field | Source in the code | Unit | Meaning |
|---|---|---|---|
| `valid` | `processOutlineGlyph` / `processBitmapGlyph` | bool | `false` (or an absent TOC entry) means no payload follows. `true` with `sdfLength == 0` is a legitimate **empty glyph** (e.g. space). |
| `width` | `FT_GlyphSlot.bitmap.width` / SVG viewport | whole pixels | Font: intended width of the glyph box at `em = L - P`; the width the runtime should draw the content box at. SVG: the viewport width. It is **not** the texture size and does **not** include padding. |
| `height` | `FT_GlyphSlot.bitmap.rows` / SVG viewport | whole pixels | Same, vertically. |
| `bearing_x` | `FT_GlyphSlot.bitmap_left` | whole pixels | Horizontal offset from the pen origin to the left edge of the glyph box. Signed. Always 0 for SVG input. |
| `bearing_y` | `FT_GlyphSlot.bitmap_top` | whole pixels | Vertical offset from the baseline **up** to the top edge of the glyph box. Signed. Always 0 for SVG input. |
| `advance_x` | `FT_GlyphSlot.advance.x` / SVG viewport | **26.6 fixed point** | Divide by 64 for pixels; `horiAdvance` is the same quantity as a float. SVG values are the viewport width/height in 26.6. |
| `advance_y` | `FT_GlyphSlot.advance.y` / SVG viewport | **26.6 fixed point** | Same convention; usually 0 for horizontal fonts. |
| `metricWidth` | `FT_GlyphSlot.metrics.width` / SVG viewport width | float pixels | The metric-box width used by the fit. **Not** modified by the fit. |
| `metricHeight` | `FT_GlyphSlot.metrics.height` / SVG viewport height | float pixels | The metric-box height used by the fit. |
| `horiBearingX` | `FT_GlyphSlot.metrics.horiBearingX` / 0 | float pixels | Sub-pixel left bearing of the metric box. |
| `horiBearingY` | `FT_GlyphSlot.metrics.horiBearingY` / SVG viewport height | float pixels | Sub-pixel top bearing of the metric box. |
| `horiAdvance` | `FT_GlyphSlot.metrics.horiAdvance` / SVG viewport width | float pixels | Horizontal advance (`advance_x / 64`). |
| `vertBearingX` | `FT_GlyphSlot.metrics.vertBearingX` | float pixels | Valid only when the face has vertical metrics (`hasVert`). |
| `vertBearingY` | `FT_GlyphSlot.metrics.vertBearingY` | float pixels | |
| `vertAdvance` | `FT_GlyphSlot.metrics.vertAdvance` | float pixels | |
| `sdfLength` | length of the encoded image | bytes | |
| `sdf` | `QImage::save(..., imageFormat)` | encoded bytes | A complete `S x S` image (or `L x L` for a legacy version 1 file with `S == 0`). Format is the header's `imageFormat`. Values are SDF/MSDF; see the "Distance value encoding" section below. |

Notes:

- **The fit uses the metric box.** `processOutlineGlyphEnd` passes `metricWidth`, `metricHeight`, `horiBearingX` and `horiBearingY` as `double` copies, so the metric overload of `translateToNewSize` is selected and the four members keep their FreeType values. They describe the box the glyph was stretched *from*, not the texture.
- **Raster glyphs** (`--forceraster`, bitmap fonts) keep their FreeType metric values too. Their `width`/`height` are the raster bitmap dimensions; the raster path scales that bitmap into the content box instead of using the metric overload.
- **SVG-backed font faces use the viewport throughout.** `processSvg` sets `width`/`height`, the four float metrics and `horiAdvance` to the viewport, sets the bearings to 0, and writes the viewport size in 26.6 into the integer advances. The texture contains the whole viewport, so the shape keeps its position inside the canvas.
- The generator stretches the glyph **non-uniformly**; the scale is `(L - 2P) / metricWidth` by `(L - 2P) / metricHeight`. `width / height` is the aspect ratio the runtime should restore.
- `hasVert == false` means vertical metrics are not meaningful; do not use `vertBearingX/Y` or `vertAdvance`.
- A glyph entry is never padded to an alignment; the next TOC entry starts immediately after the last `sdf` byte of the previous one (blobs are written back to back, in code-point order).

---

## StoredVectorImage (`.wodi`)

A standalone container for a single vector graphic. It is not part of a `PreprocessedFontFace`; it is the output of `--outvectorbin` / `--outvectorcbor`.

### Layout

```
StoredVectorImage {
    magic                   char[4]     "WODI"
    version                 uint32_t    currently 1; reject 0 and > 1
    processingSize          uint32_t    L: generation square size
    actualSize              uint32_t    S: level-0 stored square size (never 0)
    padding                 uint32_t    P: border in pixels of the L grid

    logicalX                float       original canvas/viewBox X origin
    logicalY                float       original canvas/viewBox Y origin
    logicalWidth            float       original canvas/viewBox width
    logicalHeight           float       original canvas/viewBox height
    aspectRatio             float       logicalWidth / logicalHeight

    minX                    float       control-point bbox minimum X, before normalisation
    maxX                    float       control-point bbox maximum X, before normalisation
    minY                    float       control-point bbox minimum Y, before normalisation
    maxY                    float       control-point bbox maximum Y, before normalisation

    distanceRangeX          float       nominal encoded radius, level-0 texels
    distanceRangeY          float       nominal encoded radius, level-0 texels
    encodingFlags           uint32_t    bitmask of baked transforms
    midpointAdjustment      float       baked midpoint divisor (if flagged)
    type                    uint8_t     SDFType
    distType                uint8_t     DistanceType
    imageFormat             char[8]     NUL-terminated, zero padded

    mipmapCount             uint32_t    number of encoded images, at least 1
    mipmaps                 Mipmap[mipmapCount]
}

Mipmap {
    dataLength              uint32_t
    data                    uint8[]     one complete encoded image
}
```

### Geometry fields

- `processingSize` (`L`) and `padding` (`P`) have the same meaning as in `.wodf`: `P` is part of `L`, and the source content is stretched onto the `(L - 2P)^2` box.
- `actualSize` (`S`) is the size of the **level-0** image after downsampling, so it is the size of `mipmaps[0]`. If `--intendedsize` was not given, `actualSize == processingSize`.
- `minX`/`maxX`/`minY`/`maxY` are captured **before** the normalisation/scaling in `FontOutlineDecompositionContext::translateToNewSize` (the overload taking `minX..maxY`), so they are in the original SVG/source coordinate space. Curves contribute their control points to the bounds.
- `logicalWidth`/`logicalHeight` come from the SVG viewport; the bundled svgtiny parser does not expose the viewBox origin, so `logicalX`/`logicalY` are currently always 0. If the viewport size is unavailable (`<= 0`), the fields fall back to `maxX - minX` / `maxY - minY`.
- `aspectRatio` is precomputed as `logicalWidth / logicalHeight`, or `1.0` if the height is 0.

Mapping a texture coordinate `(u, v)` in `[0, 1]` back to source coordinates:

```
u_inner = (u - P/L) / (1 - 2P/L)          // 0..1 inside the content box
v_inner = (v - P/L) / (1 - 2P/L)

x_src = minX + u_inner * (maxX - minX)
y_src = minY + v_inner * (maxY - minY)
```

Conversely, source coordinates map to logical texture pixels by `x_tex = P + (x - minX) * (L - 2P) / (maxX - minX)` and likewise for `y`; the generator flips Y only for font input, never for SVG.

### Distance and encoding fields

- `distanceRangeX` / `distanceRangeY` are computed by `effectiveDistanceRange()`:
  ```
  radius_logical = (samples_to_check_x ? samples_to_check_x / 2 : padding)
  distanceRangeX = radius_logical * actualSize / processingSize
  ```
  (and the analogous Y formula). With the defaults this is `P * S / L = P_final`. Because the encoder normalises each image by its own maximum distance, this is the **nominal search radius / upper bound** of the encoded range, not a guaranteed pixel-to-distance conversion. It exists for MSDF shaders that want a `screenPxRange`.
- `encodingFlags` records transforms that were baked into the bytes:

  | Bit | Name | Meaning |
  |---|---|---|
  | `0x1` | `EncodingInverted` | values were inverted (`1 - x`) |
  | `0x2` | `EncodingGammaCorrected` | the cubic gamma adjustment was applied |
  | `0x4` | `EncodingMaximizedDownsample` | mip generation used max instead of average |
  | `0x8` | `EncodingHasMidpointAdjustment` | `midpointAdjustment` holds a baked divisor |

- `midpointAdjustment` is only meaningful when bit `0x8` is set; otherwise it is written as `1.0`.
- Repacking a loaded `.wodi` to a different `actualSize` (`--intendedsize`) scales `distanceRangeX/Y` by `newActualSize / oldActualSize` and rebuilds the mip chain; re-encoding alone leaves the levels and ranges unchanged.
- `mipmaps[0]` is level 0 at `actualSize`. Each subsequent entry is an independently encoded image of half the previous size. Mipmaps are only produced when `--createmipmaps` is passed (`processOutlineGlyphEnd(StoredVectorImage&, ...)`); otherwise `mipmapCount == 1`.
- As with `.wodf`, each mip image is square, and `imageFormat` applies to every mip level.

---

## Reading algorithm (font)

1. Read `magic` (`char[4]`). It must be `WODF` (`57 4F 44 46`).
2. Read `version` (`uint32_t`). Supported values: `1` and `2`; reject `0` and anything greater.
3. Read `familyNameSize` (`uint32_t`, includes the NUL).
4. Read `familyNameSize` bytes of UTF-8; the last byte is `0x00`.
5. Read the metadata in exactly this order:
   - `type` (`uint8_t`), `distType` (`uint8_t`);
   - `bitmap_size`, `bitmap_logical_size`, `bitmap_padding` (`uint32_t` each);
   - `hasVert` (`bool`, 1 byte);
   - `imageFormat` (`char[8]`);
   - `ascender`, `descender`, `faceHeight`, `maxAdvance` (`float` each);
   - `unitsPerEm` (`uint32_t`);
   - **version 2 only:** `distanceRangeX`, `distanceRangeY` (`float`), `encodingFlags` (`uint32_t`), `midpointAdjustment` (`float`);
   - `charCount` (`uint32_t`).
6. Read `charCount` TOC entries of 8 bytes each (`codePoint`, `offset`).
7. Read the kerning map: `uint32 size`, then per entry `uint32 firstChar`, `uint32 pairCount`, then per pair `uint32 secondChar`, `float x`, `float y`.
8. For each TOC entry: seek to its **absolute** `offset` and read a `StoredCharacter`:
   - read `valid` (1 byte); if false, stop;
   - read 56 bytes of fixed fields;
   - read `sdfLength` (`uint32_t`);
   - read `sdfLength` bytes of encoded image.

A reader may ignore the TOC and read glyphs sequentially only if it also reads the kerning map in the same order; the TOC is the authoritative index.

## Writing algorithm (font)

1. Write magic and the container `version` (`2` for new generation, `1` when rewriting a loaded version 1 file), then `uint32(utf8.size() + 1)`, the UTF-8 bytes and a `0x00`.
2. Write the metadata fields in the order listed above. Write the four version-2 fields only when `version >= 2`. Use `charCount = storedCharacters.size()` and resolve `bitmap_size` to the real `S`.
3. Remember the current position, then write `charCount` zeroed 8-byte TOC placeholders.
4. Write the kerning map (size-prefixed nested maps).
5. For each glyph in code-point order: remember the absolute position, write `StoredCharacter` (bool, fixed fields, `uint32 sdfLength`, raw bytes), and record `(codePoint, offset)`.
6. Seek back to the remembered TOC position and overwrite the placeholders with the `(codePoint, offset)` pairs.

This is exactly what `PreprocessedFontFace::toData` does; it requires a seekable device.

## Reading algorithm (vector image)

1. Read `magic`; it must be `WODI` (`57 4F 44 49`).
2. Read `version` (`uint32_t`); supported value is `1`.
3. Read the metadata in the field order of the layout table above (`processingSize` ... `imageFormat`).
4. Read `mipmapCount` (`uint32_t`).
5. Repeat `mipmapCount` times: read `dataLength` (`uint32_t`), then `dataLength` bytes.

## Writing algorithm (vector image)

1. Write magic and version.
2. Write the metadata fields in the same order (`toData` writes them as one stream expression; keep that order).
3. Write `uint32(mipmaps.size())`.
4. For each mip level: write `uint32(size)` and the encoded bytes.

---

## Example parser pseudocode

The code below mirrors `StoredCharacter::fromData` and `PreprocessedFontFace::fromData` byte for byte, including the corrected 56-byte fixed block.

```python
# All reads are Big-Endian. f is a seekable binary file.

def read_font(f):
    assert read_bytes(f, 4) == b'WODF'
    version = read_u32(f)
    assert version in (1, 2)

    name_size = read_u32(f)            # includes the NUL
    family_name = read_bytes(f, name_size)[:-1].decode('utf-8')

    sdf_type    = read_u8(f)
    dist_type   = read_u8(f)
    bitmap_size = read_u32(f)          # S; version 1 may be 0 (= same as L)
    L           = read_u32(f)          # bitmap_logical_size
    P           = read_u32(f)          # bitmap_padding, logical px
    has_vert    = read_bool(f)
    image_fmt   = read_bytes(f, 8).split(b'\0', 1)[0].decode('ascii')
    ascender    = read_f32(f)
    descender   = read_f32(f)
    face_height = read_f32(f)
    max_advance = read_f32(f)
    units_per_em = read_u32(f)
    if version >= 2:
        distance_range_x = read_f32(f)
        distance_range_y = read_f32(f)
        encoding_flags   = read_u32(f)
        midpoint_adj     = read_f32(f)
    else:
        distance_range_x = distance_range_y = 0.0   # unknown in v1
        encoding_flags = 0
        midpoint_adj = 1.0
    char_count  = read_u32(f)

    toc = [(read_u32(f), read_u32(f)) for _ in range(char_count)]
    kerning = read_kerning(f)

    glyphs = {}
    for code_point, offset in toc:
        f.seek(offset)
        glyphs[code_point] = read_stored_character(f)

    S = bitmap_size if bitmap_size else L
    if version < 2 and L:
        distance_range_x = distance_range_y = P * S / L   # nominal reconstruction
    return dict(family=family_name, L=L, S=S, P=P, glyphs=glyphs,
                kerning=kerning, units_per_em=units_per_em,
                ascender=ascender, has_vert=has_vert, format=image_fmt,
                sdf_type=sdf_type, dist_type=dist_type,
                distance_range_x=distance_range_x,
                distance_range_y=distance_range_y,
                encoding_flags=encoding_flags,
                midpoint_adjustment=midpoint_adj)

def read_kerning(f):
    result = {}
    for _ in range(read_u32(f)):
        first = read_u32(f)
        pairs = {}
        for _ in range(read_u32(f)):
            pairs[read_u32(f)] = (read_f32(f), read_f32(f))
        result[first] = pairs
    return result

def read_stored_character(f):
    if not read_bool(f):
        return None                    # exactly one byte consumed
    width      = read_u32(f)
    height     = read_u32(f)
    bearing_x  = read_i32(f)
    bearing_y  = read_i32(f)
    advance_x  = read_u32(f)           # 26.6 fixed point!
    advance_y  = read_u32(f)           # 26.6 fixed point!
    metrics    = [read_f32(f) for _ in range(8)]   # 8 floats = 32 bytes
    sdf_len    = read_u32(f)
    sdf        = read_bytes(f, sdf_len)
    return dict(width=width, height=height, bearing_x=bearing_x,
                bearing_y=bearing_y, advance_x=advance_x / 64.0,
                advance_y=advance_y / 64.0,
                metric_width=metrics[0], metric_height=metrics[1],
                hori_bearing_x=metrics[2], hori_bearing_y=metrics[3],
                hori_advance=metrics[4], vert_bearing_x=metrics[5],
                vert_bearing_y=metrics[6], vert_advance=metrics[7],
                sdf=sdf)

def content_box(L, S, P):
    """Return (S_inner, P_final) in stored texels, plus the UV range."""
    k = S / L
    return ((L - 2 * P) * k, P * k, (P / L, (L - P) / L))
```

## CBOR mapping

CBOR files carry the same data. The discriminator is the `containerType` string; the `type` key remains the SDF flavor. Font and vector files are **not** self-describing beyond that string, so always check it before parsing.

### `PreprocessedFontFace`

| CBOR key | Value | Corresponds to |
|---|---|---|
| `containerType` | string `"PreprocessedFontFace"` | discriminator |
| `version` | unsigned | `version` |
| `fontFamily` | string | `fontFamilyName` |
| `type` | unsigned | `SDFType` |
| `dist` | unsigned | `DistanceType` |
| `dimensions` | unsigned | **`bitmap_size`** (note the legacy key name) |
| `logical_size` | unsigned | `bitmap_logical_size` |
| `padding` | unsigned | `bitmap_padding` |
| `hasVert` | bool | `hasVert` |
| `format` | string | `imageFormat` (without NUL padding) |
| `ascender` / `descender` / `faceHeight` / `maxAdvance` | float | the four face metrics |
| `unitsPerEm` | unsigned | `unitsPerEm` |
| `distanceRangeX` / `distanceRangeY` | float | **version 2:** nominal encoded ranges in level-0 texels |
| `encodingFlags` | unsigned | **version 2:** bitmask of baked transforms |
| `midpointAdjustment` | float | **version 2:** baked midpoint divisor |
| `glyphs` | map: integer code point -> glyph map | `storedCharacters` |
| `kerning` | map: first -> (map: second -> `[x, y]`) | `kerning` |

Version 1 CBOR files omit the four version-2 keys; the reader defaults them to `0, 0, 0, 1.0` and `dimensions` may be `0`.

Glyph map keys: `valid`, `width`, `height`, `bearing_x`, `bearing_y`, `advance_x`, `advance_y`, `metricsWidth`, `metricsHeight`, `horiBearingX`, `horiBearingY`, `horiAdvance`, `vertBearingX`, `vertBearingY`, `vertAdvance`, `sdf` (byte string).

### `StoredVectorImage`

| CBOR key | Value |
|---|---|
| `containerType` | string `"StoredVectorImage"` |
| `version`, `processingSize`, `actualSize`, `padding` | unsigned |
| `logicalX`, `logicalY`, `logicalWidth`, `logicalHeight`, `aspectRatio` | float |
| `minX`, `maxX`, `minY`, `maxY` | float |
| `distanceRangeX`, `distanceRangeY`, `midpointAdjustment` | float |
| `encodingFlags` | unsigned |
| `type`, `dist` | unsigned |
| `format` | string |
| `mipmaps` | array of byte strings |

---

## Notes and gotchas for implementers (and AI agents)

- **Never `memcpy` a C++ struct.** The file is written field by field. In-memory padding, `std::array` layout and enum sizes are not part of the format.
- **Big-Endian everywhere.** `QDataStream` defaults to Big-Endian, but both `setVersion(QDataStream::Qt_4_0)` and `setByteOrder(QDataStream::BigEndian)` must be set explicitly, or the file will be unreadable.
- **`bool` is one byte**, not four. Enum-valued fields (`type`, `distType`) are one byte too.
- **Strings**: only the family name uses the custom UTF-8 encoding. The length includes the terminating NUL, so the minimum is 1. Do not treat it as `QDataStream`'s `QString` encoding.
- **`imageFormat` is 7 usable characters plus NUL.** `setImageFormat()` upper-cases and truncates; readers should stop at the first NUL and must not assume the remaining bytes are meaningful (the reader forces the last byte to `0`).
- **`bitmap_size == 0` only occurs in version 1 files.** It means "the stored images are `L x L`". Version 2 writers always write the real size. Do not divide by it; resolve `S = bitmap_size ? bitmap_size : L` first.
- **Padding is not added twice.** `L` already contains `P`. The drawable content box is `L - 2P`.
- **The FreeType em size is `L - P`, not `L - 2P`.** Metrics are measured at `em = L - P`, then the glyph is re-stretched to the metric box inside the `L - 2P` content box. Do not convert metrics using `L - 2P` unless you are explicitly computing texture texel density.
- **`advance_x` / `advance_y` are 26.6 fixed point for both font and SVG input.** `horiAdvance` / `vertAdvance` are the float-pixel equivalents. Mixing them up scales advances by 64.
- **The float metric fields are the metric box.** In version 2 they are not overwritten by the fit: `metricWidth`/`metricHeight`/`horiBearingX`/`horiBearingY` describe the box the glyph was stretched from. For SVG input that box is the viewport.
- **`width` / `height` are intended layout pixels, not texture pixels.** The texture is `S x S` for every glyph in the font; the glyph's content box inside it is `S_in` texels wide. For SVG input they are the viewport size.
- **Each glyph image is independently normalised.** `0.5` is the contour in every image, but the value at distance `d` from the contour is not a constant multiple of `d`.
- **Version 2 records the nominal ranges and baked transforms.** `distanceRangeX`/`Y`, `encodingFlags` and `midpointAdjustment` are present and match `.wodi`; version 1 files have none. The range is still the nominal search radius, not an exact per-image scale.
- **Empty vs. invalid glyphs.** `valid == false` consumes one byte and is used for glyphs that could not be generated; `valid == true` with `sdfLength == 0` is a valid empty glyph such as a space. Both can appear in a TOC.
- **The TOC is the index.** Offsets are absolute from the beginning of the file; a glyph blob never needs alignment. The file must be seekable.
- **No global glyph ordering guarantee beyond the writer's `QMap` order.** Sort or index by code point if your code depends on order.
- **Versioning is strict.** Version `0` and versions above `CURRENT_VERSION` are rejected by the reference reader. The reader accepts version 1 and 2; new generation writes 2, while a loaded version 1 file stays 1. Add new fields only by bumping the version.
- **Mipmaps belong to `.wodi` only.** A `.wodf` glyph stores exactly one image; level-of-detail generation is the engine's job (or use a `.wodi`).
- **CBOR is not binary-compatible.** The CBOR key for `bitmap_size` is the legacy `dimensions`; a tool that looks for a key literally named `bitmap_size` will not find it.

## Type enumerations

### SDFType (`type`, `uint8_t`)

| Value | Name | Meaning |
|---|---|---|
| 0 | `SDF` | single-channel signed distance field |
| 1 | `MSDF` | multi-channel (RGB) signed distance field |
| 2 | `MSDFA` | multi-channel with alpha (RGBA) |

### DistanceType (`distType`, `uint8_t`)

| Value | Name | Meaning |
|---|---|---|
| 0 | `Manhattan` | L1 norm, `abs(dx) + abs(dy)` |
| 1 | `Euclidean` | L2 norm, `sqrt(dx*dx + dy*dy)` |

The generation mode (`Software`, `OpenGL`, `OpenCL`) is a runtime choice and is **not** serialized.

## Glossary

| Term | Definition |
|---|---|
| generation size / logical size (`L`) | The square resolution at which the distance field is evaluated. Padding is inside it. |
| stored size / actual size (`S`) | The square resolution of the encoded image (level 0). Version 2 writers always store the real size; a legacy version 1 `.wodf` may contain `0`, meaning "same as `L`". |
| padding (`P`) | Border on all four sides of the generation canvas, in logical pixels; part of `L`. |
| content box | The centred `(L - 2P)^2` region that holds the glyph or graphic; it is `S_in` texels in the stored image. |
| intended size | The glyph's layout box (`width` x `height`), in pixels at `em = L - P`; used to restore the aspect ratio and position on screen. |
| em (`L - P`) | The FreeType pixel size used while measuring metrics. Not stored explicitly; derive it from `L` and `P`. |
| glyph tile | One `S x S` encoded image stored in `StoredCharacter::sdf`. |
| distance field normalisation | Per-image mapping of inside/outside distances to `[0.5, 1]` / `[0, 0.5]`. |
| mip chain | The list of progressively halved images stored in `StoredVectorImage::mipmaps`. |

---

## Minimum viable runtime checklist

1. Parse the header (version 1 or 2); resolve `S = bitmap_size ? bitmap_size : bitmap_logical_size`.
2. Decode each glyph's `sdf` with `imageFormat` into an `S x S` texture (or atlas cell).
3. Keep `width`, `height`, `bearing_x`, `bearing_y`, `advance_*`, `kerning` in layout space.
4. Expand the quad by `padX = width * P / (L - 2P)` and `padY = height * P / (L - 2P)` on each side, and map the full tile UVs to the expanded quad. This applies to font and SVG glyphs alike; SVG `width`/`height` are the viewport.
5. Threshold the SDF at `0.5`; derive the antialiasing width from screen-space derivatives (`dFdx`/`dFdy`), which handles the anisotropic stretch. If you need the nominal encoded radius, use `distanceRangeX/Y` (version 2 `.wodf` and all `.wodi`).
