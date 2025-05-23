#ifndef FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
#define FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
#include <functional>
#include <glm/glm.hpp>
#include <cstdint>
#include <array>
#include <vector>
#include <span>
#include <map>
enum class EdgeType : int32_t {
	LINEAR, // A line segment. Only first two points are relevant.
	QUADRATIC, // A quadratic Bezier curve. Only first three points are relevant.
	CUBIC // A cubic Bezier curve. All four points are relevant.
};

enum LinePart {
	LINE_P1 = 0,
	LINE_P2 = 1
};

enum QuadraticPart {
	QUADRATIC_P1 = 0,
	QUADRATIC_CONTROL = 1,
	QUADRATIC_P2 = 2
};

enum CubicPart {
	CUBIC_P1 = 0,
	CUBIC_CONTROL1 = 1,
	CUBIC_CONTROL2 = 2,
	CUBIC_P2 = 3
};

enum class EdgeColor : uint32_t {
	BLACK = 0x000000,
	RED = 0xFF0000,
	GREEN = 0x00FF00,
	YELLOW = 0xFFFF00,
	BLUE = 0x0000FF,
	MAGENTA = 0xFF00FF,
	CYAN = 0x00FFFF,
	WHITE = 0xFFFFFF
};

enum class Orientation {
	CW = 0,      // Clockwise
	CCW = 1,     // Counterclockwise
	COLINEAR = 2 // Colinear
};
Orientation checkOrientation(const glm::fvec2& A, const glm::fvec2& B);
Orientation checkOrientation(const glm::fvec2& A, const glm::fvec2& B, const glm::fvec2& C);
Orientation checkQuadraticOrientation(const glm::fvec2& p1, const glm::fvec2& control, const glm::fvec2& p2);
Orientation checkCubicOrientation(const glm::fvec2& p1, const glm::fvec2& c1,const glm::fvec2& c2, const glm::fvec2& p2);

struct EdgeSegment {
	EdgeType type;
	int32_t contourId;
	uint32_t clr; // RRGGBBXX encoded edge colour.
	int32_t padding; // For easier storage in OpenGL SSBOs. std430
	std::array<glm::fvec2,4> points;
	float getMinX() const;
	float getMinY() const;
	float getMaxX() const;
	float getMaxY() const;
	void invert();
	glm::fvec2 point(float param=0.0f) const;
	glm::fvec2 direction(float param=0.0f) const;
	glm::fvec2 directionChange(float param=0.0f) const;
	void splitIntoThree(EdgeSegment& a, EdgeSegment& b, EdgeSegment& c) const;
	Orientation checkOrientation() const;
	void deconverge(int param, const glm::fvec2& vector);
	int scanlineIntersections(double x[], int dy[3], double y) const;
};
Orientation computeWinding(const std::span<const EdgeSegment>& contour, int samplesPerCurve = 10);
bool isPointInContour(const glm::fvec2& point, const std::vector<EdgeSegment>& edges, int32_t contourId);

struct BoundingBox {
	float top, bottom, left, right;
};
struct ContourInfo {
	BoundingBox bb;
	int32_t contourId;
};

struct FontOutlineDecompositionContext {
	typedef std::vector<std::vector<size_t>> IdShapeMap;
	typedef std::function<void(const IdShapeMap&)> IdShapeMapIterator;
	typedef std::pair<int32_t,int32_t> ContourDefinition;
	typedef std::map<int32_t,ContourDefinition> ContourMap;
	typedef std::vector<ContourDefinition> ContourVector;
	std::vector<ContourInfo> contourInfo;
	glm::fvec2 curPos = glm::fvec2(0.0f, 0.0f);
	glm::fvec2 firstPointInContour = glm::fvec2(0.0f, 0.0f);
	std::vector<EdgeSegment> edges;
	std::vector<EdgeSegment> stagingEdges;
	int32_t curShapeId = 0;
	void closeShape(bool checkWinding=false);
	int moveTo(const glm::fvec2& to, bool checkWinding=false);
	int lineTo(const glm::fvec2& to);
	int conicTo(const glm::fvec2& control, const glm::fvec2&  to);
	int cubicTo(const glm::fvec2& control1, const glm::fvec2& control2, const glm::fvec2& to);
	void translateToNewSize(unsigned nWidth, unsigned nHeight, unsigned paddingX, unsigned paddingY, bool invertY = true);
	void translateToNewSize(unsigned nWidth, unsigned nHeight, unsigned paddingX, unsigned paddingY, double metricWidth, double metricHeight, double horiBearingX, double horiBearingY, bool invertY = true);
	bool isWithinBoundingBox(unsigned xOffset, unsigned yOffset, unsigned width, unsigned height);
	void iterateOverContours(const IdShapeMapIterator& shapeIterator) const;
	static float computeSignedArea(const std::span<const EdgeSegment>& contourEdges, int subdivisions = 20);
	static void normalizeContour(std::vector<EdgeSegment>& contour);
	ContourMap produceContourMap() const;
	ContourVector produceContourVecetor() const;
	std::span<EdgeSegment> getEdgeSegmentsForContour(const ContourDefinition& contour);
	std::span<const EdgeSegment> getEdgeSegmentsForContour(const ContourDefinition& contour) const;
	void orientContours();
	void makeShapeIdsSigend(bool flip = false);
	void assignColours();
	void assignColoursMsdfgen(double angleThreshold = 3.0, unsigned long long seed = 1942);
	void clear();
};

#endif // FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
