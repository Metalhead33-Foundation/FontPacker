#ifndef PROCESSFONTS_HPP
#define PROCESSFONTS_HPP
#include <QVariant>
#include <QMap>
#include <QCborMap>
#include <QCborArray>
#include <QCborValue>
#include <QByteArray>

enum SDfGenerationMode {
	SOFTWARE,
	OPENGL_COMPUTE,
	OPENCL
};
enum SDFType {
	SDF,
	MSDFA
};
enum DistanceType {
	Manhattan,
	Eucledian
};

struct SDFGenerationArguments {
	SDfGenerationMode mode = SOFTWARE;
	SDFType type = SDF;
	DistanceType distType = Manhattan;
	void fromArgs(const QVariantMap& args);
};

typedef struct FT_GlyphSlotRec_*  FT_GlyphSlot;
struct StoredCharacter {
	bool valid;
	unsigned width; // Intended width: actual size is 32x32 or whatever else set in globally in the font
	unsigned height; // Intended height: actual size is 32x32 or whatever else set in globally in the font
	int bearing_x;
	int bearing_y;
	unsigned advance;
	QByteArray sdf;
	void toCbor(QCborMap& cbor) const;
	QCborMap toCbor(void) const;
	void fromCbor(const QCborMap& cbor);
	void fromFreeTypeGlyph(FT_GlyphSlot glyphSlot, const SDFGenerationArguments& args = SDFGenerationArguments());
};

void processFonts(const QVariantMap& args);

#endif // PROCESSFONTS_HPP
