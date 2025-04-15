#include "FontOutlineDecompositionContext.hpp"
#include <climits>
#include <cstring>
#include <span>
#include <cassert>
#include <stdexcept>
#include <map>

#define EPSILON std::numeric_limits<float>::epsilon()

inline glm::fvec2 mix(const glm::fvec2& a, const glm::fvec2& b, float t) {
	return a * (1.0f - t) + b * t;
}
inline glm::vec2 lerp(const glm::vec2& a, const glm::vec2& b, float t) {
	return mix(a, b, t);
}

inline glm::vec2 bezier2(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, float t) {
	glm::vec2 a = lerp(p0, p1, t);
	glm::vec2 b = lerp(p1, p2, t);
	return lerp(a, b, t);
}

inline glm::vec2 bezier3(const glm::vec2& p0, const glm::vec2& p1, const glm::vec2& p2, const glm::vec2& p3, float t) {
	glm::vec2 a = lerp(p0, p1, t);
	glm::vec2 b = lerp(p1, p2, t);
	glm::vec2 c = lerp(p2, p3, t);
	glm::vec2 d = lerp(a, b, t);
	glm::vec2 e = lerp(b, c, t);
	return lerp(d, e, t);
}
/// A special version of the cross product for 2D vectors (returns scalar value).
inline float crossProduct(const glm::fvec2& a, const glm::fvec2& b) {
	return a.x*b.y-a.y*b.x;
}

/// Returns a vector with the same length that is orthogonal to this one.
inline glm::vec2 getOrthogonal(const glm::vec2& vectr, bool polarity = true) {
	return polarity ? glm::vec2(-vectr.y, vectr.x) : glm::vec2(vectr.y, -vectr.x);
}

/// Returns a vector with unit length that is orthogonal to this one.
inline glm::vec2 getOrthonormal(const glm::vec2& vectr, bool polarity = true, bool allowZero = false) {
	if (float len = glm::length(vectr))
		return polarity ? glm::vec2(-vectr.y/len, vectr.x/len) : glm::vec2(vectr.y/len, -vectr.x/len);
	return polarity ? glm::vec2(0, !allowZero) : glm::vec2(0, -!allowZero);
}

inline EdgeSegment convertQuadraticToCubic(const EdgeSegment& quad) {
	const glm::fvec2& p0 = quad.points[0];
	const glm::fvec2& c = quad.points[1];
	const glm::fvec2& p1 = quad.points[2];

	glm::fvec2 c1 = mix(p0, c, 2.0f / 3.0f);
	glm::fvec2 c2 = mix(c, p1, 1.0f / 3.0f);

	return {CUBIC, quad.contourId, quad.clr, 0, {p0, c1, c2, p1}};
}
inline void splitLinearInThirds(const EdgeSegment& seg, EdgeSegment& part0, EdgeSegment& part1, EdgeSegment& part2) {
	glm::fvec2 p0 = seg.points[0];
	glm::fvec2 p1 = seg.points[1];
	glm::fvec2 a = mix(p0, p1, 1.0f/3.0f);
	glm::fvec2 b = mix(p0, p1, 2.0f/3.0f);

	part0 = {LINEAR, seg.contourId, seg.clr, 0, {p0, a, {}, {}}};
	part1 = {LINEAR, seg.contourId, seg.clr, 0, {a, b, {}, {}}};
	part2 = {LINEAR, seg.contourId, seg.clr, 0, {b, p1, {}, {}}};
}

inline void splitQuadraticInThirds(const EdgeSegment& seg, EdgeSegment& part0, EdgeSegment& part1, EdgeSegment& part2) {
	const glm::fvec2& p0 = seg.points[0];
	const glm::fvec2& c = seg.points[1];
	const glm::fvec2& p1 = seg.points[2];

	glm::fvec2 pt13 = 0.25f * p0 + 0.5f * c + 0.25f * p1; // Quadratic Bezier at t=1/3
	glm::fvec2 pt23 = 0.25f * p0 + 0.5f * c + 0.25f * p1; // Reuse
	glm::fvec2 c0 = mix(p0, c, 1.0f/3.0f);
	glm::fvec2 c2 = mix(c, p1, 2.0f/3.0f);
	glm::fvec2 m1 = mix(mix(p0, c, 5.0f/9.0f), mix(c, p1, 4.0f/9.0f), 0.5f);

	part0 = {QUADRATIC, seg.contourId, seg.clr, 0, {p0, c0, pt13, {}}};
	part1 = {QUADRATIC, seg.contourId, seg.clr, 0, {pt13, m1, pt23, {}}};
	part2 = {QUADRATIC, seg.contourId, seg.clr, 0, {pt23, c2, p1, {}}};
}

