#include "FontOutlineDecompositionContext.hpp"
#include <climits>
#include <cstring>
#include <span>
#include <cassert>
#include <stdexcept>
#include <map>

#define EPSILON std::numeric_limits<float>::epsilon()

/// Returns 1 for positive values, -1 for negative values, and 0 for zero.
template <typename T>
inline int sign(T n) {
	return (T(0) < n)-(n < T(0));
}
/// Returns the weighted average of a and b.
template <typename T, typename S>
inline T mix(T a, T b, S weight) {
	return T((S(1)-weight)*a+weight*b);
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

	return {EdgeType::CUBIC, quad.contourId, quad.clr, 0, {p0, c1, c2, p1}};
}
inline void splitLinearInThirds(const EdgeSegment& seg, EdgeSegment& part0, EdgeSegment& part1, EdgeSegment& part2) {
	glm::fvec2 p0 = seg.points[0];
	glm::fvec2 p1 = seg.points[1];
	glm::fvec2 a = mix(p0, p1, 1.0f/3.0f);
	glm::fvec2 b = mix(p0, p1, 2.0f/3.0f);

	part0 = {EdgeType::LINEAR, seg.contourId, seg.clr, 0, {p0, a, {}, {}}};
	part1 = {EdgeType::LINEAR, seg.contourId, seg.clr, 0, {a, b, {}, {}}};
	part2 = {EdgeType::LINEAR, seg.contourId, seg.clr, 0, {b, p1, {}, {}}};
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

	part0 = {EdgeType::QUADRATIC, seg.contourId, seg.clr, 0, {p0, c0, pt13, {}}};
	part1 = {EdgeType::QUADRATIC, seg.contourId, seg.clr, 0, {pt13, m1, pt23, {}}};
	part2 = {EdgeType::QUADRATIC, seg.contourId, seg.clr, 0, {pt23, c2, p1, {}}};
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

	part0 = {EdgeType::CUBIC, seg.contourId, seg.clr, 0, {p0, a, b, p13}};
	part1 = {EdgeType::CUBIC, seg.contourId, seg.clr, 0, {p13, d, e, p23}};
	part2 = {EdgeType::CUBIC, seg.contourId, seg.clr, 0, {p23, f, p3 == c2 ? p3 : lerp(c2, p3, 2.0f/3.0f), p3}};
}

void EdgeSegment::splitIntoThree(EdgeSegment& a, EdgeSegment& b, EdgeSegment& c) const
{
	switch (type) {
		case EdgeType::LINEAR: splitLinearInThirds(*this, a, b, c); break;
		case EdgeType::QUADRATIC: splitQuadraticInThirds(*this, a, b, c); break;
		case EdgeType::CUBIC: splitCubicInThirds(*this, a, b, c); break;
		default: break;
	}
}


