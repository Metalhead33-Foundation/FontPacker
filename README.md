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
- `--intendedsize 0` (the default) means **no downsampling**: the stored images are `L x L`. Files written by FontPacker record the real `S = L`; only historical version 1 files may contain `0`, which readers must resolve to `L`.
- `.wodi` always stores a concrete `actualSize` (assigned from the final image width).

### Pipeline

1. **Measure.** `FT_Set_Pixel_Sizes(face, L - P, L - P)`, so 1 em = `L - P` pixels. The face metrics (`ascender`, `descender`, `faceHeight`, `maxAdvance`), the original `unitsPerEm`, and each glyph's metrics are read here.
2. **Decompose.** The outline becomes line/quadratic/cubic segments (`FontOutlineDecompositionContext`), or the glyph is rasterised when `--forceraster`/bitmap fonts are used.
3. **Stretch into the square.** The decomposed outline is mapped onto `[P, L-P]` on both axes using the **glyph metric box**. `processOutlineGlyphEnd` passes `metricWidth`, `metricHeight`, `horiBearingX` and `horiBearingY` as `double` copies, so the metric overload of `translateToNewSize` is used and the stored metric fields are left intact: `scaleX = (L - 2P) / metricWidth`, `scaleY = (L - 2P) / metricHeight`. A narrow `i` and a wide `W` both fill the same square; texel usage is maximised at the cost of temporary anisotropy.
4. **Evaluate.** The SDF/MSDF is computed on the `L x L` grid (compute shader or CPU).
5. **Downsample.** If `S != 0`, the image is halved until the next power of two >= `S`, then smooth-resampled to exactly `S`. The result is encoded with `--format` (PNG by default) and stored as the glyph's `sdf` bytes.
6. **Record layout.** `width`, `height`, `bearing_x`, `bearing_y`, `advance_x`, `advance_y` (and `horiAdvance`, `vert*`) describe the **intended** box, not the stretched texture. In version 3 the FreeType 26.6 advances are converted to float pixels; every float metric keeps its metric-box value.
7. **Write the container.** All glyph tiles plus the TOC, kerning map and face metrics go into `.wodf` (or one graphic plus optional mip chain into `.wodi`).

### What the per-glyph metrics mean

For **font input**, every field is **pixels at `em = L - P`**. They are *not* in font design units and *not* in stored texels. Version 3 stores `advance_x` / `advance_y` as float pixels too; versions 1 and 2 stored them as raw **26.6 fixed point** (divide by 64). For **SVG input**, all numeric fields are **SVG user units**.

> **The fit uses the metric box.** `metricWidth`, `metricHeight`, `horiBearingX` and `horiBearingY` describe the box each glyph was stretched *from*; they keep their values. For SVG input that box is the viewport (`metricWidth`/`metricHeight` = viewport size, `horiBearingX` = 0, `horiBearingY` = viewport height).
>
> **SVG glyphs contain the viewport.** In an SVG-backed `.wodf`, `width`/`height`, the float metric box and `horiAdvance` are the SVG **viewport**, the bearings are 0, and the texture holds the whole viewport scaled into the content box. Shapes keep their position and relative size inside the canvas. `.wodi` additionally stores the shape bounds and the logical rectangle under explicit names.

| Field | Meaning |
|---|---|
| `width`, `height` | Font: intended glyph box size in whole pixels, **padding excluded**; the size the content box should be drawn at. SVG: the viewport size. |
| `bearing_x`, `bearing_y` | Offset from the pen origin to the left edge, and from the baseline up to the top edge, in whole pixels. Always 0 for SVG input. |
| `advance_x`, `advance_y` | Float advance in pixels (version 3); versions 1/2 store raw 26.6 integers. SVG advances are the viewport size. |
| `metricWidth`, `metricHeight` | Float metric-box size used by the fit (FreeType metrics, or the SVG viewport). |
| `horiBearingX`, `horiBearingY` | Float sub-pixel bearings of the metric box. |
| `horiAdvance` | Float advance in pixels; normally equal to `advance_x`. |
| `vertBearingX`, `vertBearingY`, `vertAdvance` | Vertical metrics; meaningful only when `hasVert` is true. |

