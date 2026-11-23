/**
 * @file SdfGenerationContextSoft.cpp
 * @brief CPU implementation of bitmap/outline SDF and MSDF generation.
 */

#include "SdfGenerationContextSoft.hpp"
#include "RGBA8888.hpp"

#include <glm/glm.hpp>
#include <QImage>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <vector>

namespace {

/// @brief Epsilon used when every pixel of one side has zero distance.
constexpr float kDistanceEpsilon = std::numeric_limits<float>::epsilon();

/// @brief Apply the cubic gamma adjustment used by the GPU backends.
float gammaAdjust(float x)
{
	const float d = x - 0.5f;
	return 0.5f + 2.0f * d * d * d + 0.5f * d;
}

/// @brief Return the requested SDF search half-radius in X.
int halfSamplesX(const SDFGenerationArguments& args)
{
	return args.samples_to_check_x ? static_cast<int>(args.samples_to_check_x / 2)
								  : static_cast<int>(args.padding);
}

/// @brief Return the requested SDF search half-radius in Y.
int halfSamplesY(const SDFGenerationArguments& args)
{
	return args.samples_to_check_y ? static_cast<int>(args.samples_to_check_y / 2)
								  : static_cast<int>(args.padding);
}

/// @brief Maximum distance used by the SDF normalization pass.
float maxSdfDistance(const SDFGenerationArguments& args)
{
	const float x = static_cast<float>(halfSamplesX(args));
	const float y = static_cast<float>(halfSamplesY(args));
	return args.distType == DistanceType::Euclidean ? std::sqrt(x * x + y * y) : x + y;
}

/// @brief Distance between two integer sample positions.
float sampleDistance(const SDFGenerationArguments& args, int dx, int dy)
{
	const float fx = static_cast<float>(dx);
	const float fy = static_cast<float>(dy);
	return args.distType == DistanceType::Euclidean ? std::sqrt(fx * fx + fy * fy)
												 : std::abs(fx) + std::abs(fy);
}

/// @brief Convert the bitmap to a binary mask using the GPU threshold.
std::vector<uint8_t> makeBitmapMask(const QImage& source, uint32_t size)
{
	const QImage grayscale = source.format() == QImage::Format_Grayscale8
			? source : source.convertToFormat(QImage::Format_Grayscale8);
	if(grayscale.width() != static_cast<int>(size) || grayscale.height() != static_cast<int>(size)) {
		throw std::runtime_error("Software bitmap input does not match the internal processing size.");
	}
	std::vector<uint8_t> mask(static_cast<size_t>(size) * size);
	for(uint32_t y = 0; y < size; ++y) {
		const uchar* row = grayscale.constScanLine(static_cast<int>(y));
		const size_t offset = static_cast<size_t>(y) * size;
		for(uint32_t x = 0; x < size; ++x) mask[offset + x] = row[x] >= 128 ? 1 : 0;
	}
	return mask;
}

/// @brief Compute the single-channel raw SDF of a bitmap mask.
void computeBitmapSdf(const std::vector<uint8_t>& mask, uint32_t size,
					  const SDFGenerationArguments& args, std::vector<float>& raw)
{
	const int hx = halfSamplesX(args);
	const int hy = halfSamplesY(args);
	const float maxDistance = maxSdfDistance(args);
	raw.resize(static_cast<size_t>(size) * size);
#pragma omp parallel for schedule(static)
	for(int y = 0; y < static_cast<int>(size); ++y) {
		for(int x = 0; x < static_cast<int>(size); ++x) {
			const size_t index = static_cast<size_t>(y) * size + static_cast<size_t>(x);
			const bool inside = mask[index] != 0;
			float minDistance = maxDistance;
			const int minX = std::max(0, x - hx);
			const int maxX = std::min(static_cast<int>(size) - 1, x + hx);
			const int minY = std::max(0, y - hy);
			const int maxY = std::min(static_cast<int>(size) - 1, y + hy);
			for(int sy = minY; sy <= maxY; ++sy) {
				const size_t row = static_cast<size_t>(sy) * size;
				for(int sx = minX; sx <= maxX; ++sx) {
					if((mask[row + static_cast<size_t>(sx)] != 0) == inside) continue;
					minDistance = std::min(minDistance, sampleDistance(args, sx - x, sy - y));
				}
			}
			raw[index] = maxDistance > 0.0f ? minDistance / maxDistance : 0.0f;
		}
	}
}

/// @brief Compute the simplified three-channel bitmap MSDF used by the GPU bitmap path.
void computeBitmapMsdf(const std::vector<uint8_t>& mask, uint32_t size,
					   const SDFGenerationArguments& args, std::vector<glm::fvec4>& raw)
{
	const int hx = halfSamplesX(args);
	const int hy = halfSamplesY(args);
	const float maxDistance = std::sqrt(static_cast<float>(hx) * hx + static_cast<float>(hy) * hy);
	const bool needsAlpha = args.type == MSDFA;
	raw.resize(static_cast<size_t>(size) * size);
#pragma omp parallel for schedule(static)
	for(int y = 0; y < static_cast<int>(size); ++y) {
		for(int x = 0; x < static_cast<int>(size); ++x) {
			const size_t index = static_cast<size_t>(y) * size + static_cast<size_t>(x);
			const bool inside = mask[index] != 0;
			glm::vec3 minDistance(maxDistance);
			for(int ox = -hx; ox <= hx; ++ox) {
				const int sx = std::clamp(x + ox, 0, static_cast<int>(size) - 1);
				if((mask[static_cast<size_t>(y) * size + static_cast<size_t>(sx)] != 0) != inside) {
					minDistance.r = std::min(minDistance.r, std::abs(static_cast<float>(ox)));
				}
			}
			for(int oy = -hy; oy <= hy; ++oy) {
				const int sy = std::clamp(y + oy, 0, static_cast<int>(size) - 1);
				if((mask[static_cast<size_t>(sy) * size + static_cast<size_t>(x)] != 0) != inside) {
					minDistance.g = std::min(minDistance.g, std::abs(static_cast<float>(oy)));
				}
			}
			const int maxStep = std::min(hx, hy);
			for(int offset = -maxStep; offset <= maxStep; ++offset) {
				const int sx = std::clamp(x + offset, 0, static_cast<int>(size) - 1);
				const int sy = std::clamp(y + offset, 0, static_cast<int>(size) - 1);
				if((mask[static_cast<size_t>(sy) * size + static_cast<size_t>(sx)] != 0) != inside) {
					minDistance.b = std::min(minDistance.b, std::sqrt(2.0f * static_cast<float>(offset) * offset));
				}
			}

			float alpha = 1.0f;
			if(needsAlpha) {
				float minAlpha = maxSdfDistance(args);
				const int minX = std::max(0, x - hx);
				const int maxX = std::min(static_cast<int>(size) - 1, x + hx);
				const int minY = std::max(0, y - hy);
				const int maxY = std::min(static_cast<int>(size) - 1, y + hy);
				for(int sy = minY; sy <= maxY; ++sy) {
					const size_t row = static_cast<size_t>(sy) * size;
					for(int sx = minX; sx <= maxX; ++sx) {
						if((mask[row + static_cast<size_t>(sx)] != 0) == inside) continue;
						minAlpha = std::min(minAlpha, sampleDistance(args, sx - x, sy - y));
					}
				}
				const float alphaMax = maxSdfDistance(args);
				alpha = alphaMax > 0.0f ? minAlpha / alphaMax : 0.0f;
			}
			raw[index] = glm::fvec4(maxDistance > 0.0f ? minDistance / maxDistance : glm::vec3(0.0f), alpha);
		}
	}
}

/// @brief Normalize and quantize a single-channel SDF.
QImage finalizeSdf(const std::vector<float>& raw, const std::vector<uint8_t>& inside,
				   uint32_t size, const SDFGenerationArguments& args)
{
	float maxIn = kDistanceEpsilon;
	float maxOut = kDistanceEpsilon;
	for(size_t i = 0; i < raw.size(); ++i) {
		if(inside[i]) maxIn = std::max(maxIn, std::abs(raw[i]));
		else maxOut = std::max(maxOut, std::abs(raw[i]));
	}
	std::vector<float> values = raw;
	for(size_t i = 0; i < values.size(); ++i) {
		if(inside[i]) {
			values[i] /= maxIn;
			values[i] = 0.5f + values[i] * 0.5f;
		} else {
			values[i] /= maxOut;
			values[i] = 0.5f - values[i] * 0.5f;
		}
		if(args.invert) values[i] = 1.0f - values[i];
	}
	if(args.midpointAdjustment.has_value()) {
		const float divisor = args.midpointAdjustment.value_or(1.0f);
		for(float& value : values) value = std::clamp(value / divisor, 0.0f, 1.0f);
	}
	if(args.gammaCorrect) {
		for(float& value : values) value = gammaAdjust(value);
	}

	QImage image(static_cast<int>(size), static_cast<int>(size), QImage::Format_Grayscale8);
	for(uint32_t y = 0; y < size; ++y) {
		uchar* row = image.scanLine(static_cast<int>(y));
		const size_t offset = static_cast<size_t>(y) * size;
		for(uint32_t x = 0; x < size; ++x) row[x] = static_cast<uchar>(values[offset + x] * 255.0f);
	}
	return image;
}

/// @brief Normalize and quantize an MSDF/MSDFA image.
QImage finalizeMsdf(const std::vector<glm::fvec4>& raw, uint32_t size,
					const SDFGenerationArguments& args)
{
	glm::fvec4 minDist(std::numeric_limits<float>::max());
	glm::fvec4 maxDist(std::numeric_limits<float>::lowest());
	for(const glm::fvec4& value : raw) {
		for(int c = 0; c < 4; ++c) {
			minDist[c] = std::min(minDist[c], value[c]);
			maxDist[c] = std::max(maxDist[c], value[c]);
		}
	}
	std::vector<glm::fvec4> values = raw;
	for(glm::fvec4& value : values) {
		for(int c = 0; c < 4; ++c) {
			if(value[c] >= 0.0f) value[c] /= maxDist[c];
			else value[c] = (value[c] / minDist[c]) * -1.0f;
			value[c] = value[c] * 0.5f + 0.5f;
			if(args.invert) value[c] = 1.0f - value[c];
		}
	}
	if(args.midpointAdjustment.has_value()) {
		const float divisor = args.midpointAdjustment.value_or(1.0f);
		for(glm::fvec4& value : values) {
			for(int c = 0; c < 4; ++c) value[c] = std::clamp(value[c] / divisor, 0.0f, 1.0f);
		}
	}
	if(args.gammaCorrect) {
		for(glm::fvec4& value : values) {
			for(int c = 0; c < 4; ++c) value[c] = gammaAdjust(value[c]);
		}
	}
	if(args.type != MSDFA) {
		for(glm::fvec4& value : values) value.w = 1.0f;
	}

	QImage image(static_cast<int>(size), static_cast<int>(size), QImage::Format_RGBA8888);
	for(uint32_t y = 0; y < size; ++y) {
		RGBA8888* row = reinterpret_cast<RGBA8888*>(image.scanLine(static_cast<int>(y)));
		for(uint32_t x = 0; x < size; ++x) row[x].fromFvec4(values[static_cast<size_t>(y) * size + x]);
	}
	return image;
}

/// @brief Distance between two float points under the configured metric.
float pointDistance(DistanceType distanceType, glm::vec2 a, glm::vec2 b)
{
const glm::vec2 difference = a - b;
return distanceType == DistanceType::Euclidean
? glm::length(difference)
: std::abs(difference.x) + std::abs(difference.y);
}

/// @brief Distance from a point to a finite line segment.
float distanceToLineSegment(float maxDistance, glm::vec2 p, glm::vec2 p1, glm::vec2 p2,
DistanceType distanceType)
{
(void)maxDistance;
const glm::vec2 v = p2 - p1;
const glm::vec2 w = p - p1;
const float c1 = glm::dot(w, v);
if(c1 <= 0.0f) return pointDistance(distanceType, p, p1);
const float c2 = glm::dot(v, v);
if(c2 <= c1) return pointDistance(distanceType, p, p2);
return pointDistance(distanceType, p, p1 + (c1 / c2) * v);
}

/// @brief Approximate distance from a point to a quadratic Bezier.
float distanceToQuadraticBezier(float maxDistance, glm::vec2 p, glm::vec2 p1,
glm::vec2 p2, glm::vec2 p3, int width, DistanceType distanceType)
{
float minDistance = maxDistance;
const int steps = std::max(1, width / 4);
for(int i = 0; i <= steps; ++i) {
const float t = static_cast<float>(i) / static_cast<float>(steps);
const float t1 = 1.0f - t;
const glm::vec2 point = t1 * t1 * p1 + 2.0f * t1 * t * p2 + t * t * p3;
minDistance = std::min(minDistance, pointDistance(distanceType, p, point));
}
return minDistance;
}

/// @brief Approximate distance from a point to a cubic Bezier.
float distanceToCubicBezier(float maxDistance, glm::vec2 p, glm::vec2 p1, glm::vec2 p2,
glm::vec2 p3, glm::vec2 p4, int width, DistanceType distanceType)
{
float minDistance = maxDistance;
const int steps = std::max(1, width / 4);
for(int i = 0; i <= steps; ++i) {
const float t = static_cast<float>(i) / static_cast<float>(steps);
const float t1 = 1.0f - t;
const glm::vec2 point = t1 * t1 * t1 * p1 + 3.0f * t1 * t1 * t * p2
+ 3.0f * t1 * t * t * p3 + t * t * t * p4;
minDistance = std::min(minDistance, pointDistance(distanceType, p, point));
}
return minDistance;
}

/// @brief Winding contribution of one edge.
int windingFor(glm::vec2 pos, const std::vector<EdgeSegment>& edges, size_t index, int curveSteps)
{
	const EdgeSegment& edge = edges[index];
	int winding = 0;
	if(edge.type == EdgeType::LINEAR) {
		const glm::vec2 p1 = edge.points[0];
		const glm::vec2 p2 = edge.points[1];
		if((p1.y <= pos.y && p2.y > pos.y) || (p1.y > pos.y && p2.y <= pos.y)) {
			const float intersectX = p1.x + (pos.y - p1.y) * (p2.x - p1.x) / (p2.y - p1.y);
			if(pos.x < intersectX) winding += p1.y <= pos.y ? 1 : -1;
		}
	} else if(edge.type == EdgeType::QUADRATIC || edge.type == EdgeType::CUBIC) {
		glm::vec2 previous = edge.points[0];
		const int steps = std::max(1, curveSteps);
		for(int j = 1; j <= steps; ++j) {
			const float t = static_cast<float>(j) / static_cast<float>(steps);
			const float t1 = 1.0f - t;
			glm::vec2 current;
			if(edge.type == EdgeType::QUADRATIC) {
				current = t1 * t1 * edge.points[0] + 2.0f * t1 * t * edge.points[1] + t * t * edge.points[2];
			} else {
				current = t1 * t1 * t1 * edge.points[0] + 3.0f * t1 * t1 * t * edge.points[1]
						+ 3.0f * t1 * t * t * edge.points[2] + t * t * t * edge.points[3];
			}
			if((previous.y <= pos.y && current.y > pos.y) || (previous.y > pos.y && current.y <= pos.y)) {
				const float intersectX = previous.x + (pos.y - previous.y) * (current.x - previous.x) / (current.y - previous.y);
				if(pos.x < intersectX) winding += previous.y <= pos.y ? 1 : -1;
			}
			previous = current;
		}
	}
	return winding;
}

/// @brief Total winding number across all edges.
int winding(glm::vec2 pos, const std::vector<EdgeSegment>& edges, int curveSteps)
{
	int result = 0;
	for(size_t i = 0; i < edges.size(); ++i) result += windingFor(pos, edges, i, curveSteps);
	return result;
}

/// @brief Compute one single-channel outline SDF pixel.
float outlineDistance(glm::vec2 pos, const std::vector<EdgeSegment>& edges, int width,
					  float maxDistance, DistanceType distanceType)
{
	float minDistance = maxDistance;
	for(const EdgeSegment& edge : edges) {
		float distance = maxDistance;
		switch(edge.type) {
			case EdgeType::LINEAR:
				distance = distanceToLineSegment(maxDistance, pos, edge.points[0], edge.points[1], distanceType);
				break;
			case EdgeType::QUADRATIC:
				distance = distanceToQuadraticBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2], width, distanceType);
				break;
			case EdgeType::CUBIC:
				distance = distanceToCubicBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2], edge.points[3], width, distanceType);
				break;
		}
		minDistance = std::min(minDistance, distance);
	}
	return minDistance;
}

