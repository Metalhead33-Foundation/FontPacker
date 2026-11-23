# FontPacker

FontPacker is an offline packer that turns TrueType/OpenType fonts and SVG files into signed distance field (SDF) assets:

- single-channel **SDF**, multi-channel **MSDF**, and **MSDFA** (MSDF with alpha);
- CPU (software) and OpenGL 4.3+ compute-shader generation;
- binary `.wodf` / `.wodi` containers and their CBOR equivalents;
- FreeType-style layout metrics stored next to every glyph, so the runtime never needs FreeType.

| Document | Contents |
|---|---|
| **README.md** (this file) | what the tool does, how a package is built, build/run instructions, option reference |
| [BINARY_FORMAT.md](BINARY_FORMAT.md) | complete byte-level specification of `.wodf` and `.wodi`, geometry formulas, metric provenance, CBOR mapping, gotchas |
| Doxygen site | API reference generated from the headers (see the Documentation section below) |

---

## How a font package is built (read this first)

This is the part that is most often misunderstood. The same explanation, with byte offsets and worked examples, is in [BINARY_FORMAT.md](BINARY_FORMAT.md) (section "Raster geometry").

### Three sizes, three jobs

| Symbol | Option | `.wodf` field | `.wodi` field | Meaning |
|---|---|---|---|---|
| `L` | `--internalprocesssize` | `bitmap_logical_size` | `processingSize` | Square resolution at which the distance field is **generated**. |
| `S` | `--intendedsize` | `bitmap_size` | `actualSize` | Square resolution of the image that is **stored**, after downsampling. |
| `P` | `--padding` | `bitmap_padding` | `padding` | Border on **every side**, in pixels of the `L` grid. **Part of `L`.** |

```
L x L generation canvas                    S x S stored image
+---------------------------+              +-------------+
|             P             |              |  P_final    |
|   +-------------------+   |              |  +-------+  |
| P |  glyph content    | P |  --k=S/L-->  |  |content|  |
|   |  stretched to fit |   |              |  +-------+  |
|   +-------------------+   |              |  P_final    |
|             P             |              +-------------+
+---------------------------+
```

- The content box is `(L - 2P) x (L - 2P)` **inside** `L`. Padding is never added on top of `L`.
- After downsampling by `k = S / L`, the content box is `S_in = (L - 2P) * S / L` texels and the padding is `P_final = P * S / L` texels (both may be fractional).
- `S == 0` (the default `--intendedsize`) means **no downsampling**: the stored images are `L x L`. In `.wodf` the field is literally written as `0`; readers must resolve it to `L`.
- `.wodi` always stores a concrete `actualSize` (assigned from the final image width).

### Pipeline

1. **Measure.** `FT_Set_Pixel_Sizes(face, L - P, L - P)`, so 1 em = `L - P` pixels. The face metrics (`ascender`, `descender`, `faceHeight`, `maxAdvance`), the original `unitsPerEm`, and each glyph's metrics are read here.
2. **Decompose.** The outline becomes line/quadratic/cubic segments (`FontOutlineDecompositionContext`), or the glyph is rasterised when `--forceraster`/bitmap fonts are used.
3. **Stretch into the square.** The decomposed outline is mapped onto `[P, L-P]` on both axes. **The current code fits the outline's own control-point bounds**, not the FreeType metric box: the call `translateToNewSize(L, L, P, P, output.metricWidth, output.metricHeight, output.horiBearingX, output.horiBearingY)` passes `float` lvalues, so overload resolution picks the bounds-capturing overload instead of the metric one. The scale is per-axis: `scaleX = (L - 2P) / (maxX - minX)`, `scaleY = (L - 2P) / (maxY - minY)`. A narrow `i` and a wide `W` both fill the same square; texel usage is maximised at the cost of temporary anisotropy. Because the bounds are written back into the four float fields, see the metric caveat below.
4. **Evaluate.** The SDF/MSDF is computed on the `L x L` grid (compute shader or CPU).
5. **Downsample.** If `S != 0`, the image is halved until the next power of two >= `S`, then smooth-resampled to exactly `S`. The result is encoded with `--format` (PNG by default) and stored as the glyph's `sdf` bytes.
6. **Record layout.** `width`, `height`, `bearing_x`, `bearing_y`, `advance_x`, `advance_y` (and `horiAdvance`, `vert*`) are stored unchanged from step 1; they describe the **intended** box, not the stretched texture. The four float fields `metricWidth`, `metricHeight`, `horiBearingX`, `horiBearingY` are overwritten by step 3 for vector glyphs — see the caveat below.
7. **Write the container.** All glyph tiles plus the TOC, kerning map and face metrics go into `.wodf` (or one graphic plus optional mip chain into `.wodi`).