inline void splitCubicInThirds(const EdgeSegment& seg, EdgeSegment& part0, EdgeSegment& part1, EdgeSegment& part2) {
	const glm::fvec2& p0 = seg.points[0];
	const glm::fvec2& c1 = seg.points[1];
	const glm::fvec2& c2 = seg.points[2];
	const glm::fvec2& p3 = seg.points[3];

	glm::fvec2 p13 = bezier3(p0, c1, c2, p3, 1.0f / 3.0f);
	glm::fvec2 p23 = bezier3(p0, c1, c2, p3, 2.0f / 3.0f);

	glm::fvec2 a = (p0 == c1) ? p0 : lerp(p0, c1, 1.0f/3.0f);
	glm::fvec2 b = lerp(lerp(p0, c1, 1.0f/3.0f), lerp(c1, c2, 1.0f/3.0f), 1.0f/3.0f);

	glm::fvec2 d = lerp(
		lerp(
			lerp(p0, c1, 1.0f/3.0f),
			lerp(c1, c2, 1.0f/3.0f),
			1.0f/3.0f),
		lerp(
			lerp(c1, c2, 1.0f/3.0f),
			lerp(c2, p3, 1.0f/3.0f),
			1.0f/3.0f),
		2.0f/3.0f
		);

	glm::fvec2 e = lerp(
		lerp(
			lerp(p0, c1, 2.0f/3.0f),
			lerp(c1, c2, 2.0f/3.0f),
			2.0f/3.0f),
		lerp(
			lerp(c1, c2, 2.0f/3.0f),
			lerp(c2, p3, 2.0f/3.0f),
			2.0f/3.0f),
		1.0f/3.0f
		);

	glm::fvec2 f = (c2 == p3) ? p3 : lerp(c2, p3, 2.0f/3.0f);

	part0 = {CUBIC, seg.contourId, seg.clr, 0, {p0, a, b, p13}};
	part1 = {CUBIC, seg.contourId, seg.clr, 0, {p13, d, e, p23}};
	part2 = {CUBIC, seg.contourId, seg.clr, 0, {p23, f, p3 == c2 ? p3 : lerp(c2, p3, 2.0f/3.0f), p3}};
}

void EdgeSegment::splitIntoThree(EdgeSegment& a, EdgeSegment& b, EdgeSegment& c) const
{
	switch (type) {
		case LINEAR: splitLinearInThirds(*this, a, b, c); break;
		case QUADRATIC: splitQuadraticInThirds(*this, a, b, c); break;
		case CUBIC: splitCubicInThirds(*this, a, b, c); break;
		default: break;
	}
}


float FontOutlineDecompositionContext::computeSignedArea(const std::span<const EdgeSegment>& contourEdges, int subdivisions) {
	float area = 0.0f;

	for (const auto& edge : contourEdges) {
		if (edge.type == LINEAR) {
			const glm::vec2& a = edge.points[0];
			const glm::vec2& b = edge.points[1];
			area += (a.x * b.y - b.x * a.y); // Shoelace term
		}

		else if (edge.type == QUADRATIC) {
			glm::vec2 prev = edge.points[0];
			for (int i = 1; i <= subdivisions; ++i) {
				float t = float(i) / subdivisions;
				glm::vec2 p =
					(1 - t) * (1 - t) * edge.points[0] +
					2 * (1 - t) * t * edge.points[1] +
					t * t * edge.points[2];
				area += (prev.x * p.y - p.x * prev.y);
				prev = p;
			}
		}

		else if (edge.type == CUBIC) {
			glm::vec2 prev = edge.points[0];
			for (int i = 1; i <= subdivisions; ++i) {
				float t = float(i) / subdivisions;
				glm::vec2 p =
					(1 - t) * (1 - t) * (1 - t) * edge.points[0] +
					3 * (1 - t) * (1 - t) * t * edge.points[1] +
					3 * (1 - t) * t * t * edge.points[2] +
					t * t * t * edge.points[3];
				area += (prev.x * p.y - p.x * prev.y);
				prev = p;
			}
		}
	}

	return area * 0.5f;
}
// Threshold of the dot product of adjacent edge directions to be considered convergent.
#define MSDFGEN_CORNER_DOT_EPSILON .000001
#define DECONVERGE_OVERSHOOT 1.11111111111111111 // moves control points slightly more than necessary to account for floating-point errors