To convert a stored pixel value of a **font** back to design units: `value_font_units = value_px * unitsPerEm / (L - P)`. Do not apply this to SVG-backed faces: there `unitsPerEm` is just the viewport height and the values are SVG user units.

> **Older files.** The table above describes version 3 output. Version 2 already uses the same metric box but stores `advance_x`/`advance_y` as raw 26.6 integers. Version 1 uses the old fit (vector metric fields hold outline bounds, SVG advances are plain pixels, SVG textures hold the shape bounds). Regenerate the asset to obtain version 3; loading and rewriting an older file keeps its original version.

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

Because the quad is expanded by the inverse of the content-box ratio, the silhouette lands exactly at `width x height` / `bearing_x`, `bearing_y`, while glows, shadows and bevels get the `P_final`-texel runway around it. The whole tile is square, but the glyph is not: the runtime must scale X and Y independently. For SVG-backed `.wodf` files `width`/`height` are the viewport, so the same formula applies. The "Restoring the glyph on screen" section of [BINARY_FORMAT.md](BINARY_FORMAT.md) shows the screen-derivative antialiasing that handles the anisotropy.

### Distance values

Both backends normalise each generated image independently: `0.5` is the contour, `1.0` is the largest inside distance in that image, `0.0` the largest outside distance. A constant `0.5` threshold therefore works for every glyph. **`.wodf` version 2 and later** store the nominal `distanceRangeX`/`Y`, the `encodingFlags` bitmask and `midpointAdjustment`; version 1 files do not, and the reader reconstructs `P * S / L`. The range is the nominal search radius, not an exact per-image scale; use it as an upper bound.

---

## Features

- **SDF flavours:** `SDF`, `MSDF`, `MSDFA`.
- **Backends:** `Software` (CPU) and `OpenGL` (compute shaders). `OpenCL` is declared but not implemented.
- **Inputs:** TrueType/OpenType fonts, SVG, `.wodf`/CBOR fonts, `.wodi`/CBOR vector images.
- **Repacking:** loaded `.wodf`/`.wodi` containers can be re-encoded (`--format`) and rescaled (`--intendedsize`); unsupported generation options are rejected instead of ignored.
- **Formats:** `.wodf` version 3 (versions 1 and 2 are still readable), `.wodi` version 1.
- **Outputs:** `.wodf`, CBOR font, individual glyph image files, `.wodi`, CBOR vector image.
- **Distance metrics:** Manhattan (L1) and Euclidean (L2).
- **Extras (OpenGL backend unless noted):** msdfgen-style edge colouring, invert, gamma correction, midpoint adjustment, max-downsampling (both backends), encoded mip chains for `.wodi`.

## Requirements

| Dependency | Notes |
|---|---|
| Qt 6 | `core gui opengl widgets openglwidgets core5compat` (split across `lib/` and `app/`) |
| FreeType 2 | outline loading, metrics, raster fallback |
| HarfBuzz | linked by the library (`harfbuzz/hb-ft.h`) |
| libsvgtiny | SVG parsing |
| GLM | vector math in the generation code |
| OpenMP | the software SDF loop is parallelised with `#pragma omp` |
| Catch2 3 | unit tests (`pkg-config --cflags --libs catch2`) |
| Doxygen (optional) | only for the API documentation; see below |

### Source layout

```
FontPacker.pro           # qmake subdirs project: lib, app, tests
lib/                     # FontPackerCore static library (all non-GUI/CLI code + shaders)
app/                     # GUI and command-line front end
tests/                   # Catch2 unit tests
```

### Building

The project uses qmake. Build out-of-source under `build/`:

```bash
mkdir -p build
cd build
/usr/bin/qmake6 ../FontPacker.pro
env MAKE=/usr/bin/make /usr/bin/make -j$(nproc) QMAKE=/usr/bin/qmake6
./app/FontPacker             # GUI mode
./app/FontPacker --inspect some.wodf
env MAKE=/usr/bin/make /usr/bin/make check QMAKE=/usr/bin/qmake6  # Catch2 unit tests
```

