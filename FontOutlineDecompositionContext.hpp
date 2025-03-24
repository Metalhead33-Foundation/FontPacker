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

struct EdgeSegment {
	EdgeType type;
	int32_t padding;
	std::array<glm::fvec2,4> points;
};

struct FontOutlineDecompositionContext {
	glm::fvec2 curPos;
	std::vector<EdgeSegment> edges;
	int moveTo(const glm::fvec2& to);
	int lineTo(const glm::fvec2& to);
	int conicTo(const glm::fvec2& control, const glm::fvec2&  to);
	int cubicTo(const glm::fvec2& control1, const glm::fvec2& control2, const glm::fvec2& to);
	void translateToNewSize(unsigned nWidth, unsigned nHeight, unsigned paddingX, unsigned paddingY);
	bool isWithinBoundingBox(unsigned xOffset, unsigned yOffset, unsigned width, unsigned height);
};

#endif // FONTOUTLINEDECOMPOSITIONCONTEXT_HPP
