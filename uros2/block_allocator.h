#ifndef BLOCK_ALLOCATOR_H
#define BLOCK_ALLOCATOR_H

#include <cstdlib>

#include "etl/imemory_block_allocator.h"
#include "etl/ipool.h"
#include "etl/memory.h"
#include "etl/alignment.h"

namespace uros {

//*************************************************************************
/// ipool 的公共包装类（因为 ipool 的构造函数是 protected）
//*************************************************************************
class public_ipool : public etl::ipool {
public:
    public_ipool(char* p_buffer, uint32_t item_size, uint32_t max_size)
        : ipool(p_buffer, item_size, max_size) {}
};

//*************************************************************************
/// 运行时配置的内存块分配器
/// 使用 ipool 管理内存，使用 etl::unique_ptr
//*************************************************************************
class block_allocator : public etl::imemory_block_allocator
{
public: 
    //*************************************************************************
    /// 构造函数 - 运行时指定块大小、对齐和数量
    //*************************************************************************
    block_allocator(size_t block_size, 
                    size_t alignment, 
                    size_t num_blocks)
        : raw_buffer_(ETL_NULLPTR)
        , aligned_buffer_(ETL_NULLPTR)
        , raw_block_size_(block_size)
        , alignment_(alignment)
        , num_blocks_(num_blocks)
        , aligned_block_size_(calculate_aligned_block_size(block_size, alignment))
        , peak_allocated_count_(0)
    {
        // 分配对齐的缓冲区
        allocate_aligned_buffer(aligned_block_size_, alignment, num_blocks);
        
        // 使用 etl::unique_ptr 管理 ipool
        pool_.reset(new public_ipool(aligned_buffer_, 
                                     static_cast<uint32_t>(aligned_block_size_), 
                                     static_cast<uint32_t>(num_blocks)));
    }

    //*************************************************************************
    /// 析构函数
    //*************************************************************************
    ~block_allocator()
    {
        // etl::unique_ptr 会自动清理 ipool
        if (raw_buffer_ != ETL_NULLPTR)
        {
            ::free(raw_buffer_);
        }
    }

    // 禁止拷贝
    block_allocator(const block_allocator&) ETL_DELETE;
    block_allocator& operator=(const block_allocator&) ETL_DELETE;

    //*************************************************************************
    /// 获取配置信息
    //*************************************************************************
    size_t get_block_size() const { return raw_block_size_; }
    size_t get_aligned_block_size() const { return aligned_block_size_; }
    size_t get_alignment() const { return alignment_; }
    size_t get_capacity() const { return num_blocks_; }
    size_t get_available() const { return pool_->available(); }
    size_t get_allocated_count() const { return pool_->size(); }
    size_t get_peak_allocated_count() const { return peak_allocated_count_; }

protected:
    //*************************************************************************
    /// 分配内存块 (重写虚函数)
    //*************************************************************************
    void* allocate_block(size_t required_size, size_t required_alignment) ETL_OVERRIDE
    {
        // 检查大小和对齐是否匹配
        if (required_alignment <= alignment_ && 
            required_size <= raw_block_size_ && 
            !pool_->full())
        {
            // 使用 char 是安全的：
            // ipool 总是返回 aligned_block_size_ 大小的内存
            void* p = pool_->allocate<char>();
            
            // 更新峰值分配计数
            peak_allocated_count_ = etl::max(peak_allocated_count_, pool_->size());
            
            return p;
        }
        
        return ETL_NULLPTR;
    }

    //*************************************************************************
    /// 释放内存块 (重写虚函数)
    //*************************************************************************
    bool release_block(const void* const pblock) ETL_OVERRIDE
    {
        if (pool_->is_in_pool(pblock))
        {
            pool_->release(pblock);
            return true;
        }
        return false;
    }

    //*************************************************************************
    /// 检查块所有权 (重写虚函数)
    //*************************************************************************
    bool is_owner_of_block(const void* const pblock) const ETL_OVERRIDE
    {
        return pool_->is_in_pool(pblock);
    }

private:
    //*************************************************************************
    /// 计算对齐后的块大小
    //*************************************************************************
    static size_t calculate_aligned_block_size(size_t raw_block_size, size_t alignment)
    {
        // 块大小必须：
        // 1. 至少能容纳一个指针（用于 ipool 的空闲链表）
        // 2. 至少等于 raw_block_size
        // 3. 按 alignment 对齐
        size_t min_size = (raw_block_size > sizeof(void*)) ? raw_block_size : sizeof(void*);
        return (min_size + alignment - 1) & ~(alignment - 1);
    }

    //*************************************************************************
    /// 分配对齐的缓冲区
    //*************************************************************************
    void allocate_aligned_buffer(size_t aligned_block_size, size_t alignment, size_t num_blocks)
    {
        // 计算总大小，额外分配 alignment 字节用于对齐调整
        size_t total_size = aligned_block_size * num_blocks + alignment;
        
        raw_buffer_ = ::malloc(total_size);
        ETL_ASSERT(raw_buffer_ != ETL_NULLPTR, ETL_ERROR(0));

        // 对齐地址（在 raw_buffer_ 内部找到对齐的位置）
        uintptr_t addr = reinterpret_cast<uintptr_t>(raw_buffer_);
        uintptr_t aligned_addr = (addr + alignment - 1) & ~(alignment - 1);
        aligned_buffer_ = reinterpret_cast<char*>(aligned_addr);
    }

    void* raw_buffer_;                       // 原始分配的缓冲区（用于最后释放）
    char* aligned_buffer_;                   // 对齐后的缓冲区（指向 raw_buffer_ 内部的对齐位置）
    etl::unique_ptr<public_ipool> pool_;     // 使用 ETL 的 unique_ptr 管理 public_ipool
    
    size_t raw_block_size_;                  // 用户请求的块大小
    size_t alignment_;                       // 对齐要求
    size_t aligned_block_size_;              // 对齐后的块大小
    size_t num_blocks_;                      // 块数量
    size_t peak_allocated_count_;            // 峰值分配的块数量
};

} // namespace uros

#endif