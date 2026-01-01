/**
 * @file HugePreallocator.hpp
 * @brief Custom allocator using a pre-allocated huge array.
 * 
 * Provides a custom STL allocator that uses a statically allocated
 * huge array (4096x4096 uint32_t elements) as a memory pool.
 * Useful for avoiding frequent allocations in performance-critical code.
 * 
 * @warning This allocator does not actually deallocate memory and
 *          always returns the same buffer. Use with caution.
 */

#ifndef HUGEPREALLOCATOR_HPP
#define HUGEPREALLOCATOR_HPP
#include <cstdlib> // size_t, malloc, free
#include <array>
#include <cstdint>

/**
 * @brief Huge pre-allocated array (4096x4096 uint32_t elements).
 * @typedef HugeArray
 */
typedef std::array<uint32_t,4096*4096> HugeArray;

/**
 * @brief Global instance of the huge array.
 */
extern HugeArray _hugeArray;

/**
 * @brief Custom allocator using the huge pre-allocated array.
 * @tparam T Element type.
 * @struct HugePreallocator
 */
template <class T> struct HugePreallocator {
	typedef T value_type;
	HugePreallocator() noexcept { } // default ctor not required
	template <class U> HugePreallocator(const HugePreallocator<U>&) noexcept { }
	template <class U> bool operator==(
		const HugePreallocator<U>&) const noexcept { return true; }
	template <class U> bool operator!=(
		const HugePreallocator<U>&) const noexcept { return false; }

	T * allocate(const size_t n) const {
		(void)n;
		return reinterpret_cast<T*>(_hugeArray.data());
	}
	void deallocate(T * const p, size_t) const noexcept {
		(void)p;
	}
};

#endif // HUGEPREALLOCATOR_HPP
