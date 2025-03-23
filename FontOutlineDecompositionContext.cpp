#include "FontOutlineDecompositionContext.hpp"
#include <climits>
#include <cstring>
#include <span>

template<typename T> void zeroOut(std::span<T> data) {
	std::memset(data.data(),0,data.size_bytes());
}
template<typename T, size_t N> void zeroOut(std::array<T,N>& data) {
	zeroOut(std::span<T>(data.data(),N));
}

int FontOutlineDecompositionContext::moveTo(const glm::fvec2& to)
{
	curPos = to;
	return 0;
}

int FontOutlineDecompositionContext::lineTo(const glm::fvec2& to)
{
	edges.resize(edges.size() + 1);
	auto& edge = edges.back();
	edge.type = LINEAR;
	zeroOut(edge.points);
	edge.points[0] = curPos;
	edge.points[1] = to;
	curPos = to;
	return 0;
}

int FontOutlineDecompositionContext::conicTo(const glm::fvec2& control, const glm::fvec2& to)
{
	edges.resize(edges.size() + 1);
	auto& edge = edges.back();
	edge.type = QUADRATIC;
	zeroOut(edge.points);
	edge.points[0] = curPos;
	edge.points[1] = control;
	edge.points[2] = to;
	curPos = to;
	return 0;
}

int FontOutlineDecompositionContext::cubicTo(const glm::fvec2& control1, const glm::fvec2& control2, const glm::fvec2& to)
{
	edges.resize(edges.size() + 1);
	auto& edge = edges.back();
	edge.type = CUBIC;
	zeroOut(edge.points);
	edge.points[0] = curPos;
	edge.points[1] = control1;
	edge.points[2] = control2;
	edge.points[3] = to;
	curPos = to;
	return 0;
}

void FontOutlineDecompositionContext::translateToNewSize(unsigned int nWidth, unsigned int nHeight, unsigned int paddingX, unsigned int paddingY)
{
	const unsigned widthWithoutPadding = nWidth - (2 * paddingX);
	const unsigned heightWithoutPadding = nHeight - (2 * paddingY);
	glm::fvec2 maxDim(std::numeric_limits<float>::epsilon(), std::numeric_limits<float>::epsilon());
	for(const auto& it : edges) {
		for(const auto& jt : it.points) {
			maxDim.x = std::max(maxDim.x,jt.x);
			maxDim.y = std::max(maxDim.y,jt.y);
		}
	}
	const float scaleX = static_cast<float>(widthWithoutPadding) / maxDim.x;
	const float scaleY = static_cast<float>(heightWithoutPadding) / maxDim.y;
	for(auto& it : edges) {
		for(auto& jt : it.points) {
			jt.x = (jt.x * scaleX) + static_cast<float>(paddingX);
			jt.y = (jt.y * scaleY) + static_cast<float>(paddingY);
		}
	}
}
