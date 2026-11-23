/**
 * @file RGBA8888.hpp
 * @brief Simple 8-bit-per-channel RGBA pixel with blending helpers.
 *
 * Used by the downsampling code and the OpenGL readback path. The structure is
 * a plain byte tuple; it is never serialized directly.
 */

#ifndef RGBA8888_HPP
#define RGBA8888_HPP
#include <cstdint>
#include <algorithm>
#include <glm/glm.hpp>

/**
 * @brief RGBA color/pixel with 8 bits per channel.
 * @struct RGBA8888
 */
struct RGBA8888 {
	uint8_t r; ///< Red channel.
	uint8_t g; ///< Green channel.
	uint8_t b; ///< Blue channel.
	uint8_t a; ///< Alpha channel.

	/// @brief Default constructor; leaves the channels uninitialised.
	RGBA8888() = default;

	/**
	 * @brief Construct from explicit channel values.
	 * @param r Red channel.
	 * @param g Green channel.
	 * @param b Blue channel.
	 * @param a Alpha channel.
	 */
	RGBA8888(uint8_t r, uint8_t g, uint8_t b, uint8_t a) : r(r), g(g), b(b), a(a) {}

	/**
	 * @brief Component-wise average with another pixel, rounded.
	 * @param other Pixel to average with.
	 * @return The averaged pixel.
	 */
	inline RGBA8888 averageWith(const RGBA8888& other) const {
		return RGBA8888(
			(r + other.r + 1) / 2,  // +1 for rounding
			(g + other.g + 1) / 2,
			(b + other.b + 1) / 2,
			(a + other.a + 1) / 2
			);
	}

	/**
	 * @brief Component-wise maximum with another pixel.
	 * @param other Pixel to compare with.
	 * @return The per-channel maximum.
	 */
	inline RGBA8888 maxWith(const RGBA8888& other) const {
		return RGBA8888(
			std::max(r, other.r),
			std::max(g, other.g),
			std::max(b, other.b),
			std::max(a, other.a)
			);
	}

	/**
	 * @brief Set the channels from normalized floats.
	 * @param vec Vector with components in [0, 1]; each is multiplied by 255.
	 */
	inline void fromFvec4(const glm::fvec4& vec) {
		r = static_cast<uint8_t>(vec.x * 255.0f);
		g = static_cast<uint8_t>(vec.y * 255.0f);
		b = static_cast<uint8_t>(vec.z * 255.0f);
		a = static_cast<uint8_t>(vec.w * 255.0f);
	}
};

#endif // RGBA8888_HPP
