/**
 * @file HugePreallocator.hpp
 * @brief STL allocator that always hands out one large static buffer.
 *
 * Used for transient GPU readback buffers where allocating and freeing a
 * multi-megabyte vector on every call would dominate the run time.
 *
 * @warning The allocator never deallocates and ignores the requested size; every
 *          allocation returns a pointer to the same `_hugeArray` buffer.
 *          Therefore only one such vector may be alive per type at a time.
 */

#ifndef HUGEPREALLOCATOR_HPP
#define HUGEPREALLOCATOR_HPP
#include <cstdlib> // size_t
#include <array>
#include <cstdint>

/**
 * @brief Backing storage: 4096 x 4096 32-bit words (64 MiB).
 * @typedef HugeArray
 */
typedef std::array<uint32_t,4096*4096> HugeArray;

/// @brief The single global backing buffer used by HugePreallocator.
extern HugeArray _hugeArray;

/**
 * @brief Stateless STL allocator returning the global huge array.
 * @tparam T Element type.
 * @struct HugePreallocator
 */
template <class T> struct HugePreallocator {
	typedef T value_type; ///< Allocated element type.

	/// @brief Default constructor.
	HugePreallocator() noexcept { }

	/**
	 * @brief Converting constructor; all instances are interchangeable.
	 * @tparam U Source element type.
	 * @param other Source allocator (unused).
	 */
	template <class U> HugePreallocator(const HugePreallocator<U>& other) noexcept { (void)other; }

	/**
	 * @brief Equality; all HugePreallocator instances are equal.
	 * @tparam U Other element type.
	 * @param other Other allocator (unused).
	 * @return Always true.
	 */
	template <class U> bool operator==(const HugePreallocator<U>& other) const noexcept { (void)other; return true; }

	/**
	 * @brief Inequality; all HugePreallocator instances are equal.
	 * @tparam U Other element type.
	 * @param other Other allocator (unused).
	 * @return Always false.
	 */
	template <class U> bool operator!=(const HugePreallocator<U>& other) const noexcept { (void)other; return false; }

	/**
	 * @brief Return the shared buffer reinterpreted as T*.
	 * @param n Requested element count; ignored (the buffer is fixed-size).
	 * @return Pointer to the beginning of `_hugeArray`.
	 * @warning No bounds checking is performed.
	 */
	T * allocate(const size_t n) const {
		(void)n;
		return reinterpret_cast<T*>(_hugeArray.data());
	}

	/**
	 * @brief No-op; the shared buffer is never released.
	 * @param p Ignored.
	 * @param n Ignored.
	 */
	void deallocate(T * const p, size_t n) const noexcept {
		(void)p;
		(void)n;
	}
};

#endif // HUGEPREALLOCATOR_HPP
