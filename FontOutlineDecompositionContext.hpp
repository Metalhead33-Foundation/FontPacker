#ifndef FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
#define FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
#include <functional>
#include <glm/glm.hpp>
#include <cstdint>
#include <array>
#include <vector>
#include <span>
enum EdgeType : int32_t {
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

enum Orientation {
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
	glm::fvec2 direction(float param=0.0f) const;
	glm::fvec2 directionChange(float param=0.0f) const;
	void splitIntoThree(EdgeSegment& a, EdgeSegment& b, EdgeSegment& c) const;
	Orientation checkOrientation() const;
	void deconverge(int param, const glm::fvec2& vector);
};

struct FontOutlineDecompositionContext {
	typedef std::vector<std::vector<size_t>> IdShapeMap;
	typedef std::function<void(const IdShapeMap&)> IdShapeMapIterator;
	glm::fvec2 curPos = glm::fvec2(0.0f, 0.0f);
	glm::fvec2 firstPointInContour = glm::fvec2(0.0f, 0.0f);
	std::vector<EdgeSegment> edges;
	std::vector<EdgeSegment> stagingEdges;
	int32_t curShapeId = 0;
	void closeShape();
	int moveTo(const glm::fvec2& to);
	int lineTo(const glm::fvec2& to);
	int conicTo(const glm::fvec2& control, const glm::fvec2&  to);
	int cubicTo(const glm::fvec2& control1, const glm::fvec2& control2, const glm::fvec2& to);
	void translateToNewSize(unsigned nWidth, unsigned nHeight, unsigned paddingX, unsigned paddingY);
	void translateToNewSize(unsigned nWidth, unsigned nHeight, unsigned paddingX, unsigned paddingY, double metricWidth, double metricHeight, double horiBearingX, double horiBearingY);
	bool isWithinBoundingBox(unsigned xOffset, unsigned yOffset, unsigned width, unsigned height);
	void iterateOverContours(const IdShapeMapIterator& shapeIterator) const;
	static float computeSignedArea(const std::span<const EdgeSegment>& contourEdges, int subdivisions = 20);
	static void normalizeContour(std::vector<EdgeSegment>& contour);
	void makeShapeIdsSigend(bool flip = false);
	void assignColours();
	void clear();
};

#endif // FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