`lib/FontPackerLib.pro` builds the `FontPackerCore` static library. `app/FontPackerApp.pro` links it along with `-lfreetype -lharfbuzz -lsvgtiny -fopenmp`. Defining `HIRES` changes the built-in defaults from `L=1024, P=100` to `L=4096, P=400` (see `lib/SDFGenerationArguments.cpp`).

The OpenGL shaders are compiled into the **library** as Qt resources. The executable calls `Q_INIT_RESOURCE(resources)` so the static resource object is linked and registered before `SdfGenerationGL` or `OpenGLCanvas` opens `:/shader*.glsl`.

### Unit tests

`tests/FontPackerTests.pro` builds a Catch2 3 fixture linked against `FontPackerCore` and uses the system Catch2 from `pkg-config --cflags --libs catch2`. The tests exercise binary and CBOR serialization of `StoredCharacter`, `PreprocessedFontFace` and `StoredVectorImage`, including version 1/2/3 compatibility, float/26.6 advances, legacy SVG advances, invalid glyphs and in-memory repacking. All serialization tests use `QBuffer`, so no temporary files are written; `QFile` and `QBuffer` share the same `QIODevice` interface. Run them with `make check` from the shadow build directory, or execute `tests/FontPackerTests` directly with a Catch2 reporter.

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

### Repacking stored containers

A stored `.wodf`/`.wodi` no longer has the outlines or raw distances, so it cannot be regenerated, but the following options are applied to loaded input:

| Option | Effect on loaded input |
|---|---|
| `--format <name>` | Decode every stored image and re-encode it with the new codec. |
| `--intendedsize <n>` | Resample every glyph tile (`.wodf`) or level 0 and the mip chain (`.wodi`) to `n x n`; `0` keeps the current size. |
| `--createmipmaps` | `.wodi` only: rebuild the halving chain from level 0 down to 1x1. |
| `--outfont <pattern>` | `.wodf` only: write the (possibly re-encoded) payloads to files. |

`--format` and `--intendedsize` are the only processing options that make sense; every generation option (`--mode`, `--type`, `--dist`, `--padding`, `--internalprocesssize`, `--samplestocheck*`, `--mincharcode`, `--maxcharcode`, `--forceraster`, `--invert`, `--gammacorrect`, `--midpointadjustment`, `--maximizeinsteadofaverage`, `--msdfgencoloring`) is **rejected with an error** when the input is a stored container instead of being silently ignored. A loaded version 1 or 2 `.wodf` keeps its original version, because its glyph layout/advance encoding follows the older semantics; regenerate the asset to obtain version 3.

### Inspecting a stored container

```bash
FontPacker --inspect font.wodf
FontPacker --inspect image.wodi
FontPacker --inspect font.cbor
```

`--inspect <path>` parses `.wodf`, `.wodi`, binary or CBOR containers and prints the header (version, `S`/`L`/`P`, face metrics, distance ranges and encoding flags) plus one line per glyph with bearings, float advances, metric box and encoded payload size. It implies command-line mode (no `--nogui` needed), writes nothing else, and cannot be combined with generation or output options. It is the built-in equivalent of the reference parser in [BINARY_FORMAT.md](BINARY_FORMAT.md).

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
| `--mincharcode <n>` | integer | First Unicode code point to process (inclusive). | `0` |
| `--maxcharcode <n>` | integer | One past the last code point to process (exclusive). | `0xE007F` |
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

> **Code-point range.** The loop is half-open: `for (c = mincharcode; c < maxcharcode; ++c)`. To emit `A` and `B` (U+0041, U+0042), use `--mincharcode 65 --maxcharcode 67`. Leave both unset to process the default range `0 .. 0xE007E`.

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

The command-line tool merges all shapes into a single glyph entry stored under code point 0 (`SvgTreatment::ShapesAllInOne`; no option selects per-shape output). `width`/`height`, the metric box and `horiAdvance` are the SVG viewport size; `unitsPerEm` is the viewport height. The texture contains the whole viewport, so shapes keep their position inside the canvas.