/// @brief Unpack an RRGGBBXX edge color into normalized RGB.
glm::vec3 unpackRgb(uint32_t packed)
{
	constexpr float inv255 = 1.0f / 255.0f;
	return glm::vec3(static_cast<float>((packed >> 16) & 0xFFu) * inv255,
					 static_cast<float>((packed >> 8) & 0xFFu) * inv255,
					 static_cast<float>(packed & 0xFFu) * inv255);
}

/// @brief Distance from a point to a line, used by the pseudo-distance pass.
float distanceToLinePseudo(glm::vec2 p, glm::vec2 a, glm::vec2 b, DistanceType distanceType)
{
	const glm::vec2 pa = p - a;
	const glm::vec2 ba = b - a;
	const float h = std::clamp(glm::dot(pa, ba) / glm::dot(ba, ba), 0.0f, 1.0f);
	return pointDistance(distanceType, p, a + h * ba);
}

/// @brief Closest pseudo-distance on a quadratic Bezier.
struct PseudoResult {
	float distance = 1e20f;
	glm::vec2 point{};
	float t = 0.0f;
};

PseudoResult distanceToQuadraticPseudo(glm::vec2 p, glm::vec2 p0, glm::vec2 p1,
   glm::vec2 p2, int subdivisions, DistanceType distanceType)
{
	PseudoResult result;
	result.point = p0;
	const int steps = std::max(1, subdivisions);
	const float reciprocal = 1.0f / static_cast<float>(steps);
	glm::vec2 previous = p0;
	for(int i = 1; i <= steps; ++i) {
		const float t = static_cast<float>(i) * reciprocal;
		const glm::vec2 current = glm::mix(glm::mix(p0, p1, t), glm::mix(p1, p2, t), t);
		const float distance = distanceToLinePseudo(p, previous, current, distanceType);
		if(std::abs(distance) <= result.distance) {
			result.distance = distance;
			result.point = 0.5f * (previous + current);
			result.t = t - 0.5f * reciprocal;
		}
		previous = current;
	}
	return result;
}

