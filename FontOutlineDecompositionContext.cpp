#include "FontOutlineDecompositionContext.hpp"
#include <climits>
#include <cstring>
#include <span>
#include <cassert>
#include <stdexcept>

template<typename T> void zeroOut(std::span<T> data) {
	std::memset(data.data(),0,data.size_bytes());
}
template<typename T, size_t N> void zeroOut(std::array<T,N>& data) {
	zeroOut(std::span<T>(data.data(),N));
}

int FontOutlineDecompositionContext::moveTo(const glm::fvec2& to)
{
	curPos = to;
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
	int32_t curShapeId = -1;
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
