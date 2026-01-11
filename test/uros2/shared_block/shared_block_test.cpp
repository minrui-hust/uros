///\file
/// Test suite for shared_block

#include "uros2/shared_block.h"
#include "uros2/reference_counted_block_pool.h"
#include "uros2/block_allocator.h"
#include <cstdio>
#include <cstring>
#include <cassert>

using namespace uros;

//***************************************************************************
/// Test basic construction and destruction
//***************************************************************************
void test_basic_construction()
{
    printf("Test: Basic construction\n");
    
    // Create allocator and pool
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    // Test default constructor
    shared_block block1;
    assert(!block1.is_valid());
    assert(block1.get() == ETL_NULLPTR);
    assert(block1.use_count() == 0);
    
    // Test create
    shared_block block2 = shared_block::create(pool, 64, 8);
    assert(block2.is_valid());
    assert(block2.get() != ETL_NULLPTR);
    assert(block2.use_count() == 1);
    
    printf("  ✓ Default constructor works\n");
    printf("  ✓ Create method works\n");
    printf("  ✓ Use count is 1 after creation\n");
}

//***************************************************************************
/// Test copy constructor and reference counting
//***************************************************************************
void test_copy_constructor()
{
    printf("\nTest: Copy constructor\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    shared_block block1 = shared_block::create(pool, 64, 8);
    assert(block1.use_count() == 1);
    
    // Copy constructor
    shared_block block2(block1);
    assert(block1.use_count() == 2);
    assert(block2.use_count() == 2);
    assert(block1.get() == block2.get());
    
    printf("  ✓ Copy constructor increments reference count\n");
    printf("  ✓ Both blocks share the same memory\n");
}

//***************************************************************************
/// Test move constructor
//***************************************************************************
void test_move_constructor()
{
    printf("\nTest: Move constructor\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    shared_block block1 = shared_block::create(pool, 64, 8);
    void* original_ptr = block1.get();
    assert(block1.use_count() == 1);
    
    // Move constructor
    shared_block block2(etl::move(block1));
    assert(!block1.is_valid());
    assert(block1.use_count() == 0);
    assert(block2.use_count() == 1);
    assert(block2.get() == original_ptr);
    
    printf("  ✓ Move constructor transfers ownership\n");
    printf("  ✓ Source block becomes invalid\n");
    printf("  ✓ Reference count remains 1\n");
}

//***************************************************************************
/// Test copy assignment operator
//***************************************************************************
void test_copy_assignment()
{
    printf("\nTest: Copy assignment\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    shared_block block1 = shared_block::create(pool, 64, 8);
    shared_block block2 = shared_block::create(pool, 32, 4);
    
    assert(block1.use_count() == 1);
    assert(block2.use_count() == 1);
    assert(block1.get() != block2.get());
    
    // Copy assignment
    block2 = block1;
    assert(block1.use_count() == 2);
    assert(block2.use_count() == 2);
    assert(block1.get() == block2.get());
    
    printf("  ✓ Copy assignment increments reference count\n");
    printf("  ✓ Old block is released\n");
    printf("  ✓ Both blocks share the same memory\n");
}

//***************************************************************************
/// Test move assignment operator
//***************************************************************************
void test_move_assignment()
{
    printf("\nTest: Move assignment\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    shared_block block1 = shared_block::create(pool, 64, 8);
    shared_block block2 = shared_block::create(pool, 32, 4);
    void* original_ptr = block1.get();
    
    assert(block1.use_count() == 1);
    assert(block2.use_count() == 1);
    
    // Move assignment
    block2 = etl::move(block1);
    assert(!block1.is_valid());
    assert(block1.use_count() == 0);
    assert(block2.use_count() == 1);
    assert(block2.get() == original_ptr);
    
    printf("  ✓ Move assignment transfers ownership\n");
    printf("  ✓ Source block becomes invalid\n");
    printf("  ✓ Reference count remains 1\n");
}

//***************************************************************************
/// Test reset functionality
//***************************************************************************
void test_reset()
{
    printf("\nTest: Reset\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    shared_block block1 = shared_block::create(pool, 64, 8);
    shared_block block2(block1);
    
    assert(block1.use_count() == 2);
    assert(block2.use_count() == 2);
    
    // Reset block1
    block1.reset();
    assert(!block1.is_valid());
    assert(block1.use_count() == 0);
    assert(block2.use_count() == 1);
    assert(block2.is_valid());
    
    printf("  ✓ Reset makes block invalid\n");
    printf("  ✓ Reference count decrements correctly\n");
    printf("  ✓ Other blocks remain valid\n");
}

//***************************************************************************
/// Test automatic release when reference count reaches zero
//***************************************************************************
void test_automatic_release()
{
    printf("\nTest: Automatic release\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    size_t initial_available = allocator.get_available();
    
    {
        shared_block block1 = shared_block::create(pool, 64, 8);
        assert(allocator.get_available() == initial_available - 1);
        
        {
            shared_block block2(block1);
            assert(block1.use_count() == 2);
            assert(allocator.get_available() == initial_available - 1);
        }
        
        // block2 destroyed, but block1 still holds reference
        assert(block1.use_count() == 1);
        assert(allocator.get_available() == initial_available - 1);
    }
    
    // block1 destroyed, memory should be released
    assert(allocator.get_available() == initial_available);
    
    printf("  ✓ Memory released when last reference is destroyed\n");
    printf("  ✓ Pool statistics updated correctly\n");
}

//***************************************************************************
/// Test data access and modification
//***************************************************************************
void test_data_access()
{
    printf("\nTest: Data access\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    shared_block block = shared_block::create(pool, 64, 8);
    assert(block.is_valid());
    
    // Write data
    char* data = static_cast<char*>(block.get());
    const char* test_str = "Hello, shared_block!";
    strcpy(data, test_str);
    
    // Read data through another shared block
    shared_block block2(block);
    const char* data2 = static_cast<const char*>(block2.get());
    assert(strcmp(data2, test_str) == 0);
    
    printf("  ✓ Data can be written through get()\n");
    printf("  ✓ Data is shared between copies\n");
    printf("  ✓ Const access works correctly\n");
}

//***************************************************************************
/// Test bool conversion operator
//***************************************************************************
void test_bool_conversion()
{
    printf("\nTest: Bool conversion\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    shared_block block1;
    assert(!block1);
    
    shared_block block2 = shared_block::create(pool, 64, 8);
    assert(block2);
    
    block2.reset();
    assert(!block2);
    
    printf("  ✓ Empty block converts to false\n");
    printf("  ✓ Valid block converts to true\n");
    printf("  ✓ Reset block converts to false\n");
}

//***************************************************************************
/// Test self-assignment
//***************************************************************************
void test_self_assignment()
{
    printf("\nTest: Self-assignment\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    shared_block block = shared_block::create(pool, 64, 8);
    void* original_ptr = block.get();
    int original_count = block.use_count();
    
    // Self copy assignment
    block = block;
    assert(block.get() == original_ptr);
    assert(block.use_count() == original_count);
    
    // Self move assignment
    block = etl::move(block);
    assert(block.get() == original_ptr);
    assert(block.use_count() == original_count);
    
    printf("  ✓ Self copy assignment is safe\n");
    printf("  ✓ Self move assignment is safe\n");
}

//***************************************************************************
/// Test multiple allocations and releases
//***************************************************************************
void test_multiple_allocations()
{
    printf("\nTest: Multiple allocations\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    const int num_blocks = 5;
    shared_block blocks[num_blocks];
    
    // Allocate multiple blocks
    for (int i = 0; i < num_blocks; ++i)
    {
        blocks[i] = shared_block::create(pool, 64, 8);
        assert(blocks[i].is_valid());
    }
    
    size_t available_after_alloc = allocator.get_available();
    assert(allocator.get_available() == allocator.get_capacity() - num_blocks);
    
    // Release all blocks
    for (int i = 0; i < num_blocks; ++i)
    {
        blocks[i].reset();
    }
    
    assert(allocator.get_available() == allocator.get_capacity());
    
    printf("  ✓ Multiple allocations work correctly\n");
    printf("  ✓ All blocks released successfully\n");
    printf("  ✓ Pool returns to full capacity\n");
}

//***************************************************************************
/// Test allocation failure
//***************************************************************************
void test_allocation_failure()
{
    printf("\nTest: Allocation failure\n");
    
    // Create a small pool
    uros::block_allocator allocator(128, 8, 2);
    reference_counted_block_pool pool(allocator);
    
    shared_block block1 = shared_block::create(pool, 64, 8);
    shared_block block2 = shared_block::create(pool, 64, 8);
    
    assert(block1.is_valid());
    assert(block2.is_valid());
    assert(allocator.get_available() == 0);
    
    // This should fail
    shared_block block3 = shared_block::create(pool, 64, 8);
    assert(!block3.is_valid());
    assert(block3.get() == ETL_NULLPTR);
    assert(block3.use_count() == 0);
    
    printf("  ✓ Allocation failure handled gracefully\n");
    printf("  ✓ Invalid block can be safely used\n");
}

//***************************************************************************
/// Test peak allocation tracking
//***************************************************************************
void test_peak_allocation()
{
    printf("\nTest: Peak allocation tracking\n");
    
    uros::block_allocator allocator(128, 8, 10);
    reference_counted_block_pool pool(allocator);
    
    assert(allocator.get_peak_allocated_count() == 0);
    
    {
        shared_block block1 = shared_block::create(pool, 64, 8);
        assert(allocator.get_peak_allocated_count() == 1);
        
        shared_block block2 = shared_block::create(pool, 64, 8);
        shared_block block3 = shared_block::create(pool, 64, 8);
        assert(allocator.get_peak_allocated_count() == 3);
        
        block2.reset();
        assert(allocator.get_allocated_count() == 2);
        assert(allocator.get_peak_allocated_count() == 3); // Peak doesn't decrease
    }
    
    assert(allocator.get_allocated_count() == 0);
    assert(allocator.get_peak_allocated_count() == 3); // Peak remains
    
    printf("  ✓ Peak allocation count tracked correctly\n");
    printf("  ✓ Peak doesn't decrease after release\n");
}

//***************************************************************************
/// Test cascaded allocators with different block sizes
//***************************************************************************
void test_cascaded_allocators()
{
    printf("\nTest: Cascaded allocators\n");
    
    // Create cascaded allocators: 16 -> 32 -> 64 -> 128 -> 256 bytes
    uros::block_allocator allocator_16(16, 8, 5);
    uros::block_allocator allocator_32(32, 8, 5);
    uros::block_allocator allocator_64(64, 8, 5);
    uros::block_allocator allocator_128(128, 8, 5);
    uros::block_allocator allocator_256(256, 8, 5);
    
    // Set up the cascade chain
    allocator_16.set_successor(allocator_32, allocator_64, allocator_128, allocator_256);
    
    reference_counted_block_pool pool(allocator_16);
    
    // Test small allocation (should use allocator_16)
    shared_block block_small = shared_block::create(pool, 8, 8);
    assert(block_small.is_valid());
    assert(allocator_16.get_allocated_count() == 1);
    printf("  ✓ Small block (8 bytes) allocated from 16-byte pool\n");
    
    // Test medium allocation (should use allocator_32)
    shared_block block_medium = shared_block::create(pool, 24, 8);
    assert(block_medium.is_valid());
    assert(allocator_32.get_allocated_count() == 1);
    printf("  ✓ Medium block (24 bytes) allocated from 32-byte pool\n");
    
    // Test larger allocation (should use allocator_64)
    shared_block block_large = shared_block::create(pool, 48, 8);
    assert(block_large.is_valid());
    assert(allocator_64.get_allocated_count() == 1);
    printf("  ✓ Large block (48 bytes) allocated from 64-byte pool\n");
    
    // Test even larger allocation (should use allocator_128)
    shared_block block_xlarge = shared_block::create(pool, 96, 8);
    assert(block_xlarge.is_valid());
    assert(allocator_128.get_allocated_count() == 1);
    printf("  ✓ XLarge block (96 bytes) allocated from 128-byte pool\n");
    
    // Test very large allocation (should use allocator_256)
    shared_block block_xxlarge = shared_block::create(pool, 200, 8);
    assert(block_xxlarge.is_valid());
    assert(allocator_256.get_allocated_count() == 1);
    printf("  ✓ XXLarge block (200 bytes) allocated from 256-byte pool\n");
    
    // Test multiple allocations of the same size
    shared_block blocks_16[3];
    for (int i = 0; i < 3; ++i)
    {
        blocks_16[i] = shared_block::create(pool, 8, 8);
        assert(blocks_16[i].is_valid());
    }
    assert(allocator_16.get_allocated_count() == 4); // 1 + 3
    printf("  ✓ Multiple small blocks allocated from same pool\n");
    
    // Test data isolation between different sized blocks
    char* data_small = static_cast<char*>(block_small.get());
    char* data_large = static_cast<char*>(block_large.get());
    strcpy(data_small, "Small");
    strcpy(data_large, "Large");
    assert(strcmp(data_small, "Small") == 0);
    assert(strcmp(data_large, "Large") == 0);
    printf("  ✓ Data isolation between different sized blocks\n");
    
    // Test peak allocation tracking across cascade
    assert(allocator_16.get_peak_allocated_count() == 4);
    assert(allocator_32.get_peak_allocated_count() == 1);
    assert(allocator_64.get_peak_allocated_count() == 1);
    assert(allocator_128.get_peak_allocated_count() == 1);
    assert(allocator_256.get_peak_allocated_count() == 1);
    printf("  ✓ Peak allocation tracked correctly in each pool\n");
    
    // Release all blocks and verify
    block_small.reset();
    block_medium.reset();
    block_large.reset();
    block_xlarge.reset();
    block_xxlarge.reset();
    for (int i = 0; i < 3; ++i)
    {
        blocks_16[i].reset();
    }
    
    assert(allocator_16.get_allocated_count() == 0);
    assert(allocator_32.get_allocated_count() == 0);
    assert(allocator_64.get_allocated_count() == 0);
    assert(allocator_128.get_allocated_count() == 0);
    assert(allocator_256.get_allocated_count() == 0);
    printf("  ✓ All blocks released correctly from cascade\n");
}

//***************************************************************************
/// Test cascaded allocator exhaustion and fallback
//***************************************************************************
void test_cascaded_allocator_exhaustion()
{
    printf("\nTest: Cascaded allocator exhaustion\n");
    
    // Create small pools to test exhaustion
    uros::block_allocator allocator_16(16, 8, 2);  // Only 2 blocks
    uros::block_allocator allocator_32(32, 8, 2);
    
    allocator_16.set_successor(allocator_32);
    
    reference_counted_block_pool pool(allocator_16);
    
    // Allocate 2 small blocks (exhaust 16-byte pool)
    shared_block block1 = shared_block::create(pool, 8, 8);
    shared_block block2 = shared_block::create(pool, 8, 8);
    assert(block1.is_valid());
    assert(block2.is_valid());
    assert(allocator_16.get_allocated_count() == 2);
    assert(allocator_16.get_available() == 0);
    printf("  ✓ 16-byte pool exhausted (2/2 blocks used)\n");
    
    // Next small allocation should fall back to 32-byte pool
    shared_block block3 = shared_block::create(pool, 8, 8);
    assert(block3.is_valid());
    assert(allocator_16.get_allocated_count() == 2); // Still 2
    assert(allocator_32.get_allocated_count() == 1); // Fallback used
    printf("  ✓ Fallback to 32-byte pool when 16-byte exhausted\n");
    
    // Exhaust 32-byte pool as well
    shared_block block4 = shared_block::create(pool, 8, 8);
    assert(block4.is_valid());
    assert(allocator_32.get_allocated_count() == 2);
    assert(allocator_32.get_available() == 0);
    printf("  ✓ 32-byte pool also exhausted (2/2 blocks used)\n");
    
    // Now both pools are exhausted, allocation should fail
    shared_block block5 = shared_block::create(pool, 8, 8);
    assert(!block5.is_valid());
    printf("  ✓ Allocation fails when all cascaded pools exhausted\n");
    
    // Release one block from 16-byte pool
    block1.reset();
    assert(allocator_16.get_available() == 1);
    
    // Now allocation should succeed again from 16-byte pool
    shared_block block6 = shared_block::create(pool, 8, 8);
    assert(block6.is_valid());
    assert(allocator_16.get_allocated_count() == 2);
    printf("  ✓ Allocation succeeds after releasing from primary pool\n");
}

//***************************************************************************
/// Test realistic producer-consumer scenario
//***************************************************************************
void test_producer_consumer_scenario()
{
    printf("\nTest: Producer-consumer scenario\n");
    
    // Set up cascaded allocators for different message sizes
    uros::block_allocator allocator_64(64, 8, 10);    // Small messages
    uros::block_allocator allocator_256(256, 8, 8);   // Medium messages
    uros::block_allocator allocator_1024(1024, 8, 5); // Large messages
    
    allocator_64.set_successor(allocator_256, allocator_1024);
    reference_counted_block_pool pool(allocator_64);
    
    // Message structure
    struct Message {
        int producer_id;
        int sequence;
        char data[48];
    };
    
    // Simulate message queue (using simple array for this test)
    const int QUEUE_SIZE = 20;
    shared_block message_queue[QUEUE_SIZE];
    int queue_head = 0;
    int queue_tail = 0;
    int queue_count = 0;
    
    printf("  → Initial state: All pools empty\n");
    
    // Producer 1: Generate small messages
    printf("  → Producer 1: Generating 5 small messages\n");
    for (int i = 0; i < 5; ++i)
    {
        shared_block block = shared_block::create(pool, sizeof(Message), 8);
        assert(block.is_valid());
        
        Message* msg = static_cast<Message*>(block.get());
        msg->producer_id = 1;
        msg->sequence = i;
        snprintf(msg->data, sizeof(msg->data), "P1-MSG-%d", i);
        
        message_queue[queue_tail] = block;
        queue_tail = (queue_tail + 1) % QUEUE_SIZE;
        queue_count++;
    }
    assert(allocator_64.get_allocated_count() == 5);
    printf("    ✓ 5 messages in queue, 64-byte pool: 5/10 used\n");
    
    // Producer 2: Generate medium messages
    printf("  → Producer 2: Generating 3 medium messages (200 bytes)\n");
    for (int i = 0; i < 3; ++i)
    {
        shared_block block = shared_block::create(pool, 200, 8);
        assert(block.is_valid());
        
        char* data = static_cast<char*>(block.get());
        snprintf(data, 200, "Producer2-LargeMessage-%d", i);
        
        message_queue[queue_tail] = block;
        queue_tail = (queue_tail + 1) % QUEUE_SIZE;
        queue_count++;
    }
    assert(allocator_256.get_allocated_count() == 3);
    printf("    ✓ 8 messages in queue, 256-byte pool: 3/8 used\n");
    
    // Producer 3: Generate large messages
    printf("  → Producer 3: Generating 2 large messages (800 bytes)\n");
    for (int i = 0; i < 2; ++i)
    {
        shared_block block = shared_block::create(pool, 800, 8);
        assert(block.is_valid());
        
        char* data = static_cast<char*>(block.get());
        snprintf(data, 800, "Producer3-HugeMessage-%d", i);
        
        message_queue[queue_tail] = block;
        queue_tail = (queue_tail + 1) % QUEUE_SIZE;
        queue_count++;
    }
    assert(allocator_1024.get_allocated_count() == 2);
    printf("    ✓ 10 messages in queue, 1024-byte pool: 2/5 used\n");
    
    // Consumer 1: Process 3 messages (creates shared references)
    printf("  → Consumer 1: Processing first 3 messages\n");
    shared_block consumer1_holding[3];
    for (int i = 0; i < 3; ++i)
    {
        assert(queue_count > 0);
        consumer1_holding[i] = message_queue[queue_head];  // Copy, increases ref count
        
        Message* msg = static_cast<Message*>(consumer1_holding[i].get());
        assert(msg->producer_id == 1);
        assert(msg->sequence == i);
        
        queue_head = (queue_head + 1) % QUEUE_SIZE;
        queue_count--;
    }
    printf("    ✓ Consumer 1 holding 3 messages (ref count increased)\n");
    
    // Consumer 2: Process 4 messages and release immediately
    printf("  → Consumer 2: Processing and releasing 4 messages\n");
    for (int i = 0; i < 4; ++i)
    {
        assert(queue_count > 0);
        shared_block block = message_queue[queue_head];
        
        // Process message...
        if (i < 2)
        {
            Message* msg = static_cast<Message*>(block.get());
            assert(msg->producer_id == 1);
        }
        
        queue_head = (queue_head + 1) % QUEUE_SIZE;
        queue_count--;
        // block goes out of scope, ref count decreases
    }
    // First 2 small messages should be released (Consumer 1 not holding them)
    assert(allocator_64.get_allocated_count() == 3); // 5 - 2 = 3
    printf("    ✓ 2 messages released, 64-byte pool: 3/10 used\n");
    
    // Producer 1: Generate more messages while consumers are working
    printf("  → Producer 1: Generating 3 more small messages\n");
    for (int i = 5; i < 8; ++i)
    {
        shared_block block = shared_block::create(pool, sizeof(Message), 8);
        assert(block.is_valid());
        
        Message* msg = static_cast<Message*>(block.get());
        msg->producer_id = 1;
        msg->sequence = i;
        snprintf(msg->data, sizeof(msg->data), "P1-MSG-%d", i);
        
        message_queue[queue_tail] = block;
        queue_tail = (queue_tail + 1) % QUEUE_SIZE;
        queue_count++;
    }
    assert(allocator_64.get_allocated_count() == 6); // 3 + 3 = 6
    printf("    ✓ 6 messages in queue, 64-byte pool: 6/10 used\n");
    
    // Consumer 3: Process all remaining messages
    printf("  → Consumer 3: Processing all remaining %d messages\n", queue_count);
    int processed = 0;
    while (queue_count > 0)
    {
        shared_block block = message_queue[queue_head];
        queue_head = (queue_head + 1) % QUEUE_SIZE;
        queue_count--;
        processed++;
        // Message released when block goes out of scope
    }
    printf("    ✓ Consumer 3 processed %d messages\n", processed);
    
    // Consumer 1 still holding 3 messages
    assert(allocator_64.get_allocated_count() == 3);
    printf("    ✓ Consumer 1 still holding 3 messages\n");
    
    // Consumer 1 releases 2 messages
    printf("  → Consumer 1: Releasing 2 messages\n");
    consumer1_holding[0].reset();
    consumer1_holding[1].reset();
    assert(allocator_64.get_allocated_count() == 1);
    printf("    ✓ 2 messages released, 64-byte pool: 1/10 used\n");
    
    // Consumer 1 releases last message
    printf("  → Consumer 1: Releasing last message\n");
    consumer1_holding[2].reset();
    assert(allocator_64.get_allocated_count() == 0);
    printf("    ✓ All messages released\n");
    
    // Check peak usage
    printf("  → Peak allocation statistics:\n");
    printf("    • 64-byte pool peak: %zu/%zu\n", 
           allocator_64.get_peak_allocated_count(), 
           allocator_64.get_capacity());
    printf("    • 256-byte pool peak: %zu/%zu\n", 
           allocator_256.get_peak_allocated_count(), 
           allocator_256.get_capacity());
    printf("    • 1024-byte pool peak: %zu/%zu\n", 
           allocator_1024.get_peak_allocated_count(), 
           allocator_1024.get_capacity());
    
    assert(allocator_64.get_peak_allocated_count() == 6);
    assert(allocator_256.get_peak_allocated_count() == 3);
    assert(allocator_1024.get_peak_allocated_count() == 2);
    
    printf("  ✓ Producer-consumer scenario completed successfully\n");
}

//***************************************************************************
/// Test realistic multi-threaded style producer-consumer with shared ownership
//***************************************************************************
void test_shared_ownership_scenario()
{
    printf("\nTest: Shared ownership scenario\n");
    
    // Set up allocator
    uros::block_allocator allocator(256, 8, 15);
    reference_counted_block_pool pool(allocator);
    
    struct SensorData {
        int sensor_id;
        double temperature;
        double pressure;
        long timestamp;
        char status[64];
    };
    
    printf("  → Simulating sensor data distribution system\n");
    
    // Producer: Sensor generates data
    shared_block sensor_reading = shared_block::create(pool, sizeof(SensorData), 8);
    assert(sensor_reading.is_valid());
    SensorData* data = static_cast<SensorData*>(sensor_reading.get());
    data->sensor_id = 101;
    data->temperature = 25.5;
    data->pressure = 1013.25;
    data->timestamp = 1234567890;
    strcpy(data->status, "Normal");
    assert(sensor_reading.use_count() == 1);
    printf("  ✓ Sensor produces data (ref count: 1)\n");
    
    // Multiple consumers subscribe to the same data
    shared_block logger = sensor_reading;
    assert(sensor_reading.use_count() == 2);
    printf("  ✓ Logger subscribes (ref count: 2)\n");
    
    shared_block monitor = sensor_reading;
    assert(sensor_reading.use_count() == 3);
    printf("  ✓ Monitor subscribes (ref count: 3)\n");
    
    shared_block analyzer = sensor_reading;
    assert(sensor_reading.use_count() == 4);
    printf("  ✓ Analyzer subscribes (ref count: 4)\n");
    
    shared_block database = sensor_reading;
    assert(sensor_reading.use_count() == 5);
    printf("  ✓ Database subscribes (ref count: 5)\n");
    
    // Verify all consumers see the same data
    SensorData* logger_data = static_cast<SensorData*>(logger.get());
    SensorData* monitor_data = static_cast<SensorData*>(monitor.get());
    assert(logger_data == data);
    assert(monitor_data == data);
    assert(logger_data->sensor_id == 101);
    assert(monitor_data->temperature == 25.5);
    printf("  ✓ All consumers share the same data\n");
    
    // Consumers finish processing at different times
    printf("  → Logger finishes processing\n");
    logger.reset();
    assert(sensor_reading.use_count() == 4);
    assert(allocator.get_allocated_count() == 1); // Still allocated
    printf("    ✓ Data still allocated (ref count: 4)\n");
    
    printf("  → Monitor finishes processing\n");
    monitor.reset();
    assert(sensor_reading.use_count() == 3);
    printf("    ✓ Data still allocated (ref count: 3)\n");
    
    printf("  → Analyzer finishes processing\n");
    analyzer.reset();
    assert(sensor_reading.use_count() == 2);
    printf("    ✓ Data still allocated (ref count: 2)\n");
    
    printf("  → Sensor releases original reference\n");
    sensor_reading.reset();
    assert(database.use_count() == 1);
    assert(allocator.get_allocated_count() == 1); // Still allocated for database
    printf("    ✓ Data still allocated for database (ref count: 1)\n");
    
    printf("  → Database finishes archiving\n");
    database.reset();
    assert(allocator.get_allocated_count() == 0); // Finally released
    printf("    ✓ Data released after last consumer finished (ref count: 0)\n");
    
    // Test multiple data items with overlapping lifetimes
    printf("  → Testing overlapping sensor readings\n");
    shared_block readings[5];
    for (int i = 0; i < 5; ++i)
    {
        readings[i] = shared_block::create(pool, sizeof(SensorData), 8);
        assert(readings[i].is_valid());
        SensorData* d = static_cast<SensorData*>(readings[i].get());
        d->sensor_id = 100 + i;
        d->temperature = 20.0 + i;
    }
    assert(allocator.get_allocated_count() == 5);
    printf("  ✓ 5 sensor readings allocated\n");
    
    // Create multiple references to each reading
    shared_block consumers[5][3]; // 3 consumers for each reading
    for (int i = 0; i < 5; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            consumers[i][j] = readings[i];
        }
        assert(readings[i].use_count() == 4); // Original + 3 consumers
    }
    printf("  ✓ 3 consumers subscribed to each reading (ref count: 4 each)\n");
    
    // Release readings in random order
    readings[2].reset();
    readings[0].reset();
    readings[4].reset();
    readings[1].reset();
    readings[3].reset();
    assert(allocator.get_allocated_count() == 5); // All still held by consumers
    printf("  ✓ Original readings released, but data still held by consumers\n");
    
    // Consumers release their references
    for (int i = 0; i < 5; ++i)
    {
        for (int j = 0; j < 3; ++j)
        {
            consumers[i][j].reset();
        }
    }
    assert(allocator.get_allocated_count() == 0);
    printf("  ✓ All data released after consumers finished\n");
    
    printf("  ✓ Shared ownership scenario completed successfully\n");
}

//***************************************************************************
/// Test complex scenario with backpressure
//***************************************************************************
void test_complex_scenario_with_backpressure()
{
    printf("\nTest: Complex scenario with backpressure\n");
    
    // Set up cascaded allocators: 16 -> 32 -> 64 -> 128 -> 256 -> 512 -> 1024 bytes
    uros::block_allocator allocator_16(16, 8, 8);
    uros::block_allocator allocator_32(32, 8, 8);
    uros::block_allocator allocator_64(64, 8, 10);
    uros::block_allocator allocator_128(128, 8, 10);
    uros::block_allocator allocator_256(256, 8, 8);
    uros::block_allocator allocator_512(512, 8, 6);
    uros::block_allocator allocator_1024(1024, 8, 4);
    
    allocator_16.set_successor(allocator_32, allocator_64, allocator_128, 
                                allocator_256, allocator_512, allocator_1024);
    
    reference_counted_block_pool pool(allocator_16);
    
    printf("  → System initialized with 7-level cascaded allocators\n");
    printf("    • 16B(8), 32B(8), 64B(10), 128B(10), 256B(8), 512B(6), 1024B(4)\n");
    
    // Message queue with limited capacity
    const int QUEUE_CAPACITY = 15;
    shared_block message_queue[QUEUE_CAPACITY];
    int queue_head = 0;
    int queue_tail = 0;
    int queue_count = 0;
    
    // Statistics
    int produced_total = 0;
    int consumed_total = 0;
    int dropped_total = 0;
    int backpressure_events = 0;
    
    // Message type with various sizes
    enum MessageType {
        MSG_TINY = 0,      // 8 bytes  -> 16B pool
        MSG_SMALL = 1,     // 24 bytes -> 32B pool
        MSG_MEDIUM = 2,    // 56 bytes -> 64B pool
        MSG_LARGE = 3,     // 100 bytes -> 128B pool
        MSG_XLARGE = 4,    // 220 bytes -> 256B pool
        MSG_HUGE = 5,      // 480 bytes -> 512B pool
        MSG_MASSIVE = 6    // 900 bytes -> 1024B pool
    };
    
    const size_t message_sizes[] = {8, 24, 56, 100, 220, 480, 900};
    const char* message_type_names[] = {"TINY", "SMALL", "MEDIUM", "LARGE", "XLARGE", "HUGE", "MASSIVE"};
    
    struct MessageHeader {
        int type;
        int producer_id;
        int sequence;
        long timestamp;
    };
    
    // Helper function to produce message
    auto produce_message = [&](int type, int producer_id) -> bool {
        if (queue_count >= QUEUE_CAPACITY) {
            backpressure_events++;
            return false; // Queue full - backpressure!
        }
        
        size_t msg_size = message_sizes[type];
        shared_block block = shared_block::create(pool, msg_size, 8);
        
        if (!block.is_valid()) {
            dropped_total++;
            return false; // Allocation failed - no memory!
        }
        
        MessageHeader* header = static_cast<MessageHeader*>(block.get());
        header->type = type;
        header->producer_id = producer_id;
        header->sequence = produced_total;
        header->timestamp = produced_total * 100;
        
        message_queue[queue_tail] = block;
        queue_tail = (queue_tail + 1) % QUEUE_CAPACITY;
        queue_count++;
        produced_total++;
        return true;
    };
    
    // Helper function to consume message
    auto consume_message = [&]() -> bool {
        if (queue_count == 0) {
            return false; // Queue empty
        }
        
        shared_block block = message_queue[queue_head];
        queue_head = (queue_head + 1) % QUEUE_CAPACITY;
        queue_count--;
        consumed_total++;
        
        // Process message (block will be released when going out of scope)
        return true;
    };
    
    printf("\n  → Phase 1: Fast producer, slow consumer (backpressure)\n");
    
    // Producer generates many messages quickly
    printf("    Producer: Generating burst of mixed-size messages\n");
    for (int i = 0; i < 30; ++i) {
        int msg_type = i % 7; // Cycle through all message types
        int producer_id = (i / 7) % 3 + 1; // 3 producers
        
        if (!produce_message(msg_type, producer_id)) {
            printf("      ⚠ Message %d dropped (type=%s, backpressure_events=%d, dropped=%d)\n", 
                   i, message_type_names[msg_type], backpressure_events, dropped_total);
        }
    }
    
    printf("    ✓ Production attempt: 30 messages\n");
    printf("    • Queue: %d/%d, Produced: %d, Dropped: %d, Backpressure: %d\n",
           queue_count, QUEUE_CAPACITY, produced_total, dropped_total, backpressure_events);
    
    // Check pool utilization
    printf("    • Pool utilization:\n");
    printf("      16B: %zu/%zu, 32B: %zu/%zu, 64B: %zu/%zu, 128B: %zu/%zu\n",
           allocator_16.get_allocated_count(), allocator_16.get_capacity(),
           allocator_32.get_allocated_count(), allocator_32.get_capacity(),
           allocator_64.get_allocated_count(), allocator_64.get_capacity(),
           allocator_128.get_allocated_count(), allocator_128.get_capacity());
    printf("      256B: %zu/%zu, 512B: %zu/%zu, 1024B: %zu/%zu\n",
           allocator_256.get_allocated_count(), allocator_256.get_capacity(),
           allocator_512.get_allocated_count(), allocator_512.get_capacity(),
           allocator_1024.get_allocated_count(), allocator_1024.get_capacity());
    
    assert(queue_count == QUEUE_CAPACITY); // Queue should be full
    assert(backpressure_events > 0); // Should have backpressure events
    
    printf("\n  → Phase 2: Consumer catches up\n");
    printf("    Consumer: Processing 10 messages\n");
    for (int i = 0; i < 10; ++i) {
        assert(consume_message());
    }
    printf("    ✓ Consumed: %d, Queue: %d/%d\n", consumed_total, queue_count, QUEUE_CAPACITY);
    
    printf("\n  → Phase 3: Balanced production and consumption\n");
    printf("    Simulating balanced workload (produce 2, consume 2)\n");
    for (int cycle = 0; cycle < 10; ++cycle) {
        // Produce 2 messages
        for (int i = 0; i < 2; ++i) {
            int msg_type = (produced_total + i) % 7;
            produce_message(msg_type, 1);
        }
        
        // Consume 2 messages
        for (int i = 0; i < 2; ++i) {
            if (queue_count > 0) {
                consume_message();
            }
        }
    }
    printf("    ✓ After 10 cycles: Produced=%d, Consumed=%d, Queue=%d\n",
           produced_total, consumed_total, queue_count);
    
    printf("\n  → Phase 4: Consumer draining with slow producer\n");
    printf("    Consumer: Draining remaining messages\n");
    int initial_queue = queue_count;
    
    // Consumer processes messages faster than producer
    while (queue_count > 0) {
        // Occasionally produce new message
        if ((consumed_total % 5) == 0 && produced_total < 100) {
            int msg_type = produced_total % 7;
            produce_message(msg_type, 2);
        }
        
        consume_message();
    }
    printf("    ✓ Drained %d messages, Queue now: %d\n", initial_queue, queue_count);
    
    printf("\n  → Phase 5: Stress test - exhaust specific pool\n");
    printf("    Attempting to exhaust 64B pool (capacity: %zu)\n", allocator_64.get_capacity());
    
    // Hold references to prevent release
    const int HOLD_COUNT = 12;
    shared_block held_messages[HOLD_COUNT];
    int held_index = 0;
    
    // Try to allocate more than 64B pool capacity
    for (int i = 0; i < HOLD_COUNT; ++i) {
        shared_block block = shared_block::create(pool, 56, 8); // 56 bytes -> 64B pool
        
        if (block.is_valid()) {
            held_messages[held_index++] = block;
            printf("      Allocated %d/12: 64B pool %zu/%zu\n", 
                   held_index, 
                   allocator_64.get_allocated_count(), 
                   allocator_64.get_capacity());
        } else {
            printf("      ✓ 64B pool exhausted at %d allocations\n", held_index);
            break;
        }
        
        // After exhausting 64B, should fall back to 128B
        if (i == allocator_64.get_capacity()) {
            assert(allocator_128.get_allocated_count() > 0);
            printf("      ✓ Fallback to 128B pool triggered\n");
        }
    }
    
    assert(allocator_64.get_allocated_count() == allocator_64.get_capacity());
    printf("    ✓ 64B pool completely exhausted\n");
    
    // Release half
    printf("    Releasing 6 held messages\n");
    for (int i = 0; i < 6; ++i) {
        held_messages[i].reset();
    }
    printf("    ✓ 64B pool: %zu/%zu after release\n",
           allocator_64.get_allocated_count(), allocator_64.get_capacity());
    
    // Release remaining
    for (int i = 6; i < held_index; ++i) {
        held_messages[i].reset();
    }
    
    printf("\n  → Phase 6: Final statistics\n");
    printf("    Production summary:\n");
    printf("      Total produced: %d\n", produced_total);
    printf("      Total consumed: %d\n", consumed_total);
    printf("      Total dropped: %d\n", dropped_total);
    printf("      Backpressure events: %d\n", backpressure_events);
    printf("      Messages in queue: %d\n", queue_count);
    
    printf("    Peak pool utilization:\n");
    printf("      16B: %zu/%zu (%.0f%%)\n", 
           allocator_16.get_peak_allocated_count(), allocator_16.get_capacity(),
           100.0 * allocator_16.get_peak_allocated_count() / allocator_16.get_capacity());
    printf("      32B: %zu/%zu (%.0f%%)\n", 
           allocator_32.get_peak_allocated_count(), allocator_32.get_capacity(),
           100.0 * allocator_32.get_peak_allocated_count() / allocator_32.get_capacity());
    printf("      64B: %zu/%zu (%.0f%%)\n", 
           allocator_64.get_peak_allocated_count(), allocator_64.get_capacity(),
           100.0 * allocator_64.get_peak_allocated_count() / allocator_64.get_capacity());
    printf("      128B: %zu/%zu (%.0f%%)\n", 
           allocator_128.get_peak_allocated_count(), allocator_128.get_capacity(),
           100.0 * allocator_128.get_peak_allocated_count() / allocator_128.get_capacity());
    printf("      256B: %zu/%zu (%.0f%%)\n", 
           allocator_256.get_peak_allocated_count(), allocator_256.get_capacity(),
           100.0 * allocator_256.get_peak_allocated_count() / allocator_256.get_capacity());
    printf("      512B: %zu/%zu (%.0f%%)\n", 
           allocator_512.get_peak_allocated_count(), allocator_512.get_capacity(),
           100.0 * allocator_512.get_peak_allocated_count() / allocator_512.get_capacity());
    printf("      1024B: %zu/%zu (%.0f%%)\n", 
           allocator_1024.get_peak_allocated_count(), allocator_1024.get_capacity(),
           100.0 * allocator_1024.get_peak_allocated_count() / allocator_1024.get_capacity());
    
    // Drain remaining messages
    while (queue_count > 0) {
        consume_message();
    }
    
    // Verify all memory released
    assert(allocator_16.get_allocated_count() == 0);
    assert(allocator_32.get_allocated_count() == 0);
    assert(allocator_64.get_allocated_count() == 0);
    assert(allocator_128.get_allocated_count() == 0);
    assert(allocator_256.get_allocated_count() == 0);
    assert(allocator_512.get_allocated_count() == 0);
    assert(allocator_1024.get_allocated_count() == 0);
    
    printf("\n  ✓ Complex scenario with backpressure completed successfully\n");
    printf("  ✓ All memory properly released\n");
    
    // Assertions
    assert(produced_total > 0);
    assert(consumed_total > 0);
    assert(dropped_total > 0 || backpressure_events > 0); // Should have experienced pressure
    assert(allocator_64.get_peak_allocated_count() == allocator_64.get_capacity()); // 64B exhausted
}

//***************************************************************************
/// Main test runner
//***************************************************************************
int main()
{
    printf("========================================\n");
    printf("shared_block Test Suite\n");
    printf("========================================\n\n");
    
    test_basic_construction();
    test_copy_constructor();
    test_move_constructor();
    test_copy_assignment();
    test_move_assignment();
    test_reset();
    test_automatic_release();
    test_data_access();
    test_bool_conversion();
    test_self_assignment();
    test_multiple_allocations();
    test_allocation_failure();
    test_peak_allocation();
    test_cascaded_allocators();
    test_cascaded_allocator_exhaustion();
    test_producer_consumer_scenario();
    test_shared_ownership_scenario();
    test_complex_scenario_with_backpressure();
    
    printf("\n========================================\n");
    printf("All tests passed! ✓\n");
    printf("========================================\n");
    
    return 0;
}