PseudoResult distanceToCubicPseudo(glm::vec2 p, glm::vec2 p0, glm::vec2 p1, glm::vec2 p2,
   glm::vec2 p3, int subdivisions, DistanceType distanceType)
{
	PseudoResult result;
	result.point = p0;
	const int steps = std::max(1, subdivisions);
	const float reciprocal = 1.0f / static_cast<float>(steps);
	glm::vec2 previous = p0;
	for(int i = 1; i <= steps; ++i) {
		const float t = static_cast<float>(i) * reciprocal;
		const glm::vec2 a = glm::mix(p0, p1, t);
		const glm::vec2 b = glm::mix(p1, p2, t);
		const glm::vec2 c = glm::mix(p2, p3, t);
		const glm::vec2 current = glm::mix(glm::mix(a, b, t), glm::mix(b, c, t), t);
		const float distance = distanceToLinePseudo(p, previous, current, distanceType);
		if(std::abs(distance) <= result.distance) {
			result.distance = distance;
			result.point = 0.5f * (previous + current);
			result.t = t - 0.5f * reciprocal;
		}
		previous = current;
	}
	return result;
}

/// @brief Signed pseudo-distance from a point to one edge.
float signedDistancePseudo(glm::vec2 p, const EdgeSegment& edge, int width, DistanceType distanceType)
{
	if(edge.type == EdgeType::LINEAR) {
		const glm::vec2 a = edge.points[0];
		const glm::vec2 b = edge.points[1];
		const float distance = distanceToLinePseudo(p, a, b, distanceType);
		const glm::vec2 direction = glm::normalize(b - a);
		return glm::dot(p - a, glm::vec2(-direction.y, direction.x)) >= 0.0f ? distance : -distance;
	}
	if(edge.type == EdgeType::QUADRATIC) {
		const int steps = std::max(1, width / 4);
		const PseudoResult result = distanceToQuadraticPseudo(p, edge.points[0], edge.points[1], edge.points[2], steps, distanceType);
		const glm::vec2 tangent = 2.0f * glm::mix(edge.points[1] - edge.points[0], edge.points[2] - edge.points[1], result.t);
		if(glm::dot(tangent, tangent) < 1e-6f) return result.distance;
		const float sign = tangent.x * (p.y - result.point.y) - tangent.y * (p.x - result.point.x);
		return (sign >= 0.0f ? 1.0f : -1.0f) * result.distance;
	}
	if(edge.type == EdgeType::CUBIC) {
		const int steps = std::max(1, width / 4);
		const PseudoResult result = distanceToCubicPseudo(p, edge.points[0], edge.points[1], edge.points[2], edge.points[3], steps, distanceType);
		const glm::vec2 tangent = 3.0f * glm::mix(glm::mix(edge.points[1] - edge.points[0], edge.points[2] - edge.points[1], result.t),
 glm::mix(edge.points[2] - edge.points[1], edge.points[3] - edge.points[2], result.t), result.t);
		if(glm::dot(tangent, tangent) < 1e-6f) return result.distance;
		const float sign = tangent.x * (p.y - result.point.y) - tangent.y * (p.x - result.point.x);
		return (sign >= 0.0f ? 1.0f : -1.0f) * result.distance;
	}
	return 1e20f;
}

