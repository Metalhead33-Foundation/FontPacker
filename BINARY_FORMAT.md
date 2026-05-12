# Binary Format Specification

This document describes the binary formats used by FontPacker for storing preprocessed font data and standalone vector-image SDF data. The formats use QDataStream with Qt 4.0 compatibility and Big-Endian byte order.

## Format Overview

Font binary files use the `.wodf` extension and begin with the ASCII magic `WODF`.

Standalone vector-image binary files use the `.wodi` extension and begin with the ASCII magic `WODI`.

The equivalent CBOR maps include a `containerType` string. Font CBOR uses `PreprocessedFontFace`; standalone vector-image CBOR uses `StoredVectorImage`. The CBOR `type` field remains the SDF flavor field.

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
- **Fixed strings**:
  - `char[4]`: Raw ASCII magic identifier, not null-terminated
  - `char[8]`: Null-terminated ASCII string, padded with zeros

## File Structure

The font binary file contains a single `PreprocessedFontFace` structure:

```
PreprocessedFontFace {
    magic = "WODF"
    version
    Font Family Name (UTF-8 string with length)
    Font Metadata (type, distType, sizes, flags)
    Glyph Table of Contents (offset table)
    Kerning Data
    Glyph Data (stored at offsets from TOC)
}
```

The standalone vector-image binary file contains a single `StoredVectorImage` structure:

```
StoredVectorImage {
    magic = "WODI"
    version
    Vector Metadata
    Encoded Mipmaps
}
```

## Detailed Structure

### PreprocessedFontFace

```
Offset  Field                    Type        Description
------  ----------------------  ----------  -----------------------------------------
0       magic                   char[4]     ASCII magic "WODF"
4       version                 uint32_t    PreprocessedFontFace format version
8       familyNameSize          uint32_t    Length of font family name in bytes
12      familyName              uint8[]     UTF-8 encoded font family name (familyNameSize bytes)
12+N    type                    uint8_t     SDFType enumeration value
12+N+1  distType                uint8_t     DistanceType enumeration value
12+N+2  bitmap_size             uint32_t    Bitmap size in pixels
12+N+6  bitmap_logical_size     uint32_t    Logical bitmap size
12+N+10 bitmap_padding          uint32_t    Bitmap padding in pixels
12+N+14 hasVert                 bool        Whether vertical layout is supported
12+N+15 imageFormat             char[8]     Null-terminated glyph image format (e.g. PNG)
12+N+23 ascender                float       Scaled face ascender in pixels
12+N+27 descender               float       Scaled face descender in pixels
12+N+31 faceHeight              float       Scaled baseline-to-baseline distance in pixels
12+N+35 maxAdvance              float       Scaled maximum advance in pixels
12+N+39 unitsPerEm              uint32_t    Original font units per EM
12+N+43 charCount               uint32_t    Number of glyphs stored
12+N+47 glyphTOC                GlyphTOC[]  Table of contents for glyphs (charCount entries)
...     kerning                 KerningMap  Kerning information
...     glyphData                Glyph[]    Glyph data stored at offsets from TOC
```

Where `N = familyNameSize`. Current `version` is `1`.

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

### StoredVectorImage

`StoredVectorImage` is a standalone vector-image SDF container. It is not part of the `PreprocessedFontFace` glyph table.

```
StoredVectorImage {
    magic                   char[4]     ASCII magic "WODI"
    version                 uint32_t    StoredVectorImage format version. Current value: 1
    processingSize          uint32_t    Square size used during SDF generation
    actualSize              uint32_t    Level-0/final square texture size
    padding                 uint32_t    Padding in pixels
    logicalX                float       Original vector canvas/viewBox X origin
    logicalY                float       Original vector canvas/viewBox Y origin
    logicalWidth            float       Original vector canvas/viewBox width
    logicalHeight           float       Original vector canvas/viewBox height
    aspectRatio             float       logicalWidth / logicalHeight
    minX                    float       Minimum decomposed source X before normalization
    maxX                    float       Maximum decomposed source X before normalization
    minY                    float       Minimum decomposed source Y before normalization
    maxY                    float       Maximum decomposed source Y before normalization
    distanceRangeX          float       Horizontal encoded distance range in level-0 pixels
    distanceRangeY          float       Vertical encoded distance range in level-0 pixels
    encodingFlags           uint32_t    Bitmask of baked SDF value transforms
    midpointAdjustment      float       Baked midpoint adjustment, if flagged
    type                    uint8_t     SDFType enumeration value
    distType                uint8_t     DistanceType enumeration value
    imageFormat             char[8]     Null-terminated image format
    mipmapCount             uint32_t    Number of encoded mipmap images
    mipmaps                 Mipmap[mipmapCount]
}

Mipmap {
    dataLength              uint32_t    Length of encoded image data
    data                    uint8[]     Encoded image data
}
```

`encodingFlags` uses these bits:

- `0x00000001` = SDF values were inverted
- `0x00000002` = SDF values were gamma corrected
- `0x00000004` = downsampling used maximum values instead of averaging
- `0x00000008` = `midpointAdjustment` contains a baked adjustment value

## Font Reading Algorithm

