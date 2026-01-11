#include <iostream>
#include <cassert>
#include <cstring>
#include <chrono>
#include <vector>
#include <iomanip>
#include "uros2/block_allocator.h"

using namespace std;

// 测试结构体
struct TestData {
    int id;
    double value;
    char name[32];
};

void test_basic_allocation() {
    cout << "\n=== Test 1: Basic Allocation and Release ===\n";
    
    // 创建分配器：块大小64字节，8字节对齐，容量10个块
    uros::block_allocator allocator(64, 8, 10);
    
    cout << "Block size: " << allocator.get_block_size() << " bytes\n";
    cout << "Aligned block size: " << allocator.get_aligned_block_size() << " bytes\n";
    cout << "Alignment: " << allocator.get_alignment() << " bytes\n";
    cout << "Capacity: " << allocator.get_capacity() << " blocks\n";
    cout << "Available: " << allocator.get_available() << " blocks\n";
    
    // 分配一个块
    void* block1 = allocator.allocate(64, 8);
    assert(block1 != nullptr);
    cout << "Allocated block1: " << block1 << "\n";
    cout << "Available after allocation: " << allocator.get_available() << " blocks\n";
    
    // 写入数据
    memset(block1, 0xAB, 64);
    
    // 检查所有权
    assert(allocator.is_owner_of(block1));
    cout << "Ownership check: PASS\n";
    
    // 释放块
    bool released = allocator.release(block1);
    assert(released);
    cout << "Released block1\n";
    cout << "Available after release: " << allocator.get_available() << " blocks\n";
    
    cout << "Test 1: PASSED\n";
}

void test_multiple_allocations() {
    cout << "\n=== Test 2: Multiple Allocations ===\n";
    
    uros::block_allocator allocator(sizeof(TestData), alignof(TestData), 5);
    
    TestData* blocks[5];
    
    // 分配所有块
    for (int i = 0; i < 5; i++) {
        blocks[i] = static_cast<TestData*>(allocator.allocate(sizeof(TestData), alignof(TestData)));
        assert(blocks[i] != nullptr);
        
        blocks[i]->id = i;
        blocks[i]->value = i * 3.14;
        snprintf(blocks[i]->name, sizeof(blocks[i]->name), "Block_%d", i);
        
        cout << "Allocated block " << i << ": id=" << blocks[i]->id 
             << ", value=" << blocks[i]->value 
             << ", name=" << blocks[i]->name << "\n";
    }
    
    cout << "Allocated count: " << allocator.get_allocated_count() << "\n";
    cout << "Available: " << allocator.get_available() << "\n";
    
    // 尝试再分配一个（应该失败，因为已满）
    void* extra = allocator.allocate(sizeof(TestData), alignof(TestData));
    assert(extra == nullptr);
    cout << "Extra allocation correctly failed (pool full)\n";
    
    // 释放所有块
    for (int i = 0; i < 5; i++) {
        bool released = allocator.release(blocks[i]);
        assert(released);
    }
    
    cout << "Available after releasing all: " << allocator.get_available() << "\n";
    cout << "Test 2: PASSED\n";
}

void test_alignment() {
    cout << "\n=== Test 3: Alignment Test ===\n";
    
    // 测试不同对齐要求
    size_t alignments[] = {4, 8, 16, 32, 64};
    
    for (size_t align : alignments) {
        uros::block_allocator allocator(100, align, 3);
        
        for (int i = 0; i < 3; i++) {
            void* block = allocator.allocate(100, align);
            assert(block != nullptr);
            
            uintptr_t addr = reinterpret_cast<uintptr_t>(block);
            assert((addr % align) == 0);  // 检查对齐
            
            cout << "Block " << i << " with " << align << "-byte alignment: " 
                 << block << " (aligned: " << ((addr % align) == 0 ? "YES" : "NO") << ")\n";
                 
            allocator.release(block);
        }
    }
    
    cout << "Test 3: PASSED\n";
}

void test_reuse() {
    cout << "\n=== Test 4: Block Reuse Test ===\n";
    
    uros::block_allocator allocator(32, 8, 3);
    
    // 第一轮分配
    void* block1 = allocator.allocate(32, 8);
    void* block2 = allocator.allocate(32, 8);
    void* block3 = allocator.allocate(32, 8);
    
    cout << "First allocation round:\n";
    cout << "  block1: " << block1 << "\n";
    cout << "  block2: " << block2 << "\n";
    cout << "  block3: " << block3 << "\n";
    
    // 释放中间的块
    allocator.release(block2);
    cout << "Released block2\n";
    
    // 再次分配，应该重用 block2 的位置
    void* block4 = allocator.allocate(32, 8);
    cout << "  block4: " << block4 << " (should reuse block2's address)\n";
    assert(block4 == block2);
    
    // 清理
    allocator.release(block1);
    allocator.release(block3);
    allocator.release(block4);
    
    cout << "Test 4: PASSED\n";
}

