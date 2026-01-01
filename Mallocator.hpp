/**
 * @file Mallocator.hpp
 * @brief Custom allocator using C malloc/free.
 * 
 * Provides a custom STL allocator that uses C's malloc/free functions
 * instead of C++'s new/delete. Useful for compatibility with C libraries
 * or when explicit control over memory allocation is needed.
 * 
 * @tparam T Element type.
 * @struct Mallocator
 */

#ifndef MALLOCATOR_HPP
#define MALLOCATOR_HPP
#include <cstdlib> // size_t, malloc, free
#include <new> // bad_alloc, bad_array_new_length

/**
 * @brief Custom allocator using C malloc/free.
 */
template <class T> struct Mallocator {
	typedef T value_type;
	Mallocator() noexcept { } // default ctor not required
	template <class U> Mallocator(const Mallocator<U>&) noexcept { }
	template <class U> bool operator==(
		const Mallocator<U>&) const noexcept { return true; }
	template <class U> bool operator!=(
		const Mallocator<U>&) const noexcept { return false; }

	T * allocate(const size_t n) const {
		if (n == 0) { return nullptr; }
		if (n > static_cast<size_t>(-1) / sizeof(T)) {
			throw std::bad_array_new_length();
		}
		void * const pv = malloc(n * sizeof(T));
		if (!pv) { throw std::bad_alloc(); }
		return static_cast<T *>(pv);
	}
	void deallocate(T * const p, size_t) const noexcept {
		free(p);
	}
};
#endif // MALLOCATOR_HPP
