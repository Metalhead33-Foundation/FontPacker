/**
 * @file Mallocator.hpp
 * @brief STL allocator backed by C `malloc`/`free`.
 *
 * Useful when memory must be shared with C libraries or when the C allocator is
 * preferred over `new`/`delete`. The allocator is stateless and all instances
 * compare equal.
 *
 * @tparam T Element type.
 */

#ifndef MALLOCATOR_HPP
#define MALLOCATOR_HPP
#include <cstdlib> // size_t, malloc, free
#include <new> // bad_alloc, bad_array_new_length

/**
 * @brief Stateless STL allocator using C `malloc`/`free`.
 * @struct Mallocator
 */
template <class T> struct Mallocator {
	typedef T value_type; ///< Allocated element type.

	/// @brief Default constructor.
	Mallocator() noexcept { }

	/**
	 * @brief Converting constructor; all instances are interchangeable.
	 * @tparam U Source element type.
	 * @param other Source allocator (unused).
	 */
	template <class U> Mallocator(const Mallocator<U>& other) noexcept { (void)other; }

	/**
	 * @brief Equality; all Mallocator instances are equal.
	 * @tparam U Other element type.
	 * @param other Other allocator (unused).
	 * @return Always true.
	 */
	template <class U> bool operator==(const Mallocator<U>& other) const noexcept { (void)other; return true; }

	/**
	 * @brief Inequality; all Mallocator instances are equal.
	 * @tparam U Other element type.
	 * @param other Other allocator (unused).
	 * @return Always false.
	 */
	template <class U> bool operator!=(const Mallocator<U>& other) const noexcept { (void)other; return false; }

	/**
	 * @brief Allocate raw storage for n elements.
	 * @param n Number of elements.
	 * @return Pointer to the storage, or nullptr when n is 0.
	 * @throws std::bad_array_new_length if the byte count overflows.
	 * @throws std::bad_alloc if malloc fails.
	 */
	T * allocate(const size_t n) const {
		if (n == 0) { return nullptr; }
		if (n > static_cast<size_t>(-1) / sizeof(T)) {
			throw std::bad_array_new_length();
		}
		void * const pv = malloc(n * sizeof(T));
		if (!pv) { throw std::bad_alloc(); }
		return static_cast<T *>(pv);
	}

	/**
	 * @brief Release storage obtained from allocate().
	 * @param p Pointer to release (may be nullptr).
	 * @param n Element count (unused).
	 */
	void deallocate(T * const p, size_t n) const noexcept {
		(void)n;
		free(p);
	}
};
#endif // MALLOCATOR_HPP
