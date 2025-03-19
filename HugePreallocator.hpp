#ifndef HUGEPREALLOCATOR_HPP
#define HUGEPREALLOCATOR_HPP
#include <cstdlib> // size_t, malloc, free
#include <array>
#include <cstdint>
typedef std::array<uint32_t,4096*4096> HugeArray;
extern HugeArray _hugeArray;
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