/// @brief Compute one MSDF pixel from vector edges.
glm::fvec4 outlineMsdfPixel(glm::vec2 pos, const std::vector<EdgeSegment>& edges, int width,
							float realMaxDistance, DistanceType distanceType)
{
	float maxDistance = std::numeric_limits<float>::max();
	glm::fvec4 minDistance(maxDistance);
	glm::ivec4 closestEdge(-1);
	for(size_t i = 0; i < edges.size(); ++i) {
		const EdgeSegment& edge = edges[i];
		const glm::vec3 color = unpackRgb(edge.clr);
		float distance = maxDistance;
		switch(edge.type) {
			case EdgeType::LINEAR:
				distance = distanceToLineSegment(maxDistance, pos, edge.points[0], edge.points[1], distanceType);
				break;
			case EdgeType::QUADRATIC:
				distance = distanceToQuadraticBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2], width, distanceType);
				break;
			case EdgeType::CUBIC:
				distance = distanceToCubicBezier(maxDistance, pos, edge.points[0], edge.points[1], edge.points[2], edge.points[3], width, distanceType);
				break;
		}
		if(distance <= minDistance.w) {
			minDistance.w = distance;
			closestEdge.w = static_cast<int>(i);
		}
		for(int channel = 0; channel < 3; ++channel) {
			if(color[channel] >= 0.003921568627451f && distance <= minDistance[channel]) {
				minDistance[channel] = distance;
				closestEdge[channel] = static_cast<int>(i);
			}
		}
	}

	glm::ivec4 windingNumber(0);
	const int curveSteps = std::max(1, width / 4);
	for(int channel = 0; channel < 3; ++channel) {
		if(closestEdge[channel] >= 0) windingNumber[channel] = windingFor(pos, edges, static_cast<size_t>(closestEdge[channel]), curveSteps);
	}
	windingNumber.w = winding(pos, edges, curveSteps);

	glm::fvec4 result(maxDistance);
	for(int channel = 0; channel < 3; ++channel) {
		if(closestEdge[channel] >= 0) result[channel] = signedDistancePseudo(pos, edges[static_cast<size_t>(closestEdge[channel])], width, distanceType);
	}
	result.w = windingNumber.w <= 0 ? -minDistance.w : minDistance.w;
	for(int channel = 0; channel < 4; ++channel) result[channel] = std::clamp(result[channel], -realMaxDistance, realMaxDistance);
	result.w = std::abs(result.w);

	if(closestEdge.w >= 0) {
		const glm::vec3 color = unpackRgb(edges[static_cast<size_t>(closestEdge.w)].clr);
		const float sign = windingNumber.w <= 0 ? -1.0f : 1.0f;
		for(int channel = 0; channel < 3; ++channel) {
			if(color[channel] >= 0.003921568627451f) result[channel] = std::abs(result[channel]) * sign;
		}
	}
	if(windingNumber.w == 0) result.w = -result.w;
	return result;
}

} // namespace