void test_size_mismatch() {
    cout << "\n=== Test 5: Size/Alignment Mismatch Test ===\n";
    
    uros::block_allocator allocator(64, 8, 5);
    
    // 请求超过块大小的内存（应该失败）
    void* too_big = allocator.allocate(128, 8);
    assert(too_big == nullptr);
    cout << "Correctly rejected allocation of 128 bytes (block size is 64)\n";
    
    // 请求更大的对齐（应该失败）
    void* wrong_align = allocator.allocate(64, 16);
    assert(wrong_align == nullptr);
    cout << "Correctly rejected allocation with 16-byte alignment (allocator uses 8)\n";
    
    // 请求更小的对齐和大小（应该成功）
    void* ok_block = allocator.allocate(32, 4);
    assert(ok_block != nullptr);
    cout << "Successfully allocated 32 bytes with 4-byte alignment\n";
    
    allocator.release(ok_block);
    
    cout << "Test 5: PASSED\n";
}

void test_ownership() {
    cout << "\n=== Test 6: Ownership Test ===\n";
    
    uros::block_allocator allocator1(64, 8, 3);
    uros::block_allocator allocator2(64, 8, 3);
    
    void* block1 = allocator1.allocate(64, 8);
    void* block2 = allocator2.allocate(64, 8);
    
    // allocator1 应该拥有 block1，不应该拥有 block2
    assert(allocator1.is_owner_of(block1));
    assert(!allocator1.is_owner_of(block2));
    
    // allocator2 应该拥有 block2，不应该拥有 block1
    assert(allocator2.is_owner_of(block2));
    assert(!allocator2.is_owner_of(block1));
    
    cout << "Ownership correctly distinguished between allocators\n";
    
    // 尝试用错误的分配器释放（应该失败）
    bool wrong_release = allocator1.release(block2);
    assert(!wrong_release);
    cout << "Correctly rejected release of non-owned block\n";
    
    // 用正确的分配器释放
    allocator1.release(block1);
    allocator2.release(block2);
    
    cout << "Test 6: PASSED\n";
}

// ============== 级联测试 ==============

void test_basic_cascade() {
    cout << "\n=== Test 7: Basic Cascading (Using ETL Successor Pattern) ===\n";
    
    // 创建三个分配器
    uros::block_allocator alloc1(64, 8, 3);
    uros::block_allocator alloc2(64, 8, 3);
    uros::block_allocator alloc3(64, 8, 3);
    
    // 建立级联链：alloc1 -> alloc2 -> alloc3
    alloc1.set_successor(alloc2);
    alloc2.set_successor(alloc3);
    
    cout << "Created cascade chain: alloc1(3) -> alloc2(3) -> alloc3(3)\n";
    cout << "Total capacity: 9 blocks\n";
    
    // 通过 alloc1 分配9个块（会自动级联到 alloc2 和 alloc3）
    void* blocks[9];
    for (int i = 0; i < 9; ++i) {
        blocks[i] = alloc1.allocate(64, 8);
        assert(blocks[i] != nullptr);
    }
    
    cout << "Allocated 9 blocks through cascade\n";
    cout << "  alloc1: " << alloc1.get_allocated_count() << "/" << alloc1.get_capacity() << " used\n";
    cout << "  alloc2: " << alloc2.get_allocated_count() << "/" << alloc2.get_capacity() << " used\n";
    cout << "  alloc3: " << alloc3.get_allocated_count() << "/" << alloc3.get_capacity() << " used\n";
    
    // 尝试分配第10个块（应该失败）
    void* extra = alloc1.allocate(64, 8);
    assert(extra == nullptr);
    cout << "Extra allocation correctly failed (all allocators full)\n";
    
    // 通过 alloc1 释放所有块（会自动路由到正确的分配器）
    for (int i = 0; i < 9; ++i) {
        bool released = alloc1.release(blocks[i]);
        assert(released);
    }
    
    cout << "Released all blocks, all allocators empty\n";
    cout << "Test 7: PASSED\n";
}

void test_uneven_cascade() {
    cout << "\n=== Test 8: Uneven Capacity Cascading ===\n";
    
    uros::block_allocator alloc_small(32, 4, 2);
    uros::block_allocator alloc_medium(32, 4, 5);
    uros::block_allocator alloc_large(32, 4, 10);
    
    alloc_small.set_successor(alloc_medium);
    alloc_medium.set_successor(alloc_large);
    
    cout << "Created cascade: 2 -> 5 -> 10 (total 17 blocks)\n";
    
    // 分配所有块
    vector<void*> blocks;
    for (size_t i = 0; i < 17; ++i) {
        void* ptr = alloc_small.allocate(32, 4);
        assert(ptr != nullptr);
        blocks.push_back(ptr);
    }
    
    cout << "Allocated all 17 blocks:\n";
    cout << "  alloc_small:  " << alloc_small.get_allocated_count() << "/2\n";
    cout << "  alloc_medium: " << alloc_medium.get_allocated_count() << "/5\n";
    cout << "  alloc_large:  " << alloc_large.get_allocated_count() << "/10\n";
    
    // 清理
    for (auto* ptr : blocks) {
        alloc_small.release(ptr);
    }
    
    cout << "Test 8: PASSED\n";
}

