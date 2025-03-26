#ifndef FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
#define FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
#include <glm/glm.hpp>
#include <cstdint>
#include <array>
#include <vector>
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

struct EdgeSegment {
	EdgeType type;
	int32_t padding; // For easier storage in OpenGL SSBOs. std430
	std::array<glm::fvec2,4> points;
	float getMinX() const;
	float getMinY() const;
	float getMaxX() const;
	float getMaxY() const;
};

struct FontOutlineDecompositionContext {
	glm::fvec2 curPos;
	std::vector<EdgeSegment> edges;
	int moveTo(const glm::fvec2& to);
	int lineTo(const glm::fvec2& to);
	int conicTo(const glm::fvec2& control, const glm::fvec2&  to);
	int cubicTo(const glm::fvec2& control1, const glm::fvec2& control2, const glm::fvec2& to);
	void translateToNewSize(unsigned nWidth, unsigned nHeight, unsigned paddingX, unsigned paddingY);
	void translateToNewSize(unsigned nWidth, unsigned nHeight, unsigned paddingX, unsigned paddingY, double metricWidth, double metricHeight, double horiBearingX, double horiBearingY);
	bool isWithinBoundingBox(unsigned xOffset, unsigned yOffset, unsigned width, unsigned height);
};

#endif // FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
