#pragma once

#include "core/typedefs.h"

#include <cassert>
#include <memory>
#include <new>
#include <utility>
#include <vector>

namespace rover {

// A growable arena allocator that allocates from chunks.
// Suitable for allocations whose lifetimes are collectively bounded (e.g. a level load).
// NOT thread-safe — caller must synchronize externally if shared across threads.
class ArenaAllocator {
public:
	explicit ArenaAllocator(usize chunk_size = 64 * 1024);
	~ArenaAllocator() = default;

	ArenaAllocator(const ArenaAllocator&) = delete;
	ArenaAllocator& operator=(const ArenaAllocator&) = delete;

	ArenaAllocator(ArenaAllocator&& other) noexcept;
	ArenaAllocator& operator=(ArenaAllocator&& other) noexcept;

	void* allocate(usize size, usize alignment = alignof(std::max_align_t));
	void reset();
	void release();

	template <typename T, typename... Args>
	T* construct(Args&&... args) {
		void* mem = allocate(sizeof(T), alignof(T));
		if (!mem) return nullptr;
		return new (mem) T(std::forward<Args>(args)...);
	}

private:
	struct Chunk {
		std::unique_ptr<u8[]> buffer;
		usize size = 0;
		usize offset = 0;
	};

	void add_chunk(usize min_size);

	std::vector<Chunk> chunks_;
	usize chunk_size_;
	usize current_chunk_ = 0;
};

} // namespace rover