void test_cascade_performance() {
    cout << "\n=== Test 9: Cascading Performance ===\n";
    
    // 创建5个分配器的级联链
    vector<uros::block_allocator*> allocators;
    for (int i = 0; i < 5; ++i) {
        allocators.push_back(new uros::block_allocator(64, 8, 20));
    }
    
    for (int i = 0; i < 4; ++i) {
        allocators[i]->set_successor(*allocators[i + 1]);
    }
    
    cout << "Created cascade with 5 allocators, 20 blocks each (100 total)\n";
    
    // 运行多轮分配/释放
    const int iterations = 100;
    cout << "Running " << iterations << " allocation/deallocation cycles...\n";
    
    for (int iter = 0; iter < iterations; ++iter) {
        vector<void*> temp_blocks;
        
        // 分配10-30个块
        int num_allocs = 10 + (iter % 21);
        for (int i = 0; i < num_allocs; ++i) {
            void* ptr = allocators[0]->allocate(64, 8);
            if (ptr) {
                temp_blocks.push_back(ptr);
            }
        }
        
        // 释放所有块
        for (auto* ptr : temp_blocks) {
            allocators[0]->release(ptr);
        }
    }
    
    cout << "Completed " << iterations << " cycles successfully\n";
    
    // 验证所有块都被释放
    size_t total_allocated = 0;
    for (auto* alloc : allocators) {
        total_allocated += alloc->get_allocated_count();
    }
    assert(total_allocated == 0);
    cout << "All blocks properly released\n";
    
    for (auto* alloc : allocators) {
        delete alloc;
    }
    
    cout << "Test 9: PASSED\n";
}

void test_different_block_sizes_cascade() {
    cout << "\n=== Test 10: Different Block Sizes Cascade (Most Common Use Case) ===\n";
    cout << "Small -> Medium -> Large block size cascade\n\n";
    
    // 创建不同块大小的分配器：小 -> 中 -> 大
    uros::block_allocator alloc_32(32, 8, 10);    // 32字节，10块
    uros::block_allocator alloc_128(128, 8, 5);   // 128字节，5块
    uros::block_allocator alloc_512(512, 8, 2);   // 512字节，2块
    
    // 建立级联链
    alloc_32.set_successor(alloc_128);
    alloc_128.set_successor(alloc_512);
    
    cout << "Created cascade chain:\n";
    cout << "  alloc_32:  32 bytes x 10 blocks\n";
    cout << "  alloc_128: 128 bytes x 5 blocks\n";
    cout << "  alloc_512: 512 bytes x 2 blocks\n\n";
    
    // 测试1：小对象分配（应该从 alloc_32 分配）
    cout << "Test 10.1: Allocating small objects (16 bytes)...\n";
    vector<void*> small_blocks;
    for (int i = 0; i < 10; ++i) {
        void* ptr = alloc_32.allocate(16, 8);
        assert(ptr != nullptr);
        small_blocks.push_back(ptr);
    }
    cout << "  alloc_32:  " << alloc_32.get_allocated_count() << "/10 used (expected: 10)\n";
    cout << "  alloc_128: " << alloc_128.get_allocated_count() << "/5 used (expected: 0)\n";
    cout << "  alloc_512: " << alloc_512.get_allocated_count() << "/2 used (expected: 0)\n";
    assert(alloc_32.get_allocated_count() == 10);
    assert(alloc_128.get_allocated_count() == 0);
    assert(alloc_512.get_allocated_count() == 0);
    
    // 测试2：继续分配小对象，应该级联到 alloc_128
    cout << "\nTest 10.2: alloc_32 full, cascading to alloc_128...\n";
    vector<void*> medium_blocks;
    for (int i = 0; i < 5; ++i) {
        void* ptr = alloc_32.allocate(20, 8);  // 20字节，超过32所以只能用128
        assert(ptr != nullptr);
        medium_blocks.push_back(ptr);
    }
    cout << "  alloc_32:  " << alloc_32.get_allocated_count() << "/10 used (expected: 10)\n";
    cout << "  alloc_128: " << alloc_128.get_allocated_count() << "/5 used (expected: 5)\n";
    cout << "  alloc_512: " << alloc_512.get_allocated_count() << "/2 used (expected: 0)\n";
    assert(alloc_32.get_allocated_count() == 10);
    assert(alloc_128.get_allocated_count() == 5);
    assert(alloc_512.get_allocated_count() == 0);
    
    // 测试3：继续分配，应该级联到 alloc_512
    cout << "\nTest 10.3: alloc_32 and alloc_128 full, cascading to alloc_512...\n";
    vector<void*> large_blocks;
    for (int i = 0; i < 2; ++i) {
        void* ptr = alloc_32.allocate(30, 8);
        assert(ptr != nullptr);
        large_blocks.push_back(ptr);
    }
    cout << "  alloc_32:  " << alloc_32.get_allocated_count() << "/10 used (expected: 10)\n";
    cout << "  alloc_128: " << alloc_128.get_allocated_count() << "/5 used (expected: 5)\n";
    cout << "  alloc_512: " << alloc_512.get_allocated_count() << "/2 used (expected: 2)\n";
    assert(alloc_32.get_allocated_count() == 10);
    assert(alloc_128.get_allocated_count() == 5);
    assert(alloc_512.get_allocated_count() == 2);
    
    // 测试4：所有分配器都满了
    cout << "\nTest 10.4: All allocators full, allocation should fail...\n";
    void* extra = alloc_32.allocate(16, 8);
    assert(extra == nullptr);
    cout << "  Correctly failed to allocate when all allocators are full\n";
    
    // 测试5：释放一些块，验证重用
    cout << "\nTest 10.5: Release and reuse blocks...\n";
    // 释放一些小块
    alloc_32.release(small_blocks[0]);
    alloc_32.release(small_blocks[5]);
    
    // 应该能重新分配小块
    void* reused1 = alloc_32.allocate(20, 8);
    void* reused2 = alloc_32.allocate(25, 8);
    assert(reused1 != nullptr);
    assert(reused2 != nullptr);
    cout << "  Successfully reused freed blocks\n";
    cout << "  alloc_32:  " << alloc_32.get_allocated_count() << "/10 used\n";
    
    // 清理所有块
    cout << "\nTest 10.6: Cleanup all allocations...\n";
    for (auto* ptr : small_blocks) {
        if (ptr != small_blocks[0] && ptr != small_blocks[5]) {
            alloc_32.release(ptr);
        }
    }
    for (auto* ptr : medium_blocks) {
        alloc_32.release(ptr);
    }
    for (auto* ptr : large_blocks) {
        alloc_32.release(ptr);
    }
    alloc_32.release(reused1);
    alloc_32.release(reused2);
    
    cout << "  alloc_32:  " << alloc_32.get_allocated_count() << "/10 (should be 0)\n";
    cout << "  alloc_128: " << alloc_128.get_allocated_count() << "/5 (should be 0)\n";
    cout << "  alloc_512: " << alloc_512.get_allocated_count() << "/2 (should be 0)\n";
    assert(alloc_32.get_allocated_count() == 0);
    assert(alloc_128.get_allocated_count() == 0);
    assert(alloc_512.get_allocated_count() == 0);
    
    cout << "\nTest 10: PASSED\n";
}

