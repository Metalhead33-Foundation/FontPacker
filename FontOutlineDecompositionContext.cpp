#include "FontOutlineDecompositionContext.hpp"
#include <climits>
#include <cstring>
#include <span>
#include <cassert>
#include <stdexcept>
#include <map>

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

void FontOutlineDecompositionContext::makeShapeIdsSigend(bool flip)
{
	if(flip) {
		for(auto& it : edges) it.invert();
	}
	int32_t maxShapeId = -1;
	int32_t minShapeId = std::numeric_limits<int32_t>::max();
	for(const auto& e : edges) {
		maxShapeId = std::max(maxShapeId, e.shapeId);
		minShapeId = std::min(minShapeId, e.shapeId);
	}
	std::map<int32_t,std::pair<int32_t,int32_t>> contourLimits;
	for(int32_t shapeId = minShapeId; shapeId <= maxShapeId; ++shapeId) {
		int32_t maxEdgeId = -1;
		int32_t minEdgeId = std::numeric_limits<int32_t>::max();
		for(int32_t i = 0; i < edges.size(); ++i) {
			if(edges[i].shapeId == shapeId) {
				maxEdgeId = std::max(maxEdgeId,i);
				minEdgeId = std::min(minEdgeId,i);
			}
		}
		contourLimits[shapeId] = { minEdgeId, maxEdgeId };
	}
	for( auto it = std::begin(contourLimits) ; it != std::end(contourLimits) ; ++it ) {
		std::span<EdgeSegment> segment( &edges[it->second.first], it->second.second - it->second.first );
		float area = computeSignedArea(segment);
		bool isCW = area > 0.0f;

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
				zt.shapeId *= -1;
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
			break;
		}
	}
}

Orientation EdgeSegment::checkOrientation() const
{
	switch(type) {
		case LINEAR:{
			return ::checkOrientation(points[LINE_P1], points[LINE_P2]);
			break;
		}
		case QUADRATIC:{
			return ::checkOrientation(points[QUADRATIC_P1], points[QUADRATIC_P2]);
			break;
		}
		case CUBIC: {
			return ::checkOrientation(points[CUBIC_P1], points[CUBIC_P2]);
			break;
		}
		default: return Orientation::COLINEAR;
	}
}

int FontOutlineDecompositionContext::lineTo(const glm::fvec2& to)
{
	edges.resize(edges.size() + 1);
	auto& edge = edges.back();
	edge.type = LINEAR;
	zeroOut(edge.points);
	edge.shapeId = curShapeId;
	edge.points[LINE_P1] = curPos;
	edge.points[LINE_P2] = to;
	curPos = to;
	return 0;
}

int FontOutlineDecompositionContext::conicTo(const glm::fvec2& control, const glm::fvec2& to)
{
	edges.resize(edges.size() + 1);
	auto& edge = edges.back();
	edge.type = QUADRATIC;
	zeroOut(edge.points);
	edge.shapeId = curShapeId;
	edge.points[QUADRATIC_P1] = curPos;
	edge.points[QUADRATIC_CONTROL] = control;
	edge.points[QUADRATIC_P2] = to;
	curPos = to;
	return 0;
}

int FontOutlineDecompositionContext::cubicTo(const glm::fvec2& control1, const glm::fvec2& control2, const glm::fvec2& to)
{
	edges.resize(edges.size() + 1);
	auto& edge = edges.back();
	edge.type = CUBIC;
	zeroOut(edge.points);
	edge.shapeId = curShapeId;
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
		if( it.shapeId != curShapeId ) {
			idShapeMap.push_back( std::vector<size_t>() );
			curSizeTVecetor = &idShapeMap.back();
		}
		curShapeId = it.shapeId;
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