### What the per-glyph metrics mean

For **font input**, all float fields are **pixels at `em = L - P`**. They are *not* in font design units and *not* in stored texels. The integer fields are the exception: `advance_x` / `advance_y` are raw **26.6 fixed point** (divide by 64). For **SVG input**, all numeric fields are **SVG user units** and the integer advances are plain viewport pixels. Four of the float fields change meaning for vector glyphs; see the caveat below.

> **Metric caveat (current code).** For glyphs built from outlines or SVG, `metricWidth`/`metricHeight`/`horiBearingX`/`horiBearingY` are clobbered by the fit step and hold the outline `minX`/`maxX`/`minY`/`maxY` instead of a metric box. Only glyphs generated with `--forceraster` keep the FreeType metric values in those four fields. Use `width`, `height`, `bearing_x`, `bearing_y` and `horiAdvance` for layout.
>
> **SVG special case.** In an SVG-backed `.wodf`, `width`/`height` and `advance_x`/`advance_y` are the SVG **viewport**, while the texture holds the **shape bounds** stretched to fill it. The four clobbered float fields then double as `minX`/`maxX`/`minY`/`maxY`, i.e. the rectangle the content box represents. Draw the content box at `(maxX - minX) x (maxY - minY)` inside the viewport instead of at `width x height`. `.wodi` stores this rectangle under explicit names and is preferable for SVG assets.

| Field | Meaning |
|---|---|
| `width`, `height` | Font: intended glyph box size in whole pixels, **padding excluded**; the size the content box should be drawn at. SVG: the viewport size, **not** the shape size. |
| `bearing_x`, `bearing_y` | Offset from the pen origin to the left edge, and from the baseline up to the top edge, in whole pixels. Always 0 for SVG input. |
| `advance_x`, `advance_y` | Advance in 26.6 fixed point pixels. SVG: plain viewport pixels (not 26.6). |
| `metricWidth`, `metricHeight` | Raster glyphs: float metric-box size. Vector glyphs: **overwritten with `minX` / `maxX` of the outline bounds** (see caveat). |
| `horiBearingX`, `horiBearingY` | Raster glyphs: float sub-pixel bearings. Vector glyphs: **overwritten with `minY` / `maxY`** (see caveat). |
| `horiAdvance` | Float advance in pixels (`advance_x / 64`). |
| `vertBearingX`, `vertBearingY`, `vertAdvance` | Vertical metrics; meaningful only when `hasVert` is true. |

To convert a stored pixel value of a **font** back to design units: `value_font_units = value_px * unitsPerEm / (L - P)`. Do not apply this to SVG-backed faces: there `unitsPerEm` is just the viewport height and the values are SVG user units.

### Drawing a glyph

The runtime maps the **whole** tile to a quad that is expanded beyond the intended box so the padding runway is visible:

```
padX = width  * P / (L - 2P)
padY = height * P / (L - 2P)

quadTopLeft = (pen.x + bearing_x - padX, pen.y - bearing_y - padY)
quadSize    = (width + 2 * padX,        height + 2 * padY)

uv(0,0) -> quadTopLeft        // row 0 of the tile is its top row
uv(1,1) -> quadTopLeft + quadSize
```