void test_mixed_size_allocations() {
    cout << "\n=== Test 11: Mixed Size Allocations (Realistic Scenario) ===\n";
    
    // 模拟实际使用场景：大部分是小对象，少量中等对象，偶尔有大对象
    uros::block_allocator alloc_small(64, 8, 20);     // 小块为主
    uros::block_allocator alloc_medium(256, 8, 10);   // 中块
    uros::block_allocator alloc_large(1024, 8, 5);    // 大块
    
    alloc_small.set_successor(alloc_medium);
    alloc_medium.set_successor(alloc_large);
    
    cout << "Created realistic cascade:\n";
    cout << "  Small:  64 bytes x 20 blocks\n";
    cout << "  Medium: 256 bytes x 10 blocks\n";
    cout << "  Large:  1024 bytes x 5 blocks\n\n";
    
    vector<void*> all_blocks;
    
    // 模拟混合分配模式
    cout << "Simulating mixed allocation pattern...\n";
    
    // 分配15个小对象
    for (int i = 0; i < 15; ++i) {
        void* ptr = alloc_small.allocate(32, 8);
        assert(ptr != nullptr);
        all_blocks.push_back(ptr);
    }
    cout << "  Allocated 15 small objects\n";
    
    // 分配5个中等对象
    for (int i = 0; i < 5; ++i) {
        void* ptr = alloc_small.allocate(128, 8);
        assert(ptr != nullptr);
        all_blocks.push_back(ptr);
    }
    cout << "  Allocated 5 medium objects\n";
    
    // 分配2个大对象
    for (int i = 0; i < 2; ++i) {
        void* ptr = alloc_small.allocate(512, 8);
        assert(ptr != nullptr);
        all_blocks.push_back(ptr);
    }
    cout << "  Allocated 2 large objects\n";
    
    // 再分配更多小对象（会用完 small，然后用 medium）
    for (int i = 0; i < 10; ++i) {
        void* ptr = alloc_small.allocate(48, 8);
        assert(ptr != nullptr);
        all_blocks.push_back(ptr);
    }
    cout << "  Allocated 10 more small objects\n\n";
    
    cout << "Final allocation state:\n";
    cout << "  Small:  " << alloc_small.get_allocated_count() << "/20\n";
    cout << "  Medium: " << alloc_medium.get_allocated_count() << "/10\n";
    cout << "  Large:  " << alloc_large.get_allocated_count() << "/5\n";
    
    // 验证分配效率
    size_t total_allocated = alloc_small.get_allocated_count() + 
                            alloc_medium.get_allocated_count() + 
                            alloc_large.get_allocated_count();
    cout << "  Total allocated: " << total_allocated << " blocks\n";
    assert(total_allocated == all_blocks.size());
    
    // 清理
    for (auto* ptr : all_blocks) {
        alloc_small.release(ptr);
    }
    
    assert(alloc_small.get_allocated_count() == 0);
    assert(alloc_medium.get_allocated_count() == 0);
    assert(alloc_large.get_allocated_count() == 0);
    
    cout << "\nTest 11: PASSED\n";
}