SdfGenerationContextSoft::SdfGenerationContextSoft() = default;

QImage SdfGenerationContextSoft::produceBitmapSdf(const QImage& source, const SDFGenerationArguments& args)
{
	const uint32_t size = args.internalProcessSize;
	if(size == 0) throw std::runtime_error("Internal processing size must be greater than zero.");
	const std::vector<uint8_t> mask = makeBitmapMask(source, size);
	if(args.type == SDF) {
		std::vector<float> raw;
		computeBitmapSdf(mask, size, args, raw);
		return finalizeSdf(raw, mask, size, args);
	}
	std::vector<glm::fvec4> raw;
	computeBitmapMsdf(mask, size, args, raw);
	return finalizeMsdf(raw, size, args);
}

QImage SdfGenerationContextSoft::produceOutlineSdf(const FontOutlineDecompositionContext& source,
												 const SDFGenerationArguments& args)
{
	const uint32_t size = args.internalProcessSize;
	if(size == 0) throw std::runtime_error("Internal processing size must be greater than zero.");
	if(source.edges.empty()) throw std::runtime_error("Software outline input contains no edges.");

	if(args.type == SDF) {
		const float maxDistance = maxSdfDistance(args);
		std::vector<float> raw(static_cast<size_t>(size) * size);
		std::vector<uint8_t> inside(raw.size());
#pragma omp parallel for schedule(static)
		for(int y = 0; y < static_cast<int>(size); ++y) {
			for(int x = 0; x < static_cast<int>(size); ++x) {
				const size_t index = static_cast<size_t>(y) * size + static_cast<size_t>(x);
				const glm::vec2 pos(static_cast<float>(x) + 0.5f, static_cast<float>(y) + 0.5f);
				raw[index] = outlineDistance(pos, source.edges, static_cast<int>(size), maxDistance, args.distType);
				inside[index] = winding(pos, source.edges, 8) != 0 ? 1 : 0;
			}
		}
		return finalizeSdf(raw, inside, size, args);
	}

	const float x = static_cast<float>(halfSamplesX(args));
	const float y = static_cast<float>(halfSamplesY(args));
	const float realMaxDistance = args.distType == DistanceType::Euclidean ? std::sqrt(x * x + y * y) : x + y;
	std::vector<glm::fvec4> raw(static_cast<size_t>(size) * size);
#pragma omp parallel for schedule(static)
	for(int py = 0; py < static_cast<int>(size); ++py) {
		for(int px = 0; px < static_cast<int>(size); ++px) {
			const size_t index = static_cast<size_t>(py) * size + static_cast<size_t>(px);
			const glm::vec2 pos(static_cast<float>(px) + 0.5f, static_cast<float>(py) + 0.5f);
			raw[index] = outlineMsdfPixel(pos, source.edges, static_cast<int>(size), realMaxDistance, args.distType);
		}
	}
	return finalizeMsdf(raw, size, args);
}