void FontOutlineDecompositionContext::normalizeContour(std::vector<EdgeSegment>& contour)
{
	if (contour.size() == 1) {
		EdgeSegment parts[3];
		contour[0].splitIntoThree(parts[0], parts[1], parts[2]);
		contour.clear();
		contour.push_back(parts[0]);
		contour.push_back(parts[1]);
		contour.push_back(parts[2]);
	} else {
		// Push apart convergent edge segments
		EdgeSegment *prevEdge = &contour.back();
		for (auto edge = contour.begin(); edge != contour.end(); ++edge) {
			glm::fvec2 prevDir = glm::normalize((*prevEdge).direction(1));
			glm::fvec2 curDir = glm::normalize((*edge).direction(0));
			if (glm::dot(prevDir, curDir) < MSDFGEN_CORNER_DOT_EPSILON-1) {
				double factor = DECONVERGE_OVERSHOOT*sqrt(1-(MSDFGEN_CORNER_DOT_EPSILON-1)*(MSDFGEN_CORNER_DOT_EPSILON-1))/(MSDFGEN_CORNER_DOT_EPSILON-1);
				glm::fvec2 axis = glm::normalize(curDir-prevDir);
				// Determine curve ordering using third-order derivative (t = 0) of crossProduct((*prevEdge)->point(1-t)-p0, (*edge)->point(t)-p0) where p0 is the corner (*edge)->point(0)
				if (crossProduct((*prevEdge).directionChange(1), (*edge).direction(0))+crossProduct((*edge).directionChange(0), (*prevEdge).direction(1)) < 0)
					axis = -axis;
				prevEdge->deconverge(1, getOrthogonal(axis,true));
				edge->deconverge(0, getOrthogonal(axis,false));
			}
			prevEdge = &*edge;
		}
	}
}

void FontOutlineDecompositionContext::makeShapeIdsSigend(bool flip)
{
	if(flip) {
		for(auto& it : edges) it.invert();
	}
	int32_t maxShapeId = -1;
	int32_t minShapeId = std::numeric_limits<int32_t>::max();
	for(const auto& e : edges) {
		maxShapeId = std::max(maxShapeId, e.contourId);
		minShapeId = std::min(minShapeId, e.contourId);
	}
	std::map<int32_t,std::pair<int32_t,int32_t>> contourLimits;
	for(int32_t shapeId = minShapeId; shapeId <= maxShapeId; ++shapeId) {
		int32_t maxEdgeId = -1;
		int32_t minEdgeId = std::numeric_limits<int32_t>::max();
		for(int32_t i = 0; i < edges.size(); ++i) {
			if(edges[i].contourId == shapeId) {
				maxEdgeId = std::max(maxEdgeId,i);
				minEdgeId = std::min(minEdgeId,i);
			}
		}
		contourLimits[shapeId] = { minEdgeId, maxEdgeId };
	}
	for( auto it = std::begin(contourLimits) ; it != std::end(contourLimits) ; ++it ) {
		std::span<EdgeSegment> segment( &edges[it->second.first], &edges[it->second.second+1]);
		float area = computeSignedArea(segment);
		bool isCW = area > 0.0f;
		/*int orientations[3] = { 0, 0, 0 };
		for(const auto& zt : segment) {
			++orientations[zt.checkOrientation()];
		}
		Orientation majorityOrientation = orientations[Orientation::CW] > orientations[Orientation::CCW] ? Orientation::CW : Orientation::CCW;
		for(auto& zt : segment) {
			if(zt.checkOrientation() != majorityOrientation) zt.invert();
		}*/

		/*if(isCW) {
			for(auto & zt : segment) {
				if(zt.checkOrientation() != Orientation::CW) zt.invert();
			}
		}
		else {
			//std::reverse(std::begin(segment), std::end(segment));
			for(auto & zt : segment) {
				if(zt.checkOrientation() != Orientation::CCW) zt.invert();
			}
		}*/
		if(area < 0.0f) {
			for(auto& zt : segment) {
				zt.contourId *= -1;
			}
		}
	}
}

