#include "core/allocator/pool_allocator.h"

#include <algorithm>
#include <cstring>

namespace rover {

PoolAllocator::PoolAllocator(usize block_size, usize block_count)
	: block_size_(std::max(block_size, sizeof(FreeNode))),
	  block_count_(block_count),
	  free_count_(block_count) {
	assert(block_count > 0);
	buffer_ = std::make_unique<u8[]>(block_size_ * block_count_);
	build_free_list();
}

PoolAllocator::PoolAllocator(PoolAllocator&& other) noexcept
	: buffer_(std::move(other.buffer_)),
	  block_size_(other.block_size_),
	  block_count_(other.block_count_),
	  free_count_(other.free_count_),
	  free_head_(other.free_head_) {
	other.block_size_ = 0;
	other.block_count_ = 0;
	other.free_count_ = 0;
	other.free_head_ = nullptr;
}

PoolAllocator& PoolAllocator::operator=(PoolAllocator&& other) noexcept {
	if (this != &other) {
		buffer_ = std::move(other.buffer_);
		block_size_ = other.block_size_;
		block_count_ = other.block_count_;
		free_count_ = other.free_count_;
		free_head_ = other.free_head_;
		other.block_size_ = 0;
		other.block_count_ = 0;
		other.free_count_ = 0;
		other.free_head_ = nullptr;
	}
	return *this;
}

void PoolAllocator::build_free_list() {
	free_head_ = nullptr;
	for (usize i = block_count_; i > 0; --i) {
		u8* block = buffer_.get() + (i - 1) * block_size_;
		auto* node = reinterpret_cast<FreeNode*>(block);
		node->next = free_head_;
		free_head_ = node;
	}
}

void* PoolAllocator::allocate() {
	if (!free_head_) {
		return nullptr;
	}

	FreeNode* node = free_head_;
	free_head_ = node->next;
	--free_count_;
	return static_cast<void*>(node);
}

void PoolAllocator::deallocate(void* ptr) {
	assert(ptr != nullptr);
	assert(ptr >= buffer_.get() && ptr < buffer_.get() + block_size_ * block_count_);

	auto* node = static_cast<FreeNode*>(ptr);
	node->next = free_head_;
	free_head_ = node;
	++free_count_;
}

} // namespace rover
