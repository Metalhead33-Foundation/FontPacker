# FontPacker

FontPacker is a tool for generating signed distance fields (SDF) from fonts and SVG files. It supports both single-channel and multi-channel SDF generation, with CPU and GPU-accelerated rendering options.

## Features

- **Multiple SDF Types**: Standard SDF, Multi-channel SDF (MSDF), and MSDF with Alpha (MSDFA)
- **Rendering Modes**: CPU-based software rendering and GPU-accelerated OpenGL compute shaders
- **Input Formats**: TrueType/OpenType fonts, SVG files, and preprocessed binary/CBOR formats
- **Output Formats**: Binary QDataStream format, CBOR format, and individual glyph files
- **Distance Metrics**: Manhattan (L1) and Euclidean (L2) distance calculations

## Command-Line Usage

FontPacker can be run in two modes:
- **GUI Mode** (default): Launches a graphical user interface
- **Command-Line Mode**: Use the `nogui` argument to run without GUI

### Basic Syntax

```bash
fontpacker [--nogui] [input options] [output options] [generation options]
```

### Input Options

Specify one input source:

| Argument | Description | Example |
|----------|-------------|---------|
| `--infont <path>` | Load font from file (TrueType/OpenType) | `--infont /path/to/font.ttf` |
| `--insvg <path>` | Load SVG file | `--insvg /path/to/shapes.svg` |
| `--inbin <path>` | Load preprocessed font from binary format | `--inbin font.bin` |
| `--incbor <path>` | Load preprocessed font from CBOR format | `--incbor font.cbor` |

**Note**: If no input is specified and `nogui` is used, the application will exit.

### Output Options

Specify one or more output destinations:

| Argument | Description | Example |
|----------|-------------|---------|
| `--outbin <path>` | Save preprocessed font in binary format | `--outbin output.bin` |
| `--outcbor <path>` | Save preprocessed font in CBOR format | `--outcbor output.cbor` |
| `--outfont <pattern>` | Export individual glyph SDF files | `--outfont glyph_%1.bin` |

The `--outfont` pattern uses `%1` as a placeholder for the Unicode code point.

### Generation Mode Options

| Argument | Value | Description | Default |
|----------|-------|-------------|---------|
| `--mode` | `Software` | CPU-based software rendering | `Software` |
| `--mode` | `OpenGL` | GPU-accelerated OpenGL compute shaders | |
| `--mode` | `OpenCL` | OpenCL rendering (not implemented) | |

**Examples:**
```bash
--mode Software
--mode OpenGL
```

### SDF Type Options

| Argument | Value | Description | Default |
|----------|-------|-------------|---------|
| `--type` | `SDF` | Single-channel signed distance field | `SDF` |
| `--type` | `MSDF` | Multi-channel SDF (RGB channels) | |
| `--type` | `MSDFA` | Multi-channel SDF with alpha (RGBA) | |

**Examples:**
```bash
--type SDF
--type MSDF
--type MSDFA
```

### Distance Metric Options

| Argument | Value | Description | Default |
|----------|-------|-------------|---------|
| `--dist` | `Manhattan` | Manhattan distance (L1 norm: \|x\| + \|y\|) | `Manhattan` (Software), `Euclidean` (OpenGL) |
| `--dist` | `Euclidean` | Euclidean distance (L2 norm: √(x² + y²)) | |

**Examples:**
```bash
--dist Manhattan
--dist Euclidean
```

### Dimension and Quality Options

| Argument | Type | Description | Default |
|----------|------|-------------|---------|
| `--internalprocesssize <n>` | Integer | Internal processing resolution in pixels | `1024` (or `4096` if HIRES defined) |
| `--intendedsize <n>` | Integer | Final output size in pixels | `0` (uses internal size) |
| `--padding <n>` | Integer | Padding around glyphs in pixels | `100` (or `400` if HIRES defined) |
| `--samplestocheckx <n>` | Integer | Number of samples to check in X direction | `0` (uses padding) |
| `--samplestochecky <n>` | Integer | Number of samples to check in Y direction | `0` (uses padding) |

**Examples:**
```bash
--internalprocesssize 2048
--intendedsize 64
--padding 200
--samplestocheckx 50
--samplestochecky 50
```

### Character Range Options

| Argument | Type | Description | Default |
|----------|------|-------------|---------|
| `--mincharcode <n>` | Integer | Minimum Unicode code point to process | `0` |
| `--mincharcode <n>` | Integer | Maximum Unicode code point to process | `0xE007F` |

**Note**: There appears to be a bug where `--mincharcode` is used for both min and max. Use numeric values to specify ranges.

**Examples:**
```bash
--mincharcode 32    # Start from space character
--mincharcode 126   # End at tilde (for ASCII range)
```

### Processing Options (Flags)

These options are boolean flags (presence enables the option):

| Argument | Description |
|----------|-------------|
| `--jpeg` | Compress SDF bitmap data as JPEG |
| `--forceraster` | Force rasterization instead of vector processing |
| `--invert` | Invert the SDF (inside becomes outside) |
| `--gammacorrect` | Apply gamma correction |
| `--maximizeinsteadofaverage` | Use maximum instead of average when downsampling |
| `--msdfgencoloring` | Use msdfgen-style edge coloring algorithm for MSDF |

**Examples:**
```bash
--jpeg
--forceraster
--invert
--gammacorrect
--maximizeinsteadofaverage
--msdfgencoloring
```

### Advanced Options

| Argument | Type | Description | Default |
|----------|------|-------------|---------|
| `--midpointadjustment <value>` | Float | Adjustment to SDF midpoint threshold | Not set |

**Example:**
```bash
--midpointadjustment 0.5
```

### Complete Examples

#### Generate SDF from a font file (CPU mode):
```bash
fontpacker --nogui --infont /usr/share/fonts/truetype/arial.ttf \
  --type SDF --mode Software \
  --internalprocesssize 1024 --intendedsize 32 --padding 100 \
  --mincharcode 32 --mincharcode 126 \
  --outbin arial_sdf.bin
```

#### Generate MSDF from a font file (GPU mode):
```bash
fontpacker --nogui --infont font.ttf \
  --type MSDF --mode OpenGL \
  --internalprocesssize 2048 --intendedsize 64 --padding 200 \
  --msdfgencoloring \
  --outbin font_msdf.bin
```

#### Generate SDF from SVG file:
```bash
fontpacker --nogui --insvg shapes.svg \
  --type SDF --mode Software \
  --internalprocesssize 1024 --intendedsize 32 \
  --outbin shapes_sdf.bin
```

#### Convert between formats:
```bash
# Binary to CBOR
fontpacker --nogui --inbin font.bin --outcbor font.cbor

# CBOR to Binary
fontpacker --nogui --incbor font.cbor --outbin font.bin

# Export individual glyphs
fontpacker --nogui --inbin font.bin --outfont glyphs/glyph_%1.png
```

#### Load and save with processing:
```bash
fontpacker --nogui --inbin input.bin \
  --jpeg --gammacorrect \
  --outbin output.bin
```

## Output Format Documentation

For detailed information about the binary output format, see [BINARY_FORMAT.md](BINARY_FORMAT.md).

## Building

See the project's build system documentation for compilation instructions.

## License

See LICENSE file for license information.