template<typename T> void zeroOut(std::span<T> data) {
	std::memset(data.data(),0,data.size_bytes());
}
template<typename T, size_t N> void zeroOut(std::array<T,N>& data) {
	zeroOut(std::span<T>(data.data(),N));
}

void FontOutlineDecompositionContext::closeShape()
{
	if (curPos != firstPointInContour)
		lineTo(firstPointInContour); // Close the contour
	if(stagingEdges.size()) {
		normalizeContour(stagingEdges);
		edges.insert(edges.end(),stagingEdges.begin(), stagingEdges.end());
		stagingEdges.clear();
	}
}

int FontOutlineDecompositionContext::moveTo(const glm::fvec2& to)
{
	closeShape();
	curPos = to;
	firstPointInContour = to;
	++curShapeId;
	return 0;
}

float EdgeSegment::getMinX() const
{
	switch (type) {
		case LINEAR: return std::min(points[LINE_P1].x,points[LINE_P2].x);
		case QUADRATIC: return std::min(points[QUADRATIC_P1].x,points[QUADRATIC_P2].x);
		case CUBIC: return std::min(points[CUBIC_P1].x,points[CUBIC_P2].x);
		default: return 0;
			break;
	}
}

float EdgeSegment::getMinY() const
{
	switch (type) {
		case LINEAR: return std::min(points[LINE_P1].y,points[LINE_P2].y);
		case QUADRATIC: return std::min(points[QUADRATIC_P1].y,points[QUADRATIC_P2].y);
		case CUBIC: return std::min(points[CUBIC_P1].y,points[CUBIC_P2].y);
		default: return 0;
			break;
	}
}

float EdgeSegment::getMaxX() const
{
	switch (type) {
		case LINEAR: return std::max(points[LINE_P1].x,points[LINE_P2].x);
		case QUADRATIC: return std::max(points[QUADRATIC_P1].x,points[QUADRATIC_P2].x);
		case CUBIC: return std::max(points[CUBIC_P1].x,points[CUBIC_P2].x);
		default: return 0;
			break;
	}
}

float EdgeSegment::getMaxY() const
{
	switch (type) {
		case LINEAR: return std::max(points[LINE_P1].y,points[LINE_P2].y);
		case QUADRATIC: return std::max(points[QUADRATIC_P1].y,points[QUADRATIC_P2].y);
		case CUBIC: return std::max(points[CUBIC_P1].y,points[CUBIC_P2].y);
		default: return 0;
			break;
	}
}

/*
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
*/

void EdgeSegment::invert()
{
	switch(type) {
		case LINEAR:{
			std::swap(points[LINE_P1], points[LINE_P2]);
			break;
		}
		case QUADRATIC:{
			std::swap(points[QUADRATIC_P1], points[QUADRATIC_P2]);
			break;
		}
		case CUBIC: {
			std::swap(points[CUBIC_P1], points[CUBIC_P2]);
			std::swap(points[CUBIC_CONTROL1], points[CUBIC_CONTROL2]);
			break;
		}
	}
}