1. Read `magic` (char[4], 4 bytes). For font files this must be `WODF`.
2. Read `version` (uint32_t, 4 bytes). Current supported value is `1`.
3. Read `familyNameSize` (uint32_t, 4 bytes)
4. Read font family name (familyNameSize bytes, UTF-8)
5. Read font metadata:
   - `type` (uint8_t, 1 byte)
   - `distType` (uint8_t, 1 byte)
   - `bitmap_size` (uint32_t, 4 bytes)
   - `bitmap_logical_size` (uint32_t, 4 bytes)
   - `bitmap_padding` (uint32_t, 4 bytes)
   - `hasVert` (bool, 1 byte)
   - `imageFormat` (char[8], 8 bytes, null-terminated)
   - `ascender` (float, 4 bytes)
   - `descender` (float, 4 bytes)
   - `faceHeight` (float, 4 bytes)
   - `maxAdvance` (float, 4 bytes)
   - `unitsPerEm` (uint32_t, 4 bytes)
   - `charCount` (uint32_t, 4 bytes)
6. Read glyph TOC: `charCount` entries, each containing:
   - `codePoint` (uint32_t, 4 bytes)
   - `offset` (uint32_t, 4 bytes)
7. Read `KerningMap`:
   - Read size (uint32_t)
   - For each entry:
     - Read `firstChar` (uint32_t)
     - Read `PerCharacterKerning`:
       - Read size (uint32_t)
       - For each entry:
         - Read `secondChar` (uint32_t)
         - Read `Vec2f` (2 × float, 8 bytes)
8. For each glyph in TOC:
   - Seek to `offset` from start of file
   - Read `StoredCharacter`:
     - Read `valid` (bool, 1 byte)
     - If valid:
       - Read all fixed fields (72 bytes)
       - Read `sdfLength` (uint32_t, 4 bytes)
       - Read SDF data (`sdfLength` bytes)

## Font Writing Algorithm

1. Write `magic` (char[4], 4 bytes). For font files this is `WODF`.
2. Write `version` (uint32_t, 4 bytes). Current value is `1`.
3. Write font family name:
   - Convert to UTF-8
   - Write length (uint32_t)
   - Write UTF-8 bytes
4. Write font metadata (type, distType, sizes, flags, face metrics, charCount)
5. Reserve space for TOC:
   - Record current position
   - Write `charCount` placeholder entries (all zeros, 8 bytes each)
6. Write `KerningMap`:
   - Write size (uint32_t)
   - For each entry:
     - Write `firstChar` (uint32_t)
     - Write `PerCharacterKerning`:
       - Write size (uint32_t)
       - For each entry:
         - Write `secondChar` (uint32_t)
         - Write `Vec2f` (2 × float)
7. Write glyph data:
   - For each glyph:
     - Record current position as glyph offset
     - Write `StoredCharacter` data
     - Store (codePoint, offset) mapping
8. Write TOC:
   - Seek back to TOC position
   - Write all (codePoint, offset) pairs

## Vector Image Reading Algorithm

1. Read `magic` (char[4], 4 bytes). For standalone vector-image files this must be `WODI`.
2. Read `version` (uint32_t, 4 bytes). Current supported value is `1`.
3. Read vector metadata in the `StoredVectorImage` field order above.
4. Read `mipmapCount` (uint32_t, 4 bytes).
5. For each mipmap:
   - Read `dataLength` (uint32_t, 4 bytes)
   - Read encoded image data (`dataLength` bytes)

## Vector Image Writing Algorithm

1. Write `magic` (char[4], 4 bytes). For standalone vector-image files this is `WODI`.
2. Write `version` (uint32_t, 4 bytes). Current value is `1`.
3. Write vector metadata in the `StoredVectorImage` field order above.
4. Write `mipmapCount` (uint32_t, 4 bytes).
5. For each mipmap:
   - Write `dataLength` (uint32_t, 4 bytes)
   - Write encoded image data (`dataLength` bytes)

## Example Parser Pseudocode

```python
def read_font_file(file):
    # Read header
    magic = read_bytes(file, 4)
    if magic != b'WODF':
        raise ValueError(f"Unsupported font magic: {magic!r}")
    version = read_uint32_be(file)
    if version != 1:
        raise ValueError(f"Unsupported PreprocessedFontFace version: {version}")
    family_name_size = read_uint32_be(file)
    family_name = read_bytes(file, family_name_size).decode('utf-8')
    
    # Read metadata
    type = read_uint8(file)
    dist_type = read_uint8(file)
    bitmap_size = read_uint32_be(file)
    bitmap_logical_size = read_uint32_be(file)
    bitmap_padding = read_uint32_be(file)
    has_vert = read_bool(file)
    image_format = read_bytes(file, 8).split(b'\0', 1)[0].decode('ascii')
    ascender = read_float_be(file)
    descender = read_float_be(file)
    face_height = read_float_be(file)
    max_advance = read_float_be(file)
    units_per_em = read_uint32_be(file)
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
- The first four bytes identify the binary container: `WODF` for font files and `WODI` for standalone vector-image files
- The TOC allows for efficient random access to glyphs without reading the entire file
- Glyph data is stored at variable offsets, so the file must support seeking
- The SDF bitmap data format depends on the `imageFormat` field and `type` field
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