void test_performance_o1() {
    cout << "\n=== Test 12: Performance - O(1) Time Complexity Verification ===\n";
    
    // 测试不同大小的分配器，验证性能是否为 O(1)
    vector<size_t> pool_sizes = {100, 1000, 10000};
    
    for (size_t pool_size : pool_sizes) {
        cout << "\n--- Testing with pool size: " << pool_size << " blocks ---\n";
        
        uros::block_allocator allocator(64, 8, pool_size);
        
        // 测试1：连续分配性能（应该是 O(1)，时间不随池大小增长）
        auto start = chrono::high_resolution_clock::now();
        
        vector<void*> blocks;
        blocks.reserve(pool_size);
        
        for (size_t i = 0; i < pool_size; ++i) {
            void* ptr = allocator.allocate(64, 8);
            assert(ptr != nullptr);
            blocks.push_back(ptr);
        }
        
        auto end = chrono::high_resolution_clock::now();
        auto alloc_duration = chrono::duration_cast<chrono::microseconds>(end - start);
        double avg_alloc_time = static_cast<double>(alloc_duration.count()) / pool_size;
        
        cout << "  Allocation: " << alloc_duration.count() << " µs total, "
             << avg_alloc_time << " µs/block average\n";
        
        // 测试2：随机释放和重新分配（验证空闲链表操作是 O(1)）
        start = chrono::high_resolution_clock::now();
        
        // 释放前半部分
        for (size_t i = 0; i < pool_size / 2; ++i) {
            bool released = allocator.release(blocks[i]);
            assert(released);
        }
        
        // 重新分配
        for (size_t i = 0; i < pool_size / 2; ++i) {
            void* ptr = allocator.allocate(64, 8);
            assert(ptr != nullptr);
            blocks[i] = ptr;
        }
        
        end = chrono::high_resolution_clock::now();
        auto reuse_duration = chrono::duration_cast<chrono::microseconds>(end - start);
        double avg_reuse_time = static_cast<double>(reuse_duration.count()) / pool_size;
        
        cout << "  Reuse: " << reuse_duration.count() << " µs total, "
             << avg_reuse_time << " µs/operation average\n";
        
        // 测试3：全部释放性能
        start = chrono::high_resolution_clock::now();
        
        for (auto* ptr : blocks) {
            bool released = allocator.release(ptr);
            assert(released);
        }
        
        end = chrono::high_resolution_clock::now();
        auto release_duration = chrono::duration_cast<chrono::microseconds>(end - start);
        double avg_release_time = static_cast<double>(release_duration.count()) / pool_size;
        
        cout << "  Release: " << release_duration.count() << " µs total, "
             << avg_release_time << " µs/block average\n";
        
        assert(allocator.get_allocated_count() == 0);
    }
    
    cout << "\n分析: 如果是 O(1) 复杂度，平均时间应该保持相对稳定，\n";
    cout << "      不会随着池大小从 100 -> 1000 -> 10000 显著增长\n";
    
    // 级联性能测试
    cout << "\n--- Testing cascade performance ---\n";
    
    uros::block_allocator alloc1(64, 8, 1000);
    uros::block_allocator alloc2(64, 8, 1000);
    uros::block_allocator alloc3(64, 8, 1000);
    
    alloc1.set_successor(alloc2);
    alloc2.set_successor(alloc3);
    
    auto start = chrono::high_resolution_clock::now();
    
    vector<void*> cascade_blocks;
    cascade_blocks.reserve(3000);
    
    // 通过级联分配 3000 个块
    for (int i = 0; i < 3000; ++i) {
        void* ptr = alloc1.allocate(64, 8);
        assert(ptr != nullptr);
        cascade_blocks.push_back(ptr);
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto cascade_duration = chrono::duration_cast<chrono::microseconds>(end - start);
    
    cout << "  Cascade allocation (3000 blocks): " << cascade_duration.count() << " µs\n";
    cout << "  Average: " << (static_cast<double>(cascade_duration.count()) / 3000.0) << " µs/block\n";
    cout << "  alloc1: " << alloc1.get_allocated_count() << "/1000\n";
    cout << "  alloc2: " << alloc2.get_allocated_count() << "/1000\n";
    cout << "  alloc3: " << alloc3.get_allocated_count() << "/1000\n";
    
    // 清理
    for (auto* ptr : cascade_blocks) {
        alloc1.release(ptr);
    }
    
    cout << "\nTest 12: PASSED\n";
}

void test_cascade_levels_performance() {
    cout << "\n=== Test 13: Multi-Level Cascade Performance Comparison ===\n";
    cout << "Comparing single-level vs 5-level cascade allocation\n\n";
    
    const int iterations = 10000;
    vector<size_t> block_sizes = {64, 128, 256, 512, 1024};
    
    // ===== 单级分配测试 =====
    cout << "--- Single-Level Allocators (baseline) ---\n";
    
    vector<double> single_level_times;
    
    // 在循环外创建数组，和级联测试保持一致
    void* blocks[iterations];
    
    for (size_t block_size : block_sizes) {
        // 先创建 allocator（不计入性能测量）
        uros::block_allocator allocator(block_size, 8, iterations);
        
        // 第一轮：完全初始化ipool（分配所有块，然后全部释放）
        cout << "  " << block_size << " bytes: initializing ipool...";
        for (int i = 0; i < iterations; ++i) {
            blocks[i] = allocator.allocate(block_size, 8);
        }
        for (int i = 0; i < iterations; ++i) {
            allocator.release(blocks[i]);
        }
        cout << " done!\n";
        
        // 第二轮：现在测量真正的性能（ipool已完全初始化）
        auto start = chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            blocks[i] = allocator.allocate(block_size, 8);
        }
        
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        double avg_time = static_cast<double>(duration.count()) / iterations;
        single_level_times.push_back(avg_time);
        
        cout << "    Performance: " << avg_time << " ns/block (" 
             << duration.count() / 1000 << " µs total)\n";
        
        // 清理内存，避免累积影响下一次测试
        for (int i = 0; i < iterations; ++i) {
            if (blocks[i]) allocator.release(blocks[i]);
        }
    }
    
    // ===== 5级级联测试 =====
    cout << "\n--- 5-Level Cascade (64->128->256->512->1024) ---\n";
    
    uros::block_allocator alloc_64(64, 8, iterations);
    uros::block_allocator alloc_128(128, 8, iterations);
    uros::block_allocator alloc_256(256, 8, iterations);
    uros::block_allocator alloc_512(512, 8, iterations);
    uros::block_allocator alloc_1024(1024, 8, iterations);
    
    alloc_64.set_successor(alloc_128);
    alloc_128.set_successor(alloc_256);
    alloc_256.set_successor(alloc_512);
    alloc_512.set_successor(alloc_1024);
    
    // 第一轮：完全初始化所有级联allocator的ipool
    cout << "Initializing all cascade allocators' ipools...\n";
    void* cascade_blocks[iterations];
    
    for (size_t block_size : block_sizes) {
        cout << "  Initializing for " << block_size << " byte requests...";
        // 分配所有块
        for (int i = 0; i < iterations; ++i) {
            cascade_blocks[i] = alloc_64.allocate(block_size, 8);
        }
        // 全部释放
        for (int i = 0; i < iterations; ++i) {
            alloc_64.release(cascade_blocks[i]);
        }
        cout << " done!\n";
    }
    
    cout << "\nNow measuring performance with fully initialized pools:\n\n";
    
    vector<double> cascade_times;
    
    // 第二轮：测量真正的性能（所有ipool已完全初始化）
    for (size_t request_size : block_sizes) {
        auto start = chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            cascade_blocks[i] = alloc_64.allocate(request_size, 8);
        }
        
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        double avg_time = static_cast<double>(duration.count()) / iterations;
        cascade_times.push_back(avg_time);
        
        // 显示分配情况
        cout << "  Requesting " << request_size << " bytes:\n";
        cout << "    Time: " << avg_time << " ns/block (" 
             << duration.count() / 1000 << " µs total)\n";
        cout << "    alloc_64:   " << alloc_64.get_allocated_count() << "/" << iterations << "\n";
        cout << "    alloc_128:  " << alloc_128.get_allocated_count() << "/" << iterations << "\n";
        cout << "    alloc_256:  " << alloc_256.get_allocated_count() << "/" << iterations << "\n";
        cout << "    alloc_512:  " << alloc_512.get_allocated_count() << "/" << iterations << "\n";
        cout << "    alloc_1024: " << alloc_1024.get_allocated_count() << "/" << iterations << "\n";
        cout << "\n";
    }
    
    // ===== 性能对比分析 =====
    cout << "--- Performance Comparison ---\n";
    cout << "Block Size | Single-Level | 5-Level Cascade | Overhead | Cascade Level\n";
    cout << "-----------|--------------|-----------------|----------|---------------\n";
    
    for (size_t i = 0; i < block_sizes.size(); ++i) {
        double overhead = ((cascade_times[i] - single_level_times[i]) / single_level_times[i]) * 100.0;
        int cascade_level = i + 1;  // 64在第1级，1024在第5级
        
        cout << std::setw(10) << block_sizes[i] << " | "
             << std::setw(12) << std::fixed << std::setprecision(2) << single_level_times[i] << " | "
             << std::setw(15) << cascade_times[i] << " | "
             << std::setw(7) << std::setprecision(1) << overhead << "% | "
             << "Level " << cascade_level << "\n";
    }
    
    cout << "\n分析:\n";
    cout << "  - 64字节块在第1级，无需级联，开销最小\n";
    cout << "  - 1024字节块在第5级，需要经过4次级联检查\n";
    cout << "  - 即使是最坏情况（5级级联），开销也应该很小\n";
    cout << "  - 每增加一级级联，主要开销是：检查块大小 + 跳转到successor\n";
    
    // 测试最坏情况：所有小分配器都满，只能用最后的1024分配器
    cout << "\n--- Worst Case: All smaller allocators full ---\n";
    
    // 先用完前4个分配器（直接分配，不存储）
    for (int i = 0; i < iterations; ++i) {
        alloc_64.allocate(64, 8);
        alloc_128.allocate(128, 8);
        alloc_256.allocate(256, 8);
        alloc_512.allocate(512, 8);
    }
    
    cout << "  Filled all smaller allocators\n";
    cout << "  alloc_64:  " << alloc_64.get_allocated_count() << "/" << iterations << " (full)\n";
    cout << "  alloc_128: " << alloc_128.get_allocated_count() << "/" << iterations << " (full)\n";
    cout << "  alloc_256: " << alloc_256.get_allocated_count() << "/" << iterations << " (full)\n";
    cout << "  alloc_512: " << alloc_512.get_allocated_count() << "/" << iterations << " (full)\n";
    
    // 现在测试64字节请求在前4级都满的情况下的性能（会级联到1024级）
    const int worst_case_count = 1000;
    void* worst_blocks[worst_case_count];
    
    auto start = chrono::high_resolution_clock::now();
    
    for (int i = 0; i < worst_case_count; ++i) {
        worst_blocks[i] = alloc_64.allocate(64, 8);
    }
    
    auto end = chrono::high_resolution_clock::now();
    auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
    double worst_avg = static_cast<double>(duration.count()) / worst_case_count;
    
    cout << "  Requested 1000 x 64-byte blocks (cascaded to level 5)\n";
    cout << "  Average time: " << worst_avg << " ns/block\n";
    cout << "  vs baseline (64 bytes single-level): " << single_level_times[0] << " ns/block\n";
    cout << "  Overhead: " << ((worst_avg - single_level_times[0]) / single_level_times[0] * 100.0) 
         << "%\n";
    
    // 无需清理
    
    cout << "\nTest 13: PASSED\n";
}