Because the quad is expanded by the inverse of the content-box ratio, the silhouette lands exactly at `width x height` / `bearing_x`, `bearing_y`, while glows, shadows and bevels get the `P_final`-texel runway around it. The whole tile is square, but the glyph is not: the runtime must scale X and Y independently. For SVG-backed `.wodf` files the content box is the shape bounds rather than `width x height`; see the SVG special case above. The "Restoring the glyph on screen" section of [BINARY_FORMAT.md](BINARY_FORMAT.md) has the per-axis antialiasing formula.

### Distance values

Both backends normalise each generated image independently: `0.5` is the contour, `1.0` is the largest inside distance in that image, `0.0` the largest outside distance. A constant `0.5` threshold therefore works for every glyph. `.wodf` does **not** store a distance range or the baked invert/gamma/midpoint settings; `.wodi` stores `distanceRangeX`/`Y` and `encodingFlags`. `P * S / L` is the nominal upper bound of the encoded radius in `.wodf`.

---

## Features

- **SDF flavours:** `SDF`, `MSDF`, `MSDFA`.
- **Backends:** `Software` (CPU) and `OpenGL` (compute shaders). `OpenCL` is declared but not implemented.
- **Inputs:** TrueType/OpenType fonts, SVG, `.wodf`/CBOR fonts, `.wodi`/CBOR vector images.
- **Outputs:** `.wodf`, CBOR font, individual glyph image files, `.wodi`, CBOR vector image.
- **Distance metrics:** Manhattan (L1) and Euclidean (L2).
- **Extras (OpenGL backend unless noted):** msdfgen-style edge colouring, invert, gamma correction, midpoint adjustment, max-downsampling (both backends), encoded mip chains for `.wodi`.

## Requirements

| Dependency | Notes |
|---|---|
| Qt 6 | `core gui opengl widgets openglwidgets core5compat` (see `FontPacker.pro`) |
| FreeType 2 | outline loading, metrics, raster fallback |
| HarfBuzz | linked by the project (`harfbuzz/hb-ft.h`) |
| libsvgtiny | SVG parsing |
| GLM | vector math in the generation code |
| OpenMP | the software SDF loop is parallelised with `#pragma omp` |
| Doxygen (optional) | only for the API documentation; see below |

### Building

The project uses qmake:

```bash
qmake6 FontPacker.pro      # or `qmake` on distributions where qmake is Qt 6
make -j$(nproc)
./FontPacker               # GUI mode
```

`FontPacker.pro` sets `CONFIG += c++2a cmdline` and links `-lfreetype -lharfbuzz -lsvgtiny -fopenmp`. Defining `HIRES` changes the built-in defaults from `L=1024, P=100` to `L=4096, P=400` (see `SDFGenerationArguments.cpp`).

> Note: `FontPacker.pro` contains `INCLUDEPATH += I/usr/include/harfbuzz` (a missing `-`). It is harmless when HarfBuzz is installed under `/usr/include`, but should be `-I/usr/include/harfbuzz` if your headers live elsewhere.

### Documentation

All headers are documented with Doxygen's Qt dialect (`/** ... */`, `///`, `\brief`, `\param`, `\return`, `\see`). The reference is generated from the headers and the two Markdown documents:

```bash
git submodule update --init --recursive   # fetches doxygen-awesome-css
doxygen Doxyfile                          # writes build/doxygen/html
xdg-open build/doxygen/html/index.html
```

`Doxyfile` uses the `doxygen-awesome-css` submodule for the theme (base + sidebar-only CSS, `HTML_COLORSTYLE = LIGHT` as required by the theme). The GitHub Actions workflow `.github/workflows/doxygen.yml` renders the same site on every push to `master`, uploads it as the `doxygen-html` artifact, and deploys it to GitHub Pages.

---

## Command-line usage

Without arguments FontPacker opens its Qt GUI. With the `--nogui` flag it runs as a command-line tool:

```bash
FontPacker --nogui [input options] [output options] [generation options]
```

> The flag must be written with two dashes (`--nogui`). The argument parser stores a key only for tokens that start with `--`; a bare `nogui` is silently ignored and the GUI starts. If no input is given in command-line mode, the program prints the parsed arguments and exits.

### Input options

Specify one input source:

| Argument | Description | Example |
|---|---|---|
| `--infont <path>` | Load a TrueType/OpenType font | `--infont /path/to/font.ttf` |
| `--insvg <path>` | Load an SVG file | `--insvg /path/to/shapes.svg` |
| `--inbin <path>` | Load a preprocessed font (`.wodf`) | `--inbin font.wodf` |
| `--incbor <path>` | Load a preprocessed font from CBOR | `--incbor font.cbor` |
| `--invectorbin <path>` | Load a `.wodi` vector image | `--invectorbin image.wodi` |
| `--invectorcbor <path>` | Load a vector image from CBOR | `--invectorcbor image.vcbor` |

An SVG loaded with `--insvg` is written through the **font-face** path when the output is `--outbin`, `--outcbor` or `--outfont`; it is written as a **standalone vector image** when the output is `--outvectorbin` or `--outvectorcbor`. With neither kind of output, the font-face path is used.

### Output options

| Argument | Description | Example |
|---|---|---|
| `--outbin <path>` | Write `.wodf` | `--outbin output.wodf` |
| `--outcbor <path>` | Write CBOR font | `--outcbor output.cbor` |
| `--outfont <pattern>` | Export each glyph's raw `sdf` bytes; `%1` is the code point | `--outfont glyph_%1.png` |
| `--outvectorbin <path>` | Write `.wodi` | `--outvectorbin image.wodi` |
| `--outvectorcbor <path>` | Write CBOR vector image | `--outvectorcbor image.vcbor` |

`--outfont` writes the encoded bytes exactly as stored; it does not re-encode, so the file extension in the pattern should match `--format` (the default payload is PNG).

### Generation options

| Argument | Values | Description | Default |
|---|---|---|---|
| `--mode` | `Software`, `OpenGL`, `OpenCL` | Backend. `OpenCL` throws at runtime. | `Software` |
| `--type` | `SDF`, `MSDF`, `MSDFA` | Distance-field flavour. | `SDF` |
| `--dist` | `Manhattan`, `Euclidean` | Distance norm. | `Manhattan` for Software, `Euclidean` for OpenGL |
| `--internalprocesssize <n>` | integer | `L`: generation square size. | `1024` (`4096` with `HIRES`) |
| `--intendedsize <n>` | integer | `S`: stored square size after downsampling. `0` disables downsampling. | `0` |
| `--padding <n>` | integer | `P`: border on each side, inside `L`. Must satisfy `2P < L`. | `100` (`400` with `HIRES`) |
| `--samplestocheckx <n>` | integer | Search radius in X for the SDF pass. `0` uses `P`. | `0` |
| `--samplestochecky <n>` | integer | Search radius in Y. `0` uses `P`. | `0` |
| `--mincharcode <n>` | integer | **See the warning below.** | `0` |
| `--format <name>` | string | Encoded image format passed to `QImage::save`. Upper-cased, max 7 characters. | `PNG` |
| `--midpointadjustment <v>` | float | Bakes `clamp(value / v, 0, 1)` into the SDF. `v` must be positive. **OpenGL backend only.** | unset |

Flags (presence enables them; they take no value):

| Flag | Effect |
|---|---|
| `--forceraster` | Rasterise glyphs and run the bitmap SDF path instead of the outline path. **Required for fonts in Software mode** (see limitations). |
| `--invert` | Invert the SDF values (`1 - x`). **OpenGL backend only.** |
| `--gammacorrect` | Apply the cubic gamma adjustment around 0.5. **OpenGL backend only.** |
| `--maximizeinsteadofaverage` | Use max instead of average when halving images. |
| `--msdfgencoloring` | Use the msdfgen-style edge colour assignment for MSDF. Requires the OpenGL backend. |
| `--createmipmaps` | Store every halving level in a `.wodi`. Ignored for `.wodf`. |

> **`--mincharcode` warning.** In the current code `CHAR_MAX_KEY` is also the string `"mincharcode"` (`ConstStrings.cpp`), so any use of the option sets *both* bounds to the same value; the half-open loop `for (c = min; c < max; ++c)` then produces **no glyphs**. Leave the option unset to process the built-in range `0 .. 0xE007E`. This is a known bug, not a documented range syntax.