glm::fvec2 EdgeSegment::direction(float param) const
{
	switch (type) {
		case LINEAR: {
			return points[1]-points[0];
		}
		case QUADRATIC: {
			glm::fvec2 tangent = mix(points[1]-points[0], points[2]-points[1], param);
			if ( std::abs(tangent.x) <= EPSILON && std::abs(tangent.y) <= EPSILON)
				return points[2]-points[0];
			return tangent;
		}
		case CUBIC: {
			glm::fvec2 tangent = mix(mix(points[1]-points[0], points[2]-points[1], param), mix(points[2]-points[1], points[3]-points[2], param), param);
			if ( std::abs(tangent.x) <= EPSILON && std::abs(tangent.y) <= EPSILON) {
				if (param == 0.0f) return points[2]-points[0];
				if (param == 1.0f) return points[3]-points[1];
			}
		}
		default:  {
			return glm::fvec2(0.0f, 0.0f);
		}
	}
}

glm::fvec2 EdgeSegment::directionChange(float param) const
{
	switch (type) {
		case LINEAR: {
			return glm::fvec2();
		}
		case QUADRATIC: {
			return (points[2]-points[1])-(points[1]-points[0]);
		}
		case CUBIC: {
			return mix((points[2]-points[1])-(points[1]-points[0]), (points[3]-points[2])-(points[2]-points[1]), param);
		}
		default:  {
			return glm::fvec2(0.0f, 0.0f);
		}
	}
}
/*
Orientation checkQuadraticOrientation(const glm::fvec2& p1, const glm::fvec2& control, const glm::fvec2& p2);
Orientation checkCubicOrientation(const glm::fvec2& p1, const glm::fvec2& c1,const glm::fvec2& c2, const glm::fvec2& p2);
*/
Orientation EdgeSegment::checkOrientation() const
{
	switch(type) {
		case LINEAR:{
			return ::checkOrientation(points[LINE_P1], points[LINE_P2]);
			break;
		}
		case QUADRATIC:{
			return ::checkQuadraticOrientation(points[QUADRATIC_P1], points[QUADRATIC_CONTROL], points[QUADRATIC_P2]);
			break;
		}
		case CUBIC: {
			return ::checkCubicOrientation(points[CUBIC_P1], points[CUBIC_CONTROL1], points[CUBIC_CONTROL2], points[CUBIC_P2]);
			break;
		}
		default: return Orientation::COLINEAR;
	}
}

void EdgeSegment::deconverge(int param, const glm::fvec2& vector)
{
	switch (type) {
		case LINEAR: return;
		case QUADRATIC: *this = convertQuadraticToCubic(*this); break;
		case CUBIC: break;
			break;
	}
	switch (param) {
		case 0:
			points[1] += glm::length(points[1]-points[0])*vector;
			break;
		case 1:
			points[2] += glm::length(points[2]-points[3])*vector;
			break;
		default: break;
	}
}

int FontOutlineDecompositionContext::lineTo(const glm::fvec2& to)
{
	stagingEdges.resize(stagingEdges.size() + 1);
	auto& edge = stagingEdges.back();
	edge.type = LINEAR;
	zeroOut(edge.points);
	edge.contourId = curShapeId;
	edge.points[LINE_P1] = curPos;
	edge.points[LINE_P2] = to;
	curPos = to;
	return 0;
}

int FontOutlineDecompositionContext::conicTo(const glm::fvec2& control, const glm::fvec2& to)
{
	stagingEdges.resize(stagingEdges.size() + 1);
	auto& edge = stagingEdges.back();
	edge.type = QUADRATIC;
	zeroOut(edge.points);
	edge.contourId = curShapeId;
	edge.points[QUADRATIC_P1] = curPos;
	edge.points[QUADRATIC_CONTROL] = control;
	edge.points[QUADRATIC_P2] = to;
	curPos = to;
	return 0;
}

int FontOutlineDecompositionContext::cubicTo(const glm::fvec2& control1, const glm::fvec2& control2, const glm::fvec2& to)
{
	stagingEdges.resize(stagingEdges.size() + 1);
	auto& edge = stagingEdges.back();
	edge.type = CUBIC;
	zeroOut(edge.points);
	edge.contourId = curShapeId;
	edge.points[CUBIC_P1] = curPos;
	edge.points[CUBIC_CONTROL1] = control1;
	edge.points[CUBIC_CONTROL2] = control2;
	edge.points[CUBIC_P2] = to;
	curPos = to;
	return 0;
}

