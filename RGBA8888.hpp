/**
 * @file RGBA8888.hpp
 * @brief RGBA color structure with 8 bits per channel.
 * 
 * Provides a simple RGBA color structure with utility functions for
 * color operations (averaging, max, conversion from float vectors).
 */

#ifndef RGBA8888_HPP
#define RGBA8888_HPP
#include <cstdint>
#include <algorithm>
#include <glm/glm.hpp>

/**
 * @brief RGBA color structure (8 bits per channel).
 * @struct RGBA8888
 */
struct RGBA8888 {
	uint8_t r;
	uint8_t g;
	uint8_t b;
	uint8_t a;
	
	RGBA8888() = default;
	RGBA8888(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r), g(g), b(b), a(a) {}
	
	// Average with another RGBA8888
	inline RGBA8888 averageWith(const RGBA8888& other) const {
		return RGBA8888(
			(r + other.r + 1) / 2,  // +1 for rounding
			(g + other.g + 1) / 2,
			(b + other.b + 1) / 2,
			(a + other.a + 1) / 2
			);
	}
	
	// Max with another RGBA8888
	inline RGBA8888 maxWith(const RGBA8888& other) const {
		return RGBA8888(
			std::max(r, other.r),
			std::max(g, other.g),
			std::max(b, other.b),
			std::max(a, other.a)
			);
	}

	inline void fromFvec4(const glm::fvec4& vec) {
		r = static_cast<uint8_t>(vec.x * 255.0f);
		g = static_cast<uint8_t>(vec.y * 255.0f);
		b = static_cast<uint8_t>(vec.z * 255.0f);
		a = static_cast<uint8_t>(vec.w * 255.0f);
	}
};

#endif // RGBA8888_HPP