### Option interactions worth knowing

- `--intendedsize` changes only the stored resolution, never the generation resolution. A stored size larger than `L` is pointless; a very small `S` with a large `P` can make `P_final` exceed `S/2`, which leaves no content box. Keep `P < L/2` and prefer `P` to be a multiple of `L/S`.
- `--padding` is the SDF search radius as well as the geometric border: `samples_to_check_*`, when set, replace it for the search, but the geometry always uses `P`.
- `--format` selects the image codec only; quality/compression settings are Qt defaults.
- `--invert`, `--gammacorrect` and `--midpointadjustment` are applied by the OpenGL readback path only; the Software backend silently ignores them (verified: identical output files with and without the flags). `--maximizeinsteadofaverage` is shared by both backends.
- `--type MSDF`/`MSDFA` is not implemented by the software backend at all; use `--mode OpenGL`.

---

## Examples

### A. Font to `.wodf`, software backend

```bash
FontPacker --nogui --infont /usr/share/fonts/truetype/arial.ttf \
  --mode Software --type SDF --forceraster \
  --internalprocesssize 1024 --intendedsize 64 --padding 96 \
  --outbin arial_sdf.wodf
```

`--forceraster` is required: the software backend has no outline renderer. `P = 96` is a multiple of `L / S = 16`, so the content edge lands on a stored texel boundary.

### B. Font to `.wodf`, GPU MSDF

```bash
FontPacker --nogui --infont font.ttf \
  --mode OpenGL --type MSDF --dist Euclidean \
  --internalprocesssize 2048 --intendedsize 64 --padding 128 \
  --msdfgencoloring \
  --outbin font_msdf.wodf
```

### C. SVG to a standalone `.wodi` with a mip chain

```bash
FontPacker --nogui --insvg icon.svg \
  --mode OpenGL --type MSDF \
  --internalprocesssize 1024 --intendedsize 128 --padding 64 \
  --createmipmaps \
  --outvectorbin icon_sdf.wodi
```

SVG processing needs the OpenGL backend, because the software outline renderer is not implemented.

### D. SVG stored as a font face

```bash
FontPacker --nogui --insvg shapes.svg \
  --mode OpenGL --type SDF \
  --internalprocesssize 1024 --intendedsize 32 \
  --outbin shapes_sdf.wodf
```

The command-line tool merges all shapes into a single glyph entry stored under code point 0 (`SvgTreatment::ShapesAllInOne`; no option selects per-shape output). `width`/`height` are the SVG viewport size; `unitsPerEm` is the viewport height. The texture contains the shape bounds, not the viewport; see the SVG special case above.

### E. Convert between containers

```bash
# Binary -> CBOR and back (no regeneration, no re-encoding)
FontPacker --nogui --inbin font.wodf --outcbor font.cbor
FontPacker --nogui --incbor font.cbor --outbin font.wodf

# Stored vector image binary -> CBOR
FontPacker --nogui --invectorbin image.wodi --outvectorcbor image.vcbor

# Split a font into individual encoded glyph files
FontPacker --nogui --inbin font.wodf --outfont glyphs/glyph_%1.png
```

Conversions only rewrite the container. Options such as `--format`, `--gammacorrect`, `--invert` or `--midpointadjustment` are applied **when generating** from `--infont`/`--insvg`; they are ignored when the input is a stored `.wodf`/`.wodi`, because no pixel data is regenerated.

---

## Output formats

The complete specification, with offsets, formulas and a Python reference parser, is in [BINARY_FORMAT.md](BINARY_FORMAT.md). In short:

| Container | Magic | Extension | Contents |
|---|---|---|---|
| `PreprocessedFontFace` | `WODF` | `.wodf` | face metrics, glyph TOC, kerning, one `S x S` encoded image per glyph |
| `StoredVectorImage` | `WODI` | `.wodi` | one graphic, its source bounds, distance ranges, optional mip chain |

The CBOR equivalents carry a `containerType` string (`"PreprocessedFontFace"` or `"StoredVectorImage"`). The `type` field in both is still the SDF flavour (`SDF`/`MSDF`/`MSDFA`), not the container discriminator.