void FontOutlineDecompositionContext::translateToNewSize(unsigned int nWidth, unsigned int nHeight, unsigned int paddingX, unsigned int paddingY)
{
	const unsigned widthWithoutPadding = nWidth - (2 * paddingX);
	const unsigned heightWithoutPadding = nHeight - (2 * paddingY);
	glm::fvec2 minDim(std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
	for(const auto& it : edges) {
		for(const auto& jt : it.points) {
			minDim.x = std::min(minDim.x,jt.x);
			minDim.y = std::min(minDim.y,jt.y);
		}
	}
	/* Somehow, this makes everything black? So comment it out.
	 * for(const auto& it : edges) {
			minDim.x = std::min(minDim.x,it.getMinX() );
			minDim.y = std::min(minDim.y,it.getMinY() );
	}*/
	for(auto& it : edges) {
		for(auto& jt : it.points) {
			jt -= minDim;
		}
	}
	glm::fvec2 maxDim(std::numeric_limits<float>::epsilon(), std::numeric_limits<float>::epsilon());
	for(const auto& it : edges) {
		for(const auto& jt : it.points) {
			maxDim.x = std::max(maxDim.x,jt.x);
			maxDim.y = std::max(maxDim.y,jt.y);
		}
	}
	/* Somehow, this makes everything black? So comment it out.
	 * for(const auto& it : edges) {
		maxDim.x = std::min(maxDim.x,it.getMaxX() );
		maxDim.y = std::min(maxDim.y,it.getMaxY() );
	}*/
	const float scaleX = static_cast<float>(widthWithoutPadding) / maxDim.x;
	const float scaleY = static_cast<float>(heightWithoutPadding) / maxDim.y;
	for(auto& it : edges) {
		for(auto& jt : it.points) {
			jt.x = (jt.x * scaleX) + static_cast<float>(paddingX);
			jt.y = (jt.y * scaleY) + static_cast<float>(paddingY);
		}
	}
	// Flip Y
	for(auto& it : edges) {
		for(auto& jt : it.points) {
			jt.y = static_cast<float>(nHeight) - jt.y;
		}
	}
}

void FontOutlineDecompositionContext::translateToNewSize(unsigned int nWidth, unsigned int nHeight,
														 unsigned int paddingX, unsigned int paddingY,
														 double metricWidth, double metricHeight,
														 double horiBearingX, double horiBearingY)
{
	const unsigned widthWithoutPadding = nWidth - (2 * paddingX);
	const unsigned heightWithoutPadding = nHeight - (2 * paddingY);

	// Instead of using calculated minDim, use the glyph metrics for accurate placement
	glm::fvec2 minDim(horiBearingX, horiBearingY - metricHeight); // Baseline correction
	glm::fvec2 maxDim(minDim.x + metricWidth, minDim.y + metricHeight);

	// Compute scaling factors based on known glyph metrics
	const float scaleX = static_cast<float>(widthWithoutPadding) / static_cast<float>(metricWidth);
	const float scaleY = static_cast<float>(heightWithoutPadding) / static_cast<float>(metricHeight);

	for (auto& it : edges) {
		for (auto& jt : it.points) {
			// Align points relative to the bearing-based minDim
			jt -= minDim;

			// Scale proportionally
			jt.x = (jt.x * scaleX) + static_cast<float>(paddingX);
			jt.y = (jt.y * scaleY) + static_cast<float>(paddingY);
		}
	}

	// Flip Y-axis to match OpenGL's coordinate system
	for (auto& it : edges) {
		for (auto& jt : it.points) {
			jt.y = static_cast<float>(nHeight) - jt.y;
		}
	}
}


bool FontOutlineDecompositionContext::isWithinBoundingBox(unsigned int xOffset, unsigned int yOffset, unsigned int width, unsigned int height)
{
	for(const auto& it : edges) {
		for(const auto& jt : it.points) {
			if(jt.x > static_cast<float>(width)) throw std::runtime_error("We fall outside the given width!");
			if(jt.y > static_cast<float>(height)) throw std::runtime_error("We fall outside the given height!");
			if(jt.x < static_cast<float>(xOffset)) throw std::runtime_error("Why are we inside the horizontal padding?!");
			if(jt.y < static_cast<float>(yOffset)) throw std::runtime_error("Why are we inside the vertical padding?!!");
		}
	}
	return true;
}

void FontOutlineDecompositionContext::iterateOverContours(const IdShapeMapIterator& shapeIterator) const
{
	IdShapeMap idShapeMap;
	int32_t curShapeId = 0;
	std::vector<size_t>* curSizeTVecetor = nullptr;
	for(size_t i = 0; i < edges.size(); ++i) {
		const auto& it = edges[i];
		if( it.contourId != curShapeId ) {
			idShapeMap.push_back( std::vector<size_t>() );
			curSizeTVecetor = &idShapeMap.back();
		}
		curShapeId = it.contourId;
		curSizeTVecetor->push_back(i);
	}
	shapeIterator(idShapeMap);
}

void FontOutlineDecompositionContext::assignColours()
{
	iterateOverContours( [this](const IdShapeMap& idMap) {
		for( const auto& c : idMap ) {
			uint32_t current;
			if( c.size() <= 1 ) {
				current = 0xFFFFFF;
				edges[c[0]].clr = current;
			} else {
				current = 0xFF00FF;
			}
			for( const size_t e : c ) {
				edges[e].clr = current;
				if( current == 0xFFFF00 ) {
					current = 0x00FFFF;
				}
				else {
					current = 0xFFFF00;
				}
			}
		}
	} );
}

void FontOutlineDecompositionContext::clear()
{
	curPos = glm::fvec2(0.0f, 0.0f);
	firstPointInContour = glm::fvec2(0.0f, 0.0f);
	edges.clear();
	curShapeId = 0;
}

Orientation checkOrientation(const glm::fvec2& A, const glm::fvec2& B)
{
	// Treating the line segment as a vector (B - A)
	float cross = A.x * B.y - A.y * B.x;

	if (cross > 0.0f) {
		return CCW;
	} else if (cross < 0.0f) {
		return CW;
	} else {
		return COLINEAR;
	}
}
Orientation checkOrientation(const glm::fvec2& A, const glm::fvec2& B, const glm::fvec2& C) {
	const float cross = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);

	if (cross > EPSILON ) return CCW;
	if (cross < -EPSILON) return CW;
	return COLINEAR;
}