### E. Convert and repack stored containers

```bash
# Binary -> CBOR and back (container rewrite; version 1 stays 1)
FontPacker --nogui --inbin font.wodf --outcbor font.cbor
FontPacker --nogui --incbor font.cbor --outbin font.wodf

# Re-encode and rescale glyph payloads without regenerating distances
FontPacker --nogui --inbin font.wodf --format WEBP --intendedsize 32 --outbin font_small.wodf

# Stored vector image binary -> CBOR, with a rebuilt mip chain
FontPacker --nogui --invectorbin image.wodi --createmipmaps --outvectorcbor image.vcbor

# Split a font into individual encoded glyph files
FontPacker --nogui --inbin font.wodf --outfont glyphs/glyph_%1.png
```

Conversions rewrite the container. `--format`, `--intendedsize` and (for `.wodi`) `--createmipmaps` are applied to loaded input as described above; all other processing options are rejected, because no pixel data is regenerated.

---

## Output formats

The complete specification, with offsets, formulas and a Python reference parser, is in [BINARY_FORMAT.md](BINARY_FORMAT.md). In short:

| Container | Magic | Extension | Contents |
|---|---|---|---|
| `PreprocessedFontFace` | `WODF` | `.wodf` | face metrics, glyph TOC, kerning, one `S x S` encoded image per glyph (version 3; versions 1/2 readable) |
| `StoredVectorImage` | `WODI` | `.wodi` | one graphic, its source bounds, distance ranges, optional mip chain |

The CBOR equivalents carry a `containerType` string (`"PreprocessedFontFace"` or `"StoredVectorImage"`). The `type` field in both is still the SDF flavour (`SDF`/`MSDF`/`MSDFA`), not the container discriminator.

Important points for consumers:

- All multi-byte values are Big-Endian; `bool` is 1 byte; strings are length-prefixed UTF-8 with a NUL included in the length.
- `bitmap_size` is always written as the real stored size. Only historical version 1 files may contain `0`, meaning `bitmap_logical_size x bitmap_logical_size`.
- Padding is **inside** the logical size; the content box is `L - 2P`.
- Each glyph tile is square and stretched; use `width`/`height` to restore the aspect ratio for both font and SVG input.
- `advance_x`/`advance_y` are float pixels in version 3; versions 1/2 store raw 26.6 integers (version 1 SVG: plain pixels). All other metrics are floats (SVG: user units).
- `.wodf` versions 2/3 store the nominal distance ranges, encoding flags and midpoint adjustment, like `.wodi`; version 1 files do not.

---

## Known limitations and issues

These are current properties of the code, documented so that users and automated tools do not make wrong assumptions.

1. **Software mode cannot generate from outlines.** `SdfGenerationContextSoft::produceOutlineSdf()` is unimplemented (empty function body). Outline fonts and SVG therefore require `--mode OpenGL`; TrueType/OpenType fonts can be used with Software only together with `--forceraster`.
2. **Software mode emits single-channel SDFs only.** `SdfGenerationContextSoft::produceBitmapSdf()` always builds a `QImage::Format_Grayscale8`; `--type MSDF`/`MSDFA` needs the OpenGL backend.
3. **OpenCL is declared but not implemented.** `--mode OpenCL` throws `Unsupported mode!`.
4. **Each image is normalised independently.** A value of `0.6` does not mean the same pixel distance in two different glyphs; `distanceRangeX/Y` is only the nominal search radius. This is accepted by design.
5. **`imageFormat` is limited to 7 characters.** Longer codec names are truncated and zero padded. This is accepted by design.
6. **HIRES changes defaults silently.** With `DEFINES += HIRES`, `L` defaults to 4096 and `P` to 400 instead of 1024/100. This is accepted by design.
7. **The Software backend ignores value transforms.** `--invert`, `--gammacorrect` and `--midpointadjustment` are only implemented in `SdfGenerationGL::fetchSdfFromGPU` / `fetchMSDFFromGPU`. The software distance pass normalises but does not transform the values.

---

## License

See [LICENSE](LICENSE).