void test_pure_allocation_performance() {
    cout << "\n=== Test 14: Pure Allocation Performance (No Vector Overhead) ===\n";
    cout << "Measuring ONLY the allocate() call, excluding vector operations\n\n";
    
    const int iterations = 10000;
    vector<size_t> block_sizes = {64, 128, 256, 512, 1024};
    
    cout << "--- Pure Allocation Time (single allocator per size) ---\n";
    
    for (size_t block_size : block_sizes) {
        uros::block_allocator allocator(block_size, 8, iterations);
        
        // 预分配存储空间，避免vector操作影响测试
        void* blocks[iterations];
        
        // 只测量分配操作
        auto start = chrono::high_resolution_clock::now();
        
        for (int i = 0; i < iterations; ++i) {
            blocks[i] = allocator.allocate(block_size, 8);
        }
        
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        double avg_time = static_cast<double>(duration.count()) / iterations;
        
        cout << "  " << std::setw(4) << block_size << " bytes: " 
             << std::setw(8) << std::fixed << std::setprecision(2) << avg_time << " ns/block"
             << " (total: " << duration.count() / 1000 << " µs)\n";
        
        // 验证所有分配成功
        int success_count = 0;
        for (int i = 0; i < iterations; ++i) {
            if (blocks[i] != nullptr) success_count++;
        }
        assert(success_count == iterations);
        
        // 清理
        for (int i = 0; i < iterations; ++i) {
            if (blocks[i]) allocator.release(blocks[i]);
        }
    }
    
    cout << "\n--- Analysis: Why do larger blocks take longer? ---\n";
    cout << "Testing individual operations to find the bottleneck:\n\n";
    
    // 测试1：只调用allocate()，不存储结果
    cout << "Test 1: Call allocate() without storing (measures pure allocation):\n";
    for (size_t block_size : block_sizes) {
        uros::block_allocator allocator(block_size, 8, 1000);
        
        auto start = chrono::high_resolution_clock::now();
        for (int i = 0; i < 1000; ++i) {
            volatile void* ptr = allocator.allocate(block_size, 8);
            (void)ptr;  // 防止优化掉
        }
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        
        cout << "  " << std::setw(4) << block_size << " bytes: " 
             << std::fixed << std::setprecision(2) 
             << (static_cast<double>(duration.count()) / 1000.0) << " ns/call\n";
    }
    
    // 测试2：测量ipool内部操作
    cout << "\nTest 2: Memory touching test (check if allocator zeros memory):\n";
    for (size_t block_size : block_sizes) {
        uros::block_allocator allocator(block_size, 8, 100);
        
        // 分配并写入数据
        void* blocks[100];
        for (int i = 0; i < 100; ++i) {
            blocks[i] = allocator.allocate(block_size, 8);
            memset(blocks[i], 0xFF, block_size);  // 写入数据
        }
        
        // 释放所有
        for (int i = 0; i < 100; ++i) {
            allocator.release(blocks[i]);
        }
        
        // 测量重新分配的时间（如果分配器清零内存，大块会更慢）
        auto start = chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            blocks[i] = allocator.allocate(block_size, 8);
        }
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        
        // 检查内存是否被清零
        bool is_zeroed = true;
        unsigned char* ptr = static_cast<unsigned char*>(blocks[0]);
        for (size_t i = 0; i < block_size && i < 16; ++i) {
            if (ptr[i] == 0x00) {
                is_zeroed = true;
                break;
            }
            if (ptr[i] == 0xFF) {
                is_zeroed = false;
                break;
            }
        }
        
        cout << "  " << std::setw(4) << block_size << " bytes: " 
             << std::fixed << std::setprecision(2) 
             << (static_cast<double>(duration.count()) / 100.0) << " ns/call"
             << " (memory " << (is_zeroed ? "ZEROED" : "NOT zeroed") << ")\n";
        
        // 清理
        for (int i = 0; i < 100; ++i) {
            allocator.release(blocks[i]);
        }
    }
    
    // 测试3：内存访问时间
    cout << "\nTest 3: Memory access time (cache effects):\n";
    for (size_t block_size : block_sizes) {
        uros::block_allocator allocator(block_size, 8, 100);
        
        void* blocks[100];
        for (int i = 0; i < 100; ++i) {
            blocks[i] = allocator.allocate(block_size, 8);
        }
        
        // 测量首次写入时间
        auto start = chrono::high_resolution_clock::now();
        for (int i = 0; i < 100; ++i) {
            // 只写第一个字节，触发页面/缓存加载
            *static_cast<char*>(blocks[i]) = 0;
        }
        auto end = chrono::high_resolution_clock::now();
        auto duration = chrono::duration_cast<chrono::nanoseconds>(end - start);
        
        cout << "  " << std::setw(4) << block_size << " bytes: " 
             << std::fixed << std::setprecision(2) 
             << (static_cast<double>(duration.count()) / 100.0) << " ns/block\n";
        
        // 清理
        for (int i = 0; i < 100; ++i) {
            allocator.release(blocks[i]);
        }
    }
    
    cout << "\n结论分析:\n";
    cout << "  如果 Test 1 显示所有大小的时间相近 -> 分配器本身是 O(1)，差异来自其他因素\n";
    cout << "  如果 Test 2 显示内存被清零 -> 大块耗时更多是因为 memset 开销\n";
    cout << "  如果 Test 3 显示差异 -> 可能是缓存/页面加载的影响\n";
    cout << "  如果都相近 -> 之前的差异主要来自 vector::push_back() 和内存管理开销\n";
    
    cout << "\nTest 14: PASSED\n";
}

int main() {
    cout << "========================================\n";
    cout << "Memory Block Allocator Test Suite\n";
    cout << "========================================\n";
    
    try {
        // 基本功能测试
        test_basic_allocation();
        test_multiple_allocations();
        test_alignment();
        test_reuse();
        test_size_mismatch();
        test_ownership();
        
        // 级联功能测试
        test_basic_cascade();
        test_uneven_cascade();
        test_cascade_performance();
        test_different_block_sizes_cascade();  // 最常用的级联场景
        test_mixed_size_allocations();         // 真实使用场景
        test_performance_o1();                 // O(1) 性能验证
        test_cascade_levels_performance();     // 多级级联性能对比
        test_pure_allocation_performance();    // 纯分配性能分析
        
        cout << "\n========================================\n";
        cout << "All tests PASSED!\n";
        cout << "========================================\n";
        return 0;
    } catch (const exception& e) {
        cerr << "\nTest FAILED with exception: " << e.what() << "\n";
        return 1;
    } catch (...) {
        cerr << "\nTest FAILED with unknown exception\n";
        return 1;
    }
}