Orientation checkQuadraticOrientation(const glm::fvec2& p1, const glm::fvec2& control, const glm::fvec2& p2) {
	// For quadratic Bezier, orientation is determined by the triangle (p1, control, p2)
	return checkOrientation(p1, control, p2);
}

Orientation checkCubicOrientation(const glm::fvec2& p1, const glm::fvec2& c1,
								  const glm::fvec2& c2, const glm::fvec2& p2) {
	// For cubic Bezier, we need a more comprehensive check:
	// 1. Check orientation of the control polygon
	// 2. Check for inflection points that might change orientation

	// First check the control polygon orientation
	const Orientation ctrlPolyOrientation = checkOrientation(p1, c1, c2);
	const Orientation endOrientation = checkOrientation(c1, c2, p2);

	// If control polygon is consistent, use that
	if (ctrlPolyOrientation == endOrientation && ctrlPolyOrientation != COLINEAR) {
		return ctrlPolyOrientation;
	}

	// If not consistent, we need to evaluate the curve more carefully
	// We'll use the sign of the derivative's cross product at t=0
	const glm::fvec2 initialDerivative = 3.0f * (c1 - p1);
	const glm::fvec2 secondDerivative = 6.0f * (c2 - 2.0f * c1 + p1);

	const float cross = initialDerivative.x * secondDerivative.y - initialDerivative.y * secondDerivative.x;

	if (cross > EPSILON) return CCW;
	if (cross < -EPSILON) return CW;

	// Fallback to checking the endpoints if everything else is colinear
	return checkOrientation(p1, p1 + initialDerivative, p2);
}