Important points for consumers:

- All multi-byte values are Big-Endian; `bool` is 1 byte; strings are length-prefixed UTF-8 with a NUL included in the length.
- `bitmap_size == 0` means "the stored images are `bitmap_logical_size x bitmap_logical_size`".
- Padding is **inside** the logical size; the content box is `L - 2P`.
- Each glyph tile is square and stretched; use `width`/`height` to restore the aspect ratio for font input (SVG-backed faces use the shape bounds, see above).
- `advance_x`/`advance_y` are 26.6 fixed point for font input (plain pixels for SVG input); all other metrics are float pixels at `em = L - P`.
- `.wodf` stores neither a distance range nor the baked transform flags; `.wodi` stores both.

---

## Known limitations and issues

These are current properties of the code, documented so that users and automated tools do not make wrong assumptions.

1. **Software mode cannot generate from outlines.** `SdfGenerationContextSoft::produceOutlineSdf()` is unimplemented (empty function body). Outline fonts and SVG therefore require `--mode OpenGL`; TrueType/OpenType fonts can be used with Software only together with `--forceraster`.
2. **Software mode emits single-channel SDFs only.** `SdfGenerationContextSoft::produceBitmapSdf()` always builds a `QImage::Format_Grayscale8`; `--type MSDF`/`MSDFA` needs the OpenGL backend.
3. **OpenCL is declared but not implemented.** `--mode OpenCL` throws `Unsupported mode!`.
4. **`--mincharcode` is broken.** `CHAR_MAX_KEY` is the same string as `CHAR_MIN_KEY`, so the option sets both bounds and the half-open loop processes zero glyphs.
5. **Loaded files are not regenerated.** `--inbin`, `--incbor`, `--invectorbin`, `--invectorcbor` only parse and re-serialise; processing options are ignored.
6. **`bitmap_size` may be zero.** `--intendedsize 0` (the default) disables downsampling and writes `0` into the header.
7. **No distance range or encoding flags in `.wodf`.** Per-image normalisation makes `0.5` universal, but absolute distance scaling is not stored. `.wodi` is the richer container.
8. **Each image is normalised independently.** A value of `0.6` does not mean the same pixel distance in two different glyphs.
9. **Anisotropic generations need per-axis antialiasing.** The glyph is stretched into the square, so a shader that uses one isotropic AA width will make outlines thicker along one axis.
10. **`imageFormat` is limited to 7 characters.** Longer codec names are truncated and zero padded.
11. **HIRES changes defaults silently.** With `DEFINES += HIRES`, `L` defaults to 4096 and `P` to 400 instead of 1024/100.
12. **`FontPacker.pro` has an `INCLUDEPATH` typo** (`I/usr/include/harfbuzz` instead of `-I...`); it only matters on systems where HarfBuzz is not under `/usr/include`.
13. **Vector glyphs clobber four float metric fields.** `metricWidth`/`metricHeight`/`horiBearingX`/`horiBearingY` receive the outline bounds (`minX`/`maxX`/`minY`/`maxY`) instead of FreeType metrics, because the generator's call binds to the bounds-capturing overload of `translateToNewSize`. Raster glyphs are unaffected. The integer fields and `horiAdvance` remain correct.
14. **SVG-backed glyphs mix viewport and bounds.** `processSvg` writes the viewport width/height into `width`/`height` and `advance_x`/`advance_y` (the latter as plain pixels, not 26.6), while the texture contains the shape bounds. Recover the bounds from `metricWidth`=`minX`, `metricHeight`=`maxX`, `horiBearingX`=`minY`, `horiBearingY`=`maxY`, or use `.wodi`, which stores them explicitly.
15. **The Software backend ignores value transforms.** `--invert`, `--gammacorrect` and `--midpointadjustment` are only implemented in `SdfGenerationGL::fetchSdfFromGPU` / `fetchMSDFFromGPU`. The software distance pass normalises but does not transform the values.

---

## License

See [LICENSE](LICENSE).
