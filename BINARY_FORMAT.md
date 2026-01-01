# Binary Format Specification

This document describes the binary format used by FontPacker for storing preprocessed font data. The format uses QDataStream with Qt 4.0 compatibility and Big-Endian byte order.

## Format Overview

The binary format consists of a single `PreprocessedFontFace` structure that contains font metadata, kerning information, and glyph data.

## Data Types and Endianness

- **Byte Order**: Big-Endian (network byte order)
- **QDataStream Version**: Qt_4_0
- **Integer Types**: 
  - `uint8_t`: 1 byte, unsigned
  - `uint32_t`: 4 bytes, unsigned, big-endian
  - `int32_t`: 4 bytes, signed, big-endian (two's complement)
- **Floating Point**: 
  - `float`: 4 bytes, IEEE 754 single precision, big-endian
- **Boolean**: 
  - `bool`: 1 byte (0x00 = false, 0x01 = true)
- **Strings**: 
  - UTF-8 encoded byte arrays with length prefix

## File Structure

The file contains a single `PreprocessedFontFace` structure:

```
PreprocessedFontFace {
    Font Family Name (UTF-8 string with length)
    Font Metadata (type, distType, sizes, flags)
    Glyph Table of Contents (offset table)
    Kerning Data
    Glyph Data (stored at offsets from TOC)
}
```

## Detailed Structure

### PreprocessedFontFace

```
Offset  Field                    Type        Description
------  ----------------------  ----------  -----------------------------------------
0       familyNameSize          uint32_t    Length of font family name in bytes
4       familyName              uint8[]     UTF-8 encoded font family name (familyNameSize bytes)
4+N     type                    uint8_t     SDFType enumeration value
4+N+1   distType                uint8_t     DistanceType enumeration value
4+N+2   bitmap_size             uint32_t    Bitmap size in pixels
4+N+6   bitmap_logical_size     uint32_t    Logical bitmap size
4+N+10  bitmap_padding          uint32_t    Bitmap padding in pixels
4+N+14  hasVert                 bool        Whether vertical layout is supported
4+N+15  jpeg                    bool        Whether SDF data is JPEG compressed
4+N+16  charCount               uint32_t    Number of glyphs stored
4+N+20  glyphTOC                GlyphTOC[]  Table of contents for glyphs (charCount entries)
...     kerning                 KerningMap  Kerning information
...     glyphData                Glyph[]    Glyph data stored at offsets from TOC
```

Where `N = familyNameSize`.

### Glyph Table of Contents (TOC)

The TOC is written twice: first as placeholders (all zeros), then overwritten with actual offsets after glyph data is written. This allows for efficient random access to glyphs.

```
GlyphTOC Entry {
    codePoint    uint32_t    Unicode code point of the glyph
    offset       uint32_t    Byte offset from start of file to glyph data
}
```

The TOC contains `charCount` entries, each 8 bytes (4 bytes code point + 4 bytes offset).

### KerningMap

```
KerningMap {
    size                uint32_t                    Number of entries in the map
    entries             KerningMapEntry[size]        Array of kerning entries
}

KerningMapEntry {
    firstChar           uint32_t                    First character code point
    perCharKerning      PerCharacterKerning        Kerning data for this character pair
}
```

### PerCharacterKerning

```
PerCharacterKerning {
    size                uint32_t                    Number of kerning pairs
    entries             PerCharKerningEntry[size]   Array of kerning pairs
}

PerCharKerningEntry {
    secondChar          uint32_t                    Second character code point
    kerning             Vec2f                       Kerning offset (x, y)
}
```

### Vec2f

A 2D vector represented as a pair of floats:

```
Vec2f {
    x                   float                       X component
    y                   float                       Y component
}
```

Total size: 8 bytes (4 bytes per float).

### StoredCharacter (Glyph Data)

Glyph data is stored at the offsets specified in the TOC. Each glyph entry:

```
StoredCharacter {
    valid               bool                        Whether this glyph is valid
    [if valid == true:]
        width           uint32_t                    Intended width of the glyph
        height          uint32_t                    Intended height of the glyph
        bearing_x       int32_t                     X bearing (signed)
        bearing_y       int32_t                     Y bearing (signed)
        advance_x       uint32_t                    X advance
        advance_y       uint32_t                    Y advance
        metricWidth     float                       Metric width
        metricHeight    float                       Metric height
        horiBearingX    float                       Horizontal bearing X
        horiBearingY    float                       Horizontal bearing Y
        horiAdvance     float                       Horizontal advance
        vertBearingX    float                       Vertical bearing X
        vertBearingY    float                       Vertical bearing Y
        vertAdvance     float                       Vertical advance
        sdfLength       uint32_t                    Length of SDF bitmap data
        sdf              uint8[]                     SDF bitmap data (sdfLength bytes)
}
```

If `valid == false`, only the boolean is written (1 byte). If `valid == true`, all fields are written.

**Total size when valid:**
- Header: 1 byte (valid)
- Fixed fields: 8 × uint32_t (32 bytes) + 2 × int32_t (8 bytes) + 8 × float (32 bytes) = 72 bytes
- Variable: sdfLength (4 bytes) + sdf data (sdfLength bytes)
- **Total: 77 bytes + sdfLength bytes**

## Reading Algorithm

1. Read `familyNameSize` (uint32_t, 4 bytes)
2. Read font family name (familyNameSize bytes, UTF-8)
3. Read font metadata:
   - `type` (uint8_t, 1 byte)
   - `distType` (uint8_t, 1 byte)
   - `bitmap_size` (uint32_t, 4 bytes)
   - `bitmap_logical_size` (uint32_t, 4 bytes)
   - `bitmap_padding` (uint32_t, 4 bytes)
   - `hasVert` (bool, 1 byte)
   - `jpeg` (bool, 1 byte)
   - `charCount` (uint32_t, 4 bytes)
4. Read glyph TOC: `charCount` entries, each containing:
   - `codePoint` (uint32_t, 4 bytes)
   - `offset` (uint32_t, 4 bytes)
5. Read `KerningMap`:
   - Read size (uint32_t)
   - For each entry:
     - Read `firstChar` (uint32_t)
     - Read `PerCharacterKerning`:
       - Read size (uint32_t)
       - For each entry:
         - Read `secondChar` (uint32_t)
         - Read `Vec2f` (2 × float, 8 bytes)
6. For each glyph in TOC:
   - Seek to `offset` from start of file
   - Read `StoredCharacter`:
     - Read `valid` (bool, 1 byte)
     - If valid:
       - Read all fixed fields (72 bytes)
       - Read `sdfLength` (uint32_t, 4 bytes)
       - Read SDF data (`sdfLength` bytes)

## Writing Algorithm

1. Write font family name:
   - Convert to UTF-8
   - Write length (uint32_t)
   - Write UTF-8 bytes
2. Write font metadata (type, distType, sizes, flags, charCount)
3. Reserve space for TOC:
   - Record current position
   - Write `charCount` placeholder entries (all zeros, 8 bytes each)
4. Write `KerningMap`:
   - Write size (uint32_t)
   - For each entry:
     - Write `firstChar` (uint32_t)
     - Write `PerCharacterKerning`:
       - Write size (uint32_t)
       - For each entry:
         - Write `secondChar` (uint32_t)
         - Write `Vec2f` (2 × float)
5. Write glyph data:
   - For each glyph:
     - Record current position as glyph offset
     - Write `StoredCharacter` data
     - Store (codePoint, offset) mapping
6. Write TOC:
   - Seek back to TOC position
   - Write all (codePoint, offset) pairs

## Example Parser Pseudocode

```python
def read_font_file(file):
    # Read header
    family_name_size = read_uint32_be(file)
    family_name = read_bytes(file, family_name_size).decode('utf-8')
    
    # Read metadata
    type = read_uint8(file)
    dist_type = read_uint8(file)
    bitmap_size = read_uint32_be(file)
    bitmap_logical_size = read_uint32_be(file)
    bitmap_padding = read_uint32_be(file)
    has_vert = read_bool(file)
    jpeg = read_bool(file)
    char_count = read_uint32_be(file)
    
    # Read TOC
    toc = []
    for i in range(char_count):
        code_point = read_uint32_be(file)
        offset = read_uint32_be(file)
        toc.append((code_point, offset))
    
    # Read kerning
    kerning = read_kerning_map(file)
    
    # Read glyphs
    glyphs = {}
    for code_point, offset in toc:
        file.seek(offset)
        glyph = read_stored_character(file)
        glyphs[code_point] = glyph
    
    return FontFace(family_name, type, dist_type, ...)

def read_kerning_map(file):
    size = read_uint32_be(file)
    kerning = {}
    for i in range(size):
        first_char = read_uint32_be(file)
        per_char_kerning = read_per_character_kerning(file)
        kerning[first_char] = per_char_kerning
    return kerning

def read_per_character_kerning(file):
    size = read_uint32_be(file)
    kerning = {}
    for i in range(size):
        second_char = read_uint32_be(file)
        x = read_float_be(file)
        y = read_float_be(file)
        kerning[second_char] = (x, y)
    return kerning

def read_stored_character(file):
    valid = read_bool(file)
    if not valid:
        return None
    
    width = read_uint32_be(file)
    height = read_uint32_be(file)
    bearing_x = read_int32_be(file)
    bearing_y = read_int32_be(file)
    advance_x = read_uint32_be(file)
    advance_y = read_uint32_be(file)
    metric_width = read_float_be(file)
    metric_height = read_float_be(file)
    hori_bearing_x = read_float_be(file)
    hori_bearing_y = read_float_be(file)
    hori_advance = read_float_be(file)
    vert_bearing_x = read_float_be(file)
    vert_bearing_y = read_float_be(file)
    vert_advance = read_float_be(file)
    sdf_length = read_uint32_be(file)
    sdf_data = read_bytes(file, sdf_length)
    
    return StoredCharacter(valid, width, height, ...)
```

## Notes

- All multi-byte integers and floats are in **Big-Endian** byte order
- The TOC allows for efficient random access to glyphs without reading the entire file
- Glyph data is stored at variable offsets, so the file must support seeking
- The SDF bitmap data format depends on the `jpeg` flag and `type` field
- Empty glyphs (invalid) are stored as a single boolean `false` value
- The kerning map structure allows for sparse storage of kerning pairs

## Type Enumerations

### SDFType (uint8_t)

The `type` field specifies the type of signed distance field:

- `0` = `SDF` - Standard Signed Distance Field
- `1` = `MSDF` - Multi-channel Signed Distance Field
- `2` = `MSDFA` - Multi-channel Signed Distance Field with Alpha

### DistanceType (uint8_t)

The `distType` field specifies the distance calculation method:

- `0` = `Manhattan` - Manhattan distance (L1 norm: |x| + |y|)
- `1` = `Euclidean` - Euclidean distance (L2 norm: √(x² + y²))