float FontOutlineDecompositionContext::computeSignedArea(const std::span<const EdgeSegment>& contourEdges, int subdivisions) {
	float area = 0.0f;

	for (const auto& edge : contourEdges) {
		if (edge.type == EdgeType::LINEAR) {
			const glm::vec2& a = edge.points[0];
			const glm::vec2& b = edge.points[1];
			area += (a.x * b.y - b.x * a.y); // Shoelace term
		}

		else if (edge.type == EdgeType::QUADRATIC) {
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

		else if (edge.type == EdgeType::CUBIC) {
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
				float factor = DECONVERGE_OVERSHOOT*sqrt(1-(MSDFGEN_CORNER_DOT_EPSILON-1)*(MSDFGEN_CORNER_DOT_EPSILON-1))/(MSDFGEN_CORNER_DOT_EPSILON-1);
				glm::fvec2 axis = factor * glm::normalize(curDir-prevDir);
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

std::span<EdgeSegment> FontOutlineDecompositionContext::getEdgeSegmentsForContour(const ContourDefinition& contour)
{
	return std::span<EdgeSegment>(&edges[contour.first], &edges[contour.second+1]);
}

std::span<const EdgeSegment> FontOutlineDecompositionContext::getEdgeSegmentsForContour(const ContourDefinition& contour) const
{
	return std::span<const EdgeSegment>(&edges[contour.first], &edges[contour.second+1]);
}

FontOutlineDecompositionContext::ContourVector FontOutlineDecompositionContext::produceContourVecetor() const
{
	int32_t maxShapeId = -1;
	int32_t minShapeId = std::numeric_limits<int32_t>::max();
	for(const auto& e : edges) {
		maxShapeId = std::max(maxShapeId, e.contourId);
		minShapeId = std::min(minShapeId, e.contourId);
	}
	ContourVector contourLimits;
	for(int32_t shapeId = minShapeId; shapeId <= maxShapeId; ++shapeId) {
		int32_t maxEdgeId = -1;
		int32_t minEdgeId = std::numeric_limits<int32_t>::max();
		bool insert = false;
		for(int32_t i = 0; i < edges.size(); ++i) {
			if(edges[i].contourId == shapeId) {
				maxEdgeId = std::max(maxEdgeId,i);
				minEdgeId = std::min(minEdgeId,i);
				insert = true;
			}
		}
		if(insert) contourLimits.push_back({ minEdgeId, maxEdgeId });
	}
	return contourLimits;
}

FontOutlineDecompositionContext::ContourMap FontOutlineDecompositionContext::produceContourMap() const
{
	int32_t maxShapeId = -1;
	int32_t minShapeId = std::numeric_limits<int32_t>::max();
	for(const auto& e : edges) {
		maxShapeId = std::max(maxShapeId, e.contourId);
		minShapeId = std::min(minShapeId, e.contourId);
	}
	ContourMap contourLimits;
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
	return contourLimits;
}

void FontOutlineDecompositionContext::orientContours()
{
	struct Intersection {
		double x;
		int direction;
		int contourIndex;

		static int compare(const void *a, const void *b) {
			return sign(reinterpret_cast<const Intersection *>(a)->x-reinterpret_cast<const Intersection *>(b)->x);
		}
	};

	const double ratio = .5*(sqrt(5)-1); // an irrational number to minimize chance of intersecting a corner or other point of interest
	auto contours = produceContourVecetor();
	std::vector<int> orientations(contours.size());
	std::vector<Intersection> intersections;
	for (int i = 0; i < (int) contours.size(); ++i) {
		std::span<EdgeSegment> contourEdges = getEdgeSegmentsForContour(contours[i]);
		if (!orientations[i] && !contourEdges.empty()) {
			// Find an Y that crosses the contour
			double y0 = contourEdges.front().point(0).y;
			double y1 = y0;

			for( auto edge = contourEdges.begin(); edge != contourEdges.end() && y0 == y1 ; ++edge )
				y1 = (*edge).point(1).y;

			for (auto edge = contourEdges.begin(); edge != contourEdges.end() && y0 == y1; ++edge)
				y1 = (*edge).point(ratio).y; // in case all endpoints are in a horizontal line

			double y = mix(y0, y1, ratio);
			// Scanline through whole shape at Y
			double x[3];
			int dy[3];
			for (int j = 0; j < (int) contours.size(); ++j) {
				std::span<EdgeSegment> contourEdges2 = getEdgeSegmentsForContour(contours[j]);
				for (auto edge = contourEdges2.begin(); edge != contourEdges2.end(); ++edge) {
					int n = (*edge).scanlineIntersections(x, dy, y);
					for (int k = 0; k < n; ++k) {
						Intersection intersection = { x[k], dy[k], j };
						intersections.push_back(intersection);
					}
				}
			}
			if (!intersections.empty()) {
				qsort(&intersections[0], intersections.size(), sizeof(Intersection), &Intersection::compare);
				// Disqualify multiple intersections
				for (int j = 1; j < (int) intersections.size(); ++j)
					if (intersections[j].x == intersections[j-1].x)
						intersections[j].direction = intersections[j-1].direction = 0;
				// Inspect scanline and deduce orientations of intersected contours
				for (int j = 0; j < (int) intersections.size(); ++j)
					if (intersections[j].direction)
						orientations[intersections[j].contourIndex] += 2*((j&1)^(intersections[j].direction > 0))-1;
				intersections.clear();
			}
		}
	}
	// Reverse contours that have the opposite orientation
	for (int i = 0; i < (int) contours.size(); ++i) {
		if (orientations[i] < 0) {
			std::span<EdgeSegment> contourEdges = getEdgeSegmentsForContour(contours[i]);
			std::reverse(std::begin(contourEdges), std::end(contourEdges));
			//contours[i].reverse();
		}
	}
}

void FontOutlineDecompositionContext::makeShapeIdsSigend(bool flip)
{
	if(flip) {
		for(auto& it : edges) it.invert();
	}
	auto contourLimits = produceContourMap();
	for( auto it = std::begin(contourLimits) ; it != std::end(contourLimits) ; ++it ) {
		std::span<EdgeSegment> segment = getEdgeSegmentsForContour(it->second);
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
		case EdgeType::LINEAR: return std::min(points[LINE_P1].x,points[LINE_P2].x);
		case EdgeType::QUADRATIC: return std::min(points[QUADRATIC_P1].x,points[QUADRATIC_P2].x);
		case EdgeType::CUBIC: return std::min(points[CUBIC_P1].x,points[CUBIC_P2].x);
		default: return 0;
			break;
	}
}

float EdgeSegment::getMinY() const
{
	switch (type) {
		case EdgeType::LINEAR: return std::min(points[LINE_P1].y,points[LINE_P2].y);
		case EdgeType::QUADRATIC: return std::min(points[QUADRATIC_P1].y,points[QUADRATIC_P2].y);
		case EdgeType::CUBIC: return std::min(points[CUBIC_P1].y,points[CUBIC_P2].y);
		default: return 0;
			break;
	}
}

float EdgeSegment::getMaxX() const
{
	switch (type) {
		case EdgeType::LINEAR: return std::max(points[LINE_P1].x,points[LINE_P2].x);
		case EdgeType::QUADRATIC: return std::max(points[QUADRATIC_P1].x,points[QUADRATIC_P2].x);
		case EdgeType::CUBIC: return std::max(points[CUBIC_P1].x,points[CUBIC_P2].x);
		default: return 0;
			break;
	}
}

float EdgeSegment::getMaxY() const
{
	switch (type) {
		case EdgeType::LINEAR: return std::max(points[LINE_P1].y,points[LINE_P2].y);
		case EdgeType::QUADRATIC: return std::max(points[QUADRATIC_P1].y,points[QUADRATIC_P2].y);
		case EdgeType::CUBIC: return std::max(points[CUBIC_P1].y,points[CUBIC_P2].y);
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
		case EdgeType::LINEAR:{
			std::swap(points[LINE_P1], points[LINE_P2]);
			break;
		}
		case EdgeType::QUADRATIC:{
			std::swap(points[QUADRATIC_P1], points[QUADRATIC_P2]);
			break;
		}
		case EdgeType::CUBIC: {
			std::swap(points[CUBIC_P1], points[CUBIC_P2]);
			std::swap(points[CUBIC_CONTROL1], points[CUBIC_CONTROL2]);
			break;
		}
	}
}

glm::fvec2 EdgeSegment::point(float param) const
{
	switch(type) {
		case EdgeType::LINEAR:{
			return mix(points[0], points[1], param);
			break;
		}
		case EdgeType::QUADRATIC:{
			return mix(mix(points[0], points[1], param), mix(points[1], points[2], param), param);
			break;
		}
		case EdgeType::CUBIC: {
			glm::fvec2 p12 = mix(points[1], points[2], param);
			return mix(mix(mix(points[0], points[1], param), p12, param), mix(p12, mix(points[2], points[3], param), param), param);
			break;
		}
	}
}

glm::fvec2 EdgeSegment::direction(float param) const
{
	switch (type) {
		case EdgeType::LINEAR: {
			return points[1]-points[0];
		}
		case EdgeType::QUADRATIC: {
			glm::fvec2 tangent = mix(points[1]-points[0], points[2]-points[1], param);
			if ( std::abs(tangent.x) <= EPSILON && std::abs(tangent.y) <= EPSILON)
				return points[2]-points[0];
			return tangent;
		}
		case EdgeType::CUBIC: {
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
		case EdgeType::LINEAR: {
			return glm::fvec2();
		}
		case EdgeType::QUADRATIC: {
			return (points[2]-points[1])-(points[1]-points[0]);
		}
		case EdgeType::CUBIC: {
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
		case EdgeType::LINEAR:{
			return ::checkOrientation(points[LINE_P1], points[LINE_P2]);
			break;
		}
		case EdgeType::QUADRATIC:{
			return ::checkQuadraticOrientation(points[QUADRATIC_P1], points[QUADRATIC_CONTROL], points[QUADRATIC_P2]);
			break;
		}
		case EdgeType::CUBIC: {
			return ::checkCubicOrientation(points[CUBIC_P1], points[CUBIC_CONTROL1], points[CUBIC_CONTROL2], points[CUBIC_P2]);
			break;
		}
		default: return Orientation::COLINEAR;
	}
}

void EdgeSegment::deconverge(int param, const glm::fvec2& vector)
{
	switch (type) {
		case EdgeType::LINEAR: return;
		case EdgeType::QUADRATIC: *this = convertQuadraticToCubic(*this); break;
		case EdgeType::CUBIC: break;
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

int solveQuadratic(double x[2], double a, double b, double c) {
	// a == 0 -> linear equation
	if (a == 0 || fabs(b) > 1e12*fabs(a)) {
		// a == 0, b == 0 -> no solution
		if (b == 0) {
			if (c == 0)
				return -1; // 0 == 0
			return 0;
		}
		x[0] = -c/b;
		return 1;
	}
	double dscr = b*b-4*a*c;
	if (dscr > 0) {
		dscr = sqrt(dscr);
		x[0] = (-b+dscr)/(2*a);
		x[1] = (-b-dscr)/(2*a);
		return 2;
	} else if (dscr == 0) {
		x[0] = -b/(2*a);
		return 1;
	} else
		return 0;
}

static int solveCubicNormed(double x[3], double a, double b, double c) {
	double a2 = a*a;
	double q = 1/9.*(a2-3*b);
	double r = 1/54.*(a*(2*a2-9*b)+27*c);
	double r2 = r*r;
	double q3 = q*q*q;
	a *= 1/3.;
	if (r2 < q3) {
		double t = r/sqrt(q3);
		if (t < -1) t = -1;
		if (t > 1) t = 1;
		t = acos(t);
		q = -2*sqrt(q);
		x[0] = q*cos(1/3.*t)-a;
		x[1] = q*cos(1/3.*(t+2*M_PI))-a;
		x[2] = q*cos(1/3.*(t-2*M_PI))-a;
		return 3;
	} else {
		double u = (r < 0 ? 1 : -1)*pow(fabs(r)+sqrt(r2-q3), 1/3.);
		double v = u == 0 ? 0 : q/u;
		x[0] = (u+v)-a;
		if (u == v || fabs(u-v) < 1e-12*fabs(u+v)) {
			x[1] = -.5*(u+v)-a;
			return 2;
		}
		return 1;
	}
}

int solveCubic(double x[3], double a, double b, double c, double d) {
	if (a != 0) {
		double bn = b/a;
		if (fabs(bn) < 1e6) // Above this ratio, the numerical error gets larger than if we treated a as zero
			return solveCubicNormed(x, bn, c/a, d/a);
	}
	return solveQuadratic(x, b, c, d);
}

int EdgeSegment::scanlineIntersections(double x[], int dy[], double y) const
{
	auto& p = points;
	switch (type) {
		case EdgeType::LINEAR: {
			if ((y >= points[0].y && y < points[1].y) || (y >= points[1].y && y < points[0].y)) {
				double param = (y-points[0].y)/(points[1].y-points[0].y);
				x[0] = mix(points[0].x, points[1].x, param);
				dy[0] = sign(points[1].y-points[0].y);
				return 1;
			}
			return 0;
		}
		case EdgeType::QUADRATIC: {
			int total = 0;
			int nextDY = y > p[0].y ? 1 : -1;
			x[total] = p[0].x;
			if (p[0].y == y) {
				if (p[0].y < p[1].y || (p[0].y == p[1].y && p[0].y < p[2].y))
					dy[total++] = 1;
				else
					nextDY = 1;
			}
			{
				glm::fvec2 ab = p[1]-p[0];
				glm::fvec2 br = p[2]-p[1]-ab;
				double t[2];
				int solutions = solveQuadratic(t, br.y, 2*ab.y, p[0].y-y);
				// Sort solutions
				double tmp;
				if (solutions >= 2 && t[0] > t[1])
					tmp = t[0], t[0] = t[1], t[1] = tmp;
				for (int i = 0; i < solutions && total < 2; ++i) {
					if (t[i] >= 0 && t[i] <= 1) {
						x[total] = p[0].x+2*t[i]*ab.x+t[i]*t[i]*br.x;
						if (nextDY*(ab.y+t[i]*br.y) >= 0) {
							dy[total++] = nextDY;
							nextDY = -nextDY;
						}
					}
				}
			}
			if (p[2].y == y) {
				if (nextDY > 0 && total > 0) {
					--total;
					nextDY = -1;
				}
				if ((p[2].y < p[1].y || (p[2].y == p[1].y && p[2].y < p[0].y)) && total < 2) {
					x[total] = p[2].x;
					if (nextDY < 0) {
						dy[total++] = -1;
						nextDY = 1;
					}
				}
			}
			if (nextDY != (y >= p[2].y ? 1 : -1)) {
				if (total > 0)
					--total;
				else {
					if (fabs(p[2].y-y) < fabs(p[0].y-y))
						x[total] = p[2].x;
					dy[total++] = nextDY;
				}
			}
			return total;
		}
		case EdgeType::CUBIC:  {
			int total = 0;
			int nextDY = y > p[0].y ? 1 : -1;
			x[total] = p[0].x;
			if (p[0].y == y) {
				if (p[0].y < p[1].y || (p[0].y == p[1].y && (p[0].y < p[2].y || (p[0].y == p[2].y && p[0].y < p[3].y))))
					dy[total++] = 1;
				else
					nextDY = 1;
			}
			{
				glm::fvec2 ab = p[1]-p[0];
				glm::fvec2 br = p[2]-p[1]-ab;
				glm::fvec2 as = (p[3]-p[2])-(p[2]-p[1])-br;
				double t[3];
				int solutions = solveCubic(t, as.y, 3*br.y, 3*ab.y, p[0].y-y);
				// Sort solutions
				double tmp;
				if (solutions >= 2) {
					if (t[0] > t[1])
						tmp = t[0], t[0] = t[1], t[1] = tmp;
					if (solutions >= 3 && t[1] > t[2]) {
						tmp = t[1], t[1] = t[2], t[2] = tmp;
						if (t[0] > t[1])
							tmp = t[0], t[0] = t[1], t[1] = tmp;
					}
				}
				for (int i = 0; i < solutions && total < 3; ++i) {
					if (t[i] >= 0 && t[i] <= 1) {
						x[total] = p[0].x+3*t[i]*ab.x+3*t[i]*t[i]*br.x+t[i]*t[i]*t[i]*as.x;
						if (nextDY*(ab.y+2*t[i]*br.y+t[i]*t[i]*as.y) >= 0) {
							dy[total++] = nextDY;
							nextDY = -nextDY;
						}
					}
				}
			}
			if (p[3].y == y) {
				if (nextDY > 0 && total > 0) {
					--total;
					nextDY = -1;
				}
				if ((p[3].y < p[2].y || (p[3].y == p[2].y && (p[3].y < p[1].y || (p[3].y == p[1].y && p[3].y < p[0].y)))) && total < 3) {
					x[total] = p[3].x;
					if (nextDY < 0) {
						dy[total++] = -1;
						nextDY = 1;
					}
				}
			}
			if (nextDY != (y >= p[3].y ? 1 : -1)) {
				if (total > 0)
					--total;
				else {
					if (fabs(p[3].y-y) < fabs(p[0].y-y))
						x[total] = p[3].x;
					dy[total++] = nextDY;
				}
			}
			return total;
		}
		default: return 0;
	}
}

int FontOutlineDecompositionContext::lineTo(const glm::fvec2& to)
{
	stagingEdges.resize(stagingEdges.size() + 1);
	auto& edge = stagingEdges.back();
	edge.type = EdgeType::LINEAR;
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
	edge.type = EdgeType::QUADRATIC;
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
	edge.type = EdgeType::CUBIC;
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

enum EdgeColorMSDFGEn {
	BLACK = 0,
	RED = 1,
	GREEN = 2,
	YELLOW = 3,
	BLUE = 4,
	MAGENTA = 5,
	CYAN = 6,
	WHITE = 7
};
const std::array<EdgeColor,EdgeColorMSDFGEn::WHITE+1> PALETTE = {
	EdgeColor::BLACK, EdgeColor::RED, EdgeColor::GREEN, EdgeColor::YELLOW, EdgeColor::BLUE, EdgeColor::MAGENTA, EdgeColor::CYAN, EdgeColor::WHITE
};

// More black magic from MSDFGen

static int symmetricalTrichotomy(int position, int n) {
	return int(3+2.875*position/(n-1)-1.4375+.5)-3;
}

static bool isCorner(const glm::fvec2 &aDir, const glm::fvec2 &bDir, double crossThreshold) {
	return glm::dot(aDir, bDir) <= 0 || fabs(crossProduct(aDir, bDir)) > crossThreshold;
}
#define MSDFGEN_EDGE_LENGTH_PRECISION 4
static double estimateEdgeLength(const EdgeSegment *edge) {
	double len = 0;
	glm::fvec2 prev = edge->point(0);
	for (int i = 1; i <= MSDFGEN_EDGE_LENGTH_PRECISION; ++i) {
		glm::fvec2 cur = edge->point(1./MSDFGEN_EDGE_LENGTH_PRECISION*i);
		len += (cur-prev).length();
		prev = cur;
	}
	return len;
}

static int seedExtract2(unsigned long long &seed) {
	int v = int(seed)&1;
	seed >>= 1;
	return v;
}

static int seedExtract3(unsigned long long &seed) {
	int v = int(seed%3);
	seed /= 3;
	return v;
}

static EdgeColorMSDFGEn initColor(unsigned long long &seed) {
	static const EdgeColorMSDFGEn colors[3] = { CYAN, MAGENTA, YELLOW };
	return colors[seedExtract3(seed)];
}

static void switchColor(EdgeColorMSDFGEn &color, unsigned long long &seed) {
	int shifted = color<<(1+seedExtract2(seed));
	color = EdgeColorMSDFGEn((shifted|shifted>>3)&WHITE);
}

static void switchColor(EdgeColorMSDFGEn &color, unsigned long long &seed, EdgeColorMSDFGEn banned) {
	EdgeColorMSDFGEn combined = EdgeColorMSDFGEn(color&banned);
	if (combined == RED || combined == GREEN || combined == BLUE)
		color = EdgeColorMSDFGEn(combined^WHITE);
	else
		switchColor(color, seed);
}

void FontOutlineDecompositionContext::assignColours()
{
	iterateOverContours( [this](const IdShapeMap& idMap) {
		for( const auto& c : idMap ) {
			uint32_t current;
			if( c.size() <= 1 ) {
				current = static_cast<uint32_t>(EdgeColor::WHITE);
				edges[c[0]].clr = current;
			} else {
				current = static_cast<uint32_t>(EdgeColor::MAGENTA);
			}
			for( const size_t e : c ) {
				edges[e].clr = current;
				if( current == static_cast<uint32_t>(EdgeColor::YELLOW) ) {
					current = static_cast<uint32_t>(EdgeColor::CYAN);
				}
				else {
					current = static_cast<uint32_t>(EdgeColor::YELLOW);
				}
			}
		}
	} );
}

void FontOutlineDecompositionContext::assignColoursMsdfgen(double angleThreshold, unsigned long long seed)
{
	double crossThreshold = sin(angleThreshold);
	EdgeColorMSDFGEn color = initColor(seed);
	std::vector<int> corners;
	auto contours = produceContourVecetor();
	for(const auto& contour : contours) {
		auto edges = getEdgeSegmentsForContour(contour);
		// Black magic to count corners?
		{
			corners.clear();
			glm::fvec2 prevDirection = edges.back().direction(1);
			int index = 0;
			for (auto edge = edges.begin(); edge != edges.end(); ++edge, ++index) {
				if (isCorner(glm::normalize(prevDirection), glm::normalize((*edge).direction(0)), crossThreshold))
					corners.push_back(index);
				prevDirection = (*edge).direction(1);
			}
		}
		// Smooth contour
		if (corners.empty()) {
			switchColor(color, seed);
			for (auto edge = edges.begin(); edge != edges.end(); ++edge)
				(*edge).clr = color;
		}
		 // "Teardrop" case
		else if (corners.size() == 1)
		{
			EdgeColorMSDFGEn colors[3];
			switchColor(color, seed);
			colors[0] = color;
			colors[1] = WHITE;
			switchColor(color, seed);
			colors[2] = color;
			int corner = corners[0];
			{
				int m = (int) edges.size();
				for (int i = 0; i < m; ++i)
					edges[(corner+i)%m].clr = colors[1+symmetricalTrichotomy(i, m)];
			}
		}
		// Multiple corners
		else {
			int cornerCount = (int) corners.size();
			int spline = 0;
			int start = corners[0];
			int m = (int)edges.size();
			switchColor(color, seed);
			EdgeColorMSDFGEn initialColor = color;
			for (int i = 0; i < m; ++i) {
				int index = (start+i)%m;
				if (spline+1 < cornerCount && corners[spline+1] == index) {
					++spline;
					switchColor(color, seed, EdgeColorMSDFGEn((spline == cornerCount-1)*initialColor));
				}
				edges[index].clr = color;
			}
		}
	}
	for(auto& zt : edges) {
		zt.clr = static_cast<uint32_t>(PALETTE[zt.clr]);
	}
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
		return Orientation::CCW;
	} else if (cross < 0.0f) {
		return Orientation::CW;
	} else {
		return Orientation::COLINEAR;
	}
}
Orientation checkOrientation(const glm::fvec2& A, const glm::fvec2& B, const glm::fvec2& C) {
	const float cross = (B.x - A.x) * (C.y - A.y) - (B.y - A.y) * (C.x - A.x);

	if (cross > EPSILON ) return Orientation::CCW;
	if (cross < -EPSILON) return Orientation::CW;
	return Orientation::COLINEAR;
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
	if (ctrlPolyOrientation == endOrientation && ctrlPolyOrientation != Orientation::COLINEAR) {
		return ctrlPolyOrientation;
	}

	// If not consistent, we need to evaluate the curve more carefully
	// We'll use the sign of the derivative's cross product at t=0
	const glm::fvec2 initialDerivative = 3.0f * (c1 - p1);
	const glm::fvec2 secondDerivative = 6.0f * (c2 - 2.0f * c1 + p1);

	const float cross = initialDerivative.x * secondDerivative.y - initialDerivative.y * secondDerivative.x;

	if (cross > EPSILON) return Orientation::CCW;
	if (cross < -EPSILON) return Orientation::CW;

	// Fallback to checking the endpoints if everything else is colinear
	return checkOrientation(p1, p1 + initialDerivative, p2);
}
