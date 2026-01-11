///\file
/// Multi-threaded test suite for shared_block

#include "uros2/shared_block.h"
#include "uros2/reference_counted_block_pool.h"
#include "uros2/block_allocator.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <pthread.h>
#include <unistd.h>
#include <atomic>

using namespace uros;

//***************************************************************************
/// Helper structures and functions for multi-threaded test
//***************************************************************************

// Shared state between threads
struct MTTestSharedState {
    enum { QUEUE_CAPACITY = 50 };
    
    // Cascaded allocators
    uros::block_allocator allocator_64;
    uros::block_allocator allocator_256;
    uros::block_allocator allocator_1024;
    reference_counted_block_pool pool;
    
    // Thread-safe queue (simple mutex-based)
    shared_block message_queue[QUEUE_CAPACITY];
    volatile int queue_head;
    volatile int queue_tail;
    volatile int queue_count;
    pthread_mutex_t queue_mutex;
    
    // Statistics (atomic for thread-safety)
    std::atomic<int> produced_total;
    std::atomic<int> consumed_total;
    std::atomic<int> dropped_total;
    std::atomic<int> backpressure_events;
    
    // Control flags
    volatile bool stop_producers;
    volatile bool stop_consumers;
    
    MTTestSharedState() 
        : allocator_64(64, 8, 20)
        , allocator_256(256, 8, 15)
        , allocator_1024(1024, 8, 10)
        , pool(allocator_64)
        , queue_head(0)
        , queue_tail(0)
        , queue_count(0)
        , produced_total(0)
        , consumed_total(0)
        , dropped_total(0)
        , backpressure_events(0)
        , stop_producers(false)
        , stop_consumers(false)
    {
        allocator_64.set_successor(allocator_256, allocator_1024);
        pthread_mutex_init(&queue_mutex, NULL);
    }
    
    ~MTTestSharedState() {
        pthread_mutex_destroy(&queue_mutex);
    }
    
    bool enqueue(shared_block& block) {
        pthread_mutex_lock(&queue_mutex);
        
        if (queue_count >= QUEUE_CAPACITY) {
            pthread_mutex_unlock(&queue_mutex);
            backpressure_events++;
            return false;
        }
        
        message_queue[queue_tail] = block;
        queue_tail = (queue_tail + 1) % QUEUE_CAPACITY;
        queue_count++;
        
        pthread_mutex_unlock(&queue_mutex);
        return true;
    }
    
    bool dequeue(shared_block& block) {
        pthread_mutex_lock(&queue_mutex);
        
        if (queue_count == 0) {
            pthread_mutex_unlock(&queue_mutex);
            return false;
        }
        
        block = message_queue[queue_head];
        message_queue[queue_head].reset(); // Clear slot
        queue_head = (queue_head + 1) % QUEUE_CAPACITY;
        queue_count--;
        
        pthread_mutex_unlock(&queue_mutex);
        return true;
    }
};

struct MTTestMessageHeader {
    int producer_id;
    int sequence;
    int size_category; // 0=small, 1=medium, 2=large
    long timestamp;
    char data[48];
};

// Producer thread function
struct ProducerArgs {
    MTTestSharedState* state;
    int producer_id;
    int messages_to_produce;
};

void* producer_thread_func(void* arg) {
    ProducerArgs* args = static_cast<ProducerArgs*>(arg);
    MTTestSharedState* state = args->state;
    int producer_id = args->producer_id;
    int messages_to_produce = args->messages_to_produce;
    
    printf("    [Producer %d] Started, target: %d messages\n", 
           producer_id, messages_to_produce);
    
    int produced = 0;
    int failed = 0;
    
    while (produced < messages_to_produce && !state->stop_producers) {
        // Vary message sizes to use all three pools
        // Note: Each block has ~24 bytes overhead (reference_counted_block)
        // So for 64B pool, user data should be ~40 bytes
        int size_category = produced % 3;
        size_t msg_size;
        
        switch (size_category) {
            case 0: msg_size = 32; break;   // 32 bytes -> 64B pool (32 + 24 = 56, fits in 64)
            case 1: msg_size = 160; break;  // 160 bytes -> 256B pool (160 + 24 = 184, fits in 256)
            case 2: msg_size = 700; break;  // 700 bytes -> 1024B pool (700 + 24 = 724, fits in 1024)
            default: msg_size = 32; break;
        }
        
        // Allocate message
        shared_block block = shared_block::create(state->pool, msg_size, 8);
        
        if (!block.is_valid()) {
            // Allocation failed - skip this message and continue
            state->dropped_total++;
            failed++;
            produced++; // Move to next message
            usleep(100); // Small delay before next attempt
            continue;
        }
        
        // Fill message header
        MTTestMessageHeader* header = static_cast<MTTestMessageHeader*>(block.get());
        header->producer_id = producer_id;
        header->sequence = produced;
        header->size_category = size_category;
        header->timestamp = produced * 100;
        snprintf(header->data, sizeof(header->data), 
                 "P%d-MSG%d-SIZE%d", producer_id, produced, size_category);
        
        // Try to enqueue
        if (state->enqueue(block)) {
            state->produced_total++;
            produced++;
        } else {
            // Queue full - backpressure, skip and try next message
            failed++;
            produced++; // Move to next message to avoid infinite loop
            usleep(500); // Back off
        }
        
        // Simulate variable production rate
        if ((produced % 10) == 0 && produced > 0) {
            usleep(100); // Small pause every 10 messages
        }
    }
    
    printf("    [Producer %d] Finished: %d produced (target: %d), %d failed\n", 
           producer_id, produced, messages_to_produce, failed);
    
    return NULL;
}

// Consumer thread function
struct ConsumerArgs {
    MTTestSharedState* state;
    int consumer_id;
};

void* consumer_thread_func(void* arg) {
    ConsumerArgs* args = static_cast<ConsumerArgs*>(arg);
    MTTestSharedState* state = args->state;
    int consumer_id = args->consumer_id;
    
    printf("    [Consumer %d] Started\n", consumer_id);
    
    int consumed = 0;
    int empty_reads = 0;
    
    while (!state->stop_consumers) {
        shared_block block;
        
        if (state->dequeue(block)) {
            // Process message
            MTTestMessageHeader* header = static_cast<MTTestMessageHeader*>(block.get());
            
            // Verify message integrity
            assert(header->producer_id >= 1 && header->producer_id <= 3);
            assert(header->size_category >= 0 && header->size_category <= 2);
            
            consumed++;
            state->consumed_total++;
            empty_reads = 0;
            
            // Simulate variable processing time
            if ((consumed % 7) == 0) {
                usleep(200); // Slow processing occasionally
            } else {
                usleep(50); // Normal processing
            }
            
            // Block will be released when going out of scope
        } else {
            // Queue empty
            empty_reads++;
            usleep(100);
            
            // If queue has been empty for a while and producers stopped, exit
            if (empty_reads > 20 && state->stop_producers) {
                break;
            }
        }
    }
    
    printf("    [Consumer %d] Finished: %d consumed\n", consumer_id, consumed);
    
    return NULL;
}

//***************************************************************************
/// Test multi-threaded producer-consumer with real threads
//***************************************************************************
void test_multithreaded_producer_consumer()
{
    printf("\nTest: Multi-threaded producer-consumer\n");
    
    // Create shared state
    MTTestSharedState state;
    
    printf("  → System initialized with cascaded allocators\n");
    printf("    • 64B(20), 256B(15), 1024B(10)\n");
    
    printf("\n  → Starting threads\n");
    
    // Create threads
    const int NUM_PRODUCERS = 3;
    const int NUM_CONSUMERS = 4;
    const int MESSAGES_PER_PRODUCER = 100;
    
    pthread_t producer_threads[NUM_PRODUCERS];
    pthread_t consumer_threads[NUM_CONSUMERS];
    ProducerArgs producer_args[NUM_PRODUCERS];
    ConsumerArgs consumer_args[NUM_CONSUMERS];
    
    // Start consumer threads first
    printf("    Starting %d consumer threads\n", NUM_CONSUMERS);
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumer_args[i].state = &state;
        consumer_args[i].consumer_id = i + 1;
        pthread_create(&consumer_threads[i], NULL, consumer_thread_func, &consumer_args[i]);
    }
    
    // Start producer threads
    printf("    Starting %d producer threads (%d messages each)\n", 
           NUM_PRODUCERS, MESSAGES_PER_PRODUCER);
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producer_args[i].state = &state;
        producer_args[i].producer_id = i + 1;
        producer_args[i].messages_to_produce = MESSAGES_PER_PRODUCER;
        pthread_create(&producer_threads[i], NULL, producer_thread_func, &producer_args[i]);
    }
    
    printf("  ✓ All threads started\n");
    
    // Wait for producers to finish
    printf("\n  → Waiting for producers to complete\n");
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        pthread_join(producer_threads[i], NULL);
    }
    state.stop_producers = true;
    
    printf("  ✓ All producers finished\n");
    
    int total_attempted = state.produced_total.load() + state.dropped_total.load();
    double drop_rate = (total_attempted > 0) ? 
                       (100.0 * state.dropped_total.load() / total_attempted) : 0.0;
    
    printf("    • Produced: %d, Dropped: %d (%.2f%% drop rate), Backpressure events: %d\n",
           state.produced_total.load(), state.dropped_total.load(), 
           drop_rate, state.backpressure_events.load());
    
    // Wait a bit for consumers to drain the queue
    printf("\n  → Waiting for consumers to drain queue\n");
    int wait_cycles = 0;
    while (state.queue_count > 0 && wait_cycles < 100) {
        usleep(10000); // 10ms
        wait_cycles++;
        if ((wait_cycles % 10) == 0) {
            printf("    Queue: %d remaining\n", state.queue_count);
        }
    }
    
    // Stop consumers
    state.stop_consumers = true;
    
    printf("  → Waiting for consumers to finish\n");
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        pthread_join(consumer_threads[i], NULL);
    }
    
    printf("  ✓ All consumers finished\n");
    printf("    • Consumed: %d\n", state.consumed_total.load());
    
    // Print final statistics
    printf("\n  → Final Statistics\n");
    printf("    Production:\n");
    printf("      Total attempted: %d messages\n", total_attempted);
    printf("      Successfully produced: %d\n", state.produced_total.load());
    printf("      Dropped (allocation failed): %d\n", state.dropped_total.load());
    printf("      Drop rate: %.2f%%\n", drop_rate);
    printf("      Backpressure events (queue full): %d\n", state.backpressure_events.load());
    
    printf("    Consumption:\n");
    printf("      Total consumed: %d\n", state.consumed_total.load());
    printf("      Messages in queue: %d\n", state.queue_count);
    
    printf("    Pool utilization:\n");
    printf("      64B: current=%zu/%zu, peak=%zu (%.0f%%)\n",
           state.allocator_64.get_allocated_count(),
           state.allocator_64.get_capacity(),
           state.allocator_64.get_peak_allocated_count(),
           100.0 * state.allocator_64.get_peak_allocated_count() / state.allocator_64.get_capacity());
    printf("      256B: current=%zu/%zu, peak=%zu (%.0f%%)\n",
           state.allocator_256.get_allocated_count(),
           state.allocator_256.get_capacity(),
           state.allocator_256.get_peak_allocated_count(),
           100.0 * state.allocator_256.get_peak_allocated_count() / state.allocator_256.get_capacity());
    printf("      1024B: current=%zu/%zu, peak=%zu (%.0f%%)\n",
           state.allocator_1024.get_allocated_count(),
           state.allocator_1024.get_capacity(),
           state.allocator_1024.get_peak_allocated_count(),
           100.0 * state.allocator_1024.get_peak_allocated_count() / state.allocator_1024.get_capacity());
    
    // Drain any remaining messages
    printf("\n  → Cleaning up remaining messages\n");
    shared_block block;
    int drained = 0;
    while (state.dequeue(block)) {
        drained++;
    }
    if (drained > 0) {
        printf("    Drained %d remaining messages\n", drained);
    }
    
    // Verify all memory released
    usleep(10000); // Give time for any pending releases
    
    printf("  → Verifying memory cleanup\n");
    printf("    • 64B pool: %zu allocated\n", state.allocator_64.get_allocated_count());
    printf("    • 256B pool: %zu allocated\n", state.allocator_256.get_allocated_count());
    printf("    • 1024B pool: %zu allocated\n", state.allocator_1024.get_allocated_count());
    
    assert(state.allocator_64.get_allocated_count() == 0);
    assert(state.allocator_256.get_allocated_count() == 0);
    assert(state.allocator_1024.get_allocated_count() == 0);
    
    printf("  ✓ All memory properly released\n");
    
    // Assertions
    assert(state.produced_total > 0);
    assert(state.consumed_total > 0);
    assert(state.produced_total == state.consumed_total + state.dropped_total);
    
    printf("\n  ✓ Multi-threaded producer-consumer completed successfully\n");
}

//***************************************************************************
/// Test slow production, fast consumption - should have zero drops
//***************************************************************************

// Slow producer thread (with explicit delays)
void* slow_producer_thread_func(void* arg) {
    ProducerArgs* args = static_cast<ProducerArgs*>(arg);
    MTTestSharedState* state = args->state;
    int producer_id = args->producer_id;
    int messages_to_produce = args->messages_to_produce;
    
    printf("    [Slow Producer %d] Started, target: %d messages\n", 
           producer_id, messages_to_produce);
    
    int produced = 0;
    int failed = 0;
    
    while (produced < messages_to_produce && !state->stop_producers) {
        int size_category = produced % 3;
        size_t msg_size;
        
        switch (size_category) {
            case 0: msg_size = 32; break;
            case 1: msg_size = 160; break;
            case 2: msg_size = 700; break;
            default: msg_size = 32; break;
        }
        
        // Allocate message
        shared_block block = shared_block::create(state->pool, msg_size, 8);
        
        if (!block.is_valid()) {
            state->dropped_total++;
            failed++;
            produced++;
            usleep(1000); // Wait before next attempt
            continue;
        }
        
        // Fill message header
        MTTestMessageHeader* header = static_cast<MTTestMessageHeader*>(block.get());
        header->producer_id = producer_id;
        header->sequence = produced;
        header->size_category = size_category;
        header->timestamp = produced * 100;
        snprintf(header->data, sizeof(header->data), 
                 "P%d-MSG%d-SIZE%d", producer_id, produced, size_category);
        
        // Try to enqueue
        if (state->enqueue(block)) {
            state->produced_total++;
            produced++;
        } else {
            failed++;
            produced++;
            usleep(500);
        }
        
        // Slow production - add significant delay between messages
        usleep(5000); // 5ms delay per message (much slower than fast consumers)
    }
    
    printf("    [Slow Producer %d] Finished: %d produced (target: %d), %d failed\n", 
           producer_id, produced, messages_to_produce, failed);
    
    return NULL;
}

void test_slow_production_fast_consumption()
{
    printf("\nTest: Slow production, fast consumption (no drops expected)\n");
    
    MTTestSharedState state;
    
    printf("  → System initialized with cascaded allocators\n");
    printf("    • 64B(20), 256B(15), 1024B(10)\n");
    
    const int NUM_PRODUCERS = 2;
    const int NUM_CONSUMERS = 4;
    const int MESSAGES_PER_PRODUCER = 50;
    
    pthread_t producer_threads[NUM_PRODUCERS];
    pthread_t consumer_threads[NUM_CONSUMERS];
    ProducerArgs producer_args[NUM_PRODUCERS];
    ConsumerArgs consumer_args[NUM_CONSUMERS];
    
    printf("\n  → Starting threads\n");
    printf("    Starting %d fast consumer threads\n", NUM_CONSUMERS);
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumer_args[i].state = &state;
        consumer_args[i].consumer_id = i + 1;
        pthread_create(&consumer_threads[i], NULL, consumer_thread_func, &consumer_args[i]);
    }
    
    printf("    Starting %d slow producer threads (%d messages each, 5ms delay per message)\n", 
           NUM_PRODUCERS, MESSAGES_PER_PRODUCER);
    
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producer_args[i].state = &state;
        producer_args[i].producer_id = i + 1;
        producer_args[i].messages_to_produce = MESSAGES_PER_PRODUCER;
        pthread_create(&producer_threads[i], NULL, slow_producer_thread_func, &producer_args[i]);
    }
    
    printf("  ✓ All threads started\n");
    
    printf("\n  → Producers running slowly (5ms per message, consumers should drain instantly)\n");
    
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        pthread_join(producer_threads[i], NULL);
    }
    state.stop_producers = true;
    
    printf("  ✓ All producers finished\n");
    
    int total_attempted = state.produced_total.load() + state.dropped_total.load();
    double drop_rate = (total_attempted > 0) ? 
                       (100.0 * state.dropped_total.load() / total_attempted) : 0.0;
    
    printf("    • Produced: %d, Dropped: %d (%.2f%% drop rate)\n",
           state.produced_total.load(), state.dropped_total.load(), drop_rate);
    
    // Wait for consumers
    int wait_cycles = 0;
    while (state.queue_count > 0 && wait_cycles < 50) {
        usleep(10000);
        wait_cycles++;
    }
    
    state.stop_consumers = true;
    
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        pthread_join(consumer_threads[i], NULL);
    }
    
    printf("  ✓ All consumers finished\n");
    printf("    • Consumed: %d\n", state.consumed_total.load());
    
    printf("\n  → Final Statistics\n");
    printf("    Dropped messages: %d\n", state.dropped_total.load());
    printf("    Drop rate: %.2f%%\n", drop_rate);
    printf("    Peak pool utilization:\n");
    printf("      64B: %zu/%zu (%.0f%%)\n",
           state.allocator_64.get_peak_allocated_count(),
           state.allocator_64.get_capacity(),
           100.0 * state.allocator_64.get_peak_allocated_count() / state.allocator_64.get_capacity());
    printf("      256B: %zu/%zu (%.0f%%)\n",
           state.allocator_256.get_peak_allocated_count(),
           state.allocator_256.get_capacity(),
           100.0 * state.allocator_256.get_peak_allocated_count() / state.allocator_256.get_capacity());
    printf("      1024B: %zu/%zu (%.0f%%)\n",
           state.allocator_1024.get_peak_allocated_count(),
           state.allocator_1024.get_capacity(),
           100.0 * state.allocator_1024.get_peak_allocated_count() / state.allocator_1024.get_capacity());
    
    // Cleanup
    shared_block block;
    while (state.dequeue(block)) {}
    usleep(10000);
    
    assert(state.allocator_64.get_allocated_count() == 0);
    assert(state.allocator_256.get_allocated_count() == 0);
    assert(state.allocator_1024.get_allocated_count() == 0);
    
    // Key assertion: with slow production and fast consumption, should have ZERO drops
    if (state.dropped_total.load() != 0) {
        printf("  ✗ FAILED: Expected 0 drops, but got %d drops!\n", state.dropped_total.load());
        assert(false && "Slow production test should have zero drops");
    }
    
    printf("  ✓ Slow production test passed (drops: %d)\n", state.dropped_total.load());
}

//***************************************************************************
/// Test fast production, slow consumption - all pools should reach 100%%
//***************************************************************************

// Slow consumer thread (with explicit delays)
void* slow_consumer_thread_func(void* arg) {
    ConsumerArgs* args = static_cast<ConsumerArgs*>(arg);
    MTTestSharedState* state = args->state;
    int consumer_id = args->consumer_id;
    
    printf("    [Slow Consumer %d] Started\n", consumer_id);
    
    int consumed = 0;
    int empty_reads = 0;
    
    while (!state->stop_consumers) {
        shared_block block;
        
        if (state->dequeue(block)) {
            MTTestMessageHeader* header = static_cast<MTTestMessageHeader*>(block.get());
            
            assert(header->producer_id >= 1 && header->producer_id <= 4);
            assert(header->size_category >= 0 && header->size_category <= 2);
            
            consumed++;
            state->consumed_total++;
            empty_reads = 0;
            
            // Slow processing - significant delay every time
            usleep(3000); // 3ms delay per message (much slower than production)
        } else {
            empty_reads++;
            usleep(1000);
            
            if (empty_reads > 50 && state->stop_producers) {
                break;
            }
        }
    }
    
    printf("    [Slow Consumer %d] Finished: %d consumed\n", consumer_id, consumed);
    
    return NULL;
}

void test_fast_production_slow_consumption()
{
    printf("\nTest: Fast production, slow consumption (100%% pool utilization expected)\n");
    
    MTTestSharedState state;
    
    printf("  → System initialized with cascaded allocators\n");
    printf("    • 64B(20), 256B(15), 1024B(10)\n");
    
    const int NUM_PRODUCERS = 4;
    const int NUM_CONSUMERS = 2;
    const int MESSAGES_PER_PRODUCER = 60;
    
    pthread_t producer_threads[NUM_PRODUCERS];
    pthread_t consumer_threads[NUM_CONSUMERS];
    ProducerArgs producer_args[NUM_PRODUCERS];
    ConsumerArgs consumer_args[NUM_CONSUMERS];
    
    printf("\n  → Starting threads\n");
    printf("    Starting %d slow consumer threads\n", NUM_CONSUMERS);
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        consumer_args[i].state = &state;
        consumer_args[i].consumer_id = i + 1;
        pthread_create(&consumer_threads[i], NULL, slow_consumer_thread_func, &consumer_args[i]);
    }
    
    printf("    Starting %d fast producer threads (%d messages each)\n", 
           NUM_PRODUCERS, MESSAGES_PER_PRODUCER);
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        producer_args[i].state = &state;
        producer_args[i].producer_id = i + 1;
        producer_args[i].messages_to_produce = MESSAGES_PER_PRODUCER;
        pthread_create(&producer_threads[i], NULL, producer_thread_func, &producer_args[i]);
    }
    
    printf("  ✓ All threads started\n");
    
    printf("\n  → Producers running fast, consumers slow (pools should fill up)\n");
    
    // Monitor pool utilization during production
    usleep(500000); // Wait 500ms to let system reach steady state
    
    printf("    Checking pool utilization during execution:\n");
    printf("      64B: current=%zu/%zu (%.0f%%)\n",
           state.allocator_64.get_allocated_count(),
           state.allocator_64.get_capacity(),
           100.0 * state.allocator_64.get_allocated_count() / state.allocator_64.get_capacity());
    printf("      256B: current=%zu/%zu (%.0f%%)\n",
           state.allocator_256.get_allocated_count(),
           state.allocator_256.get_capacity(),
           100.0 * state.allocator_256.get_allocated_count() / state.allocator_256.get_capacity());
    printf("      1024B: current=%zu/%zu (%.0f%%)\n",
           state.allocator_1024.get_allocated_count(),
           state.allocator_1024.get_capacity(),
           100.0 * state.allocator_1024.get_allocated_count() / state.allocator_1024.get_capacity());
    
    for (int i = 0; i < NUM_PRODUCERS; ++i) {
        pthread_join(producer_threads[i], NULL);
    }
    state.stop_producers = true;
    
    printf("  ✓ All producers finished\n");
    
    int total_attempted = state.produced_total.load() + state.dropped_total.load();
    double drop_rate = (total_attempted > 0) ? 
                       (100.0 * state.dropped_total.load() / total_attempted) : 0.0;
    
    printf("    • Produced: %d, Dropped: %d (%.2f%% drop rate)\n",
           state.produced_total.load(), state.dropped_total.load(), drop_rate);
    
    // Wait for consumers to drain
    printf("\n  → Waiting for consumers to drain queue\n");
    int wait_cycles = 0;
    while (state.queue_count > 0 && wait_cycles < 200) {
        usleep(50000); // 50ms
        wait_cycles++;
        if ((wait_cycles % 10) == 0) {
            printf("    Queue: %d remaining, consumed so far: %d\n", 
                   state.queue_count, state.consumed_total.load());
        }
    }
    
    state.stop_consumers = true;
    
    for (int i = 0; i < NUM_CONSUMERS; ++i) {
        pthread_join(consumer_threads[i], NULL);
    }
    
    printf("  ✓ All consumers finished\n");
    printf("    • Consumed: %d\n", state.consumed_total.load());
    
    printf("\n  → Final Statistics\n");
    printf("    Production:\n");
    printf("      Total attempted: %d messages\n", total_attempted);
    printf("      Successfully produced: %d\n", state.produced_total.load());
    printf("      Dropped: %d (%.2f%% drop rate)\n", 
           state.dropped_total.load(), drop_rate);
    
    printf("    Peak pool utilization:\n");
    printf("      64B: %zu/%zu (%.0f%%)\n",
           state.allocator_64.get_peak_allocated_count(),
           state.allocator_64.get_capacity(),
           100.0 * state.allocator_64.get_peak_allocated_count() / state.allocator_64.get_capacity());
    printf("      256B: %zu/%zu (%.0f%%)\n",
           state.allocator_256.get_peak_allocated_count(),
           state.allocator_256.get_capacity(),
           100.0 * state.allocator_256.get_peak_allocated_count() / state.allocator_256.get_capacity());
    printf("      1024B: %zu/%zu (%.0f%%)\n",
           state.allocator_1024.get_peak_allocated_count(),
           state.allocator_1024.get_capacity(),
           100.0 * state.allocator_1024.get_peak_allocated_count() / state.allocator_1024.get_capacity());
    
    // Cleanup
    shared_block block;
    while (state.dequeue(block)) {}
    usleep(10000);
    
    assert(state.allocator_64.get_allocated_count() == 0);
    assert(state.allocator_256.get_allocated_count() == 0);
    assert(state.allocator_1024.get_allocated_count() == 0);
    
    // Key assertions: with fast production and slow consumption
    // 1. Should see drops (pools exhausted)
    if (state.dropped_total.load() == 0) {
        printf("  ✗ FAILED: Expected drops due to pool exhaustion, but got 0 drops!\n");
        assert(false && "Fast production test should have drops");
    }
    
    // 2. ALL pools should have reached 100% utilization
    double peak_64_percent = 100.0 * state.allocator_64.get_peak_allocated_count() / state.allocator_64.get_capacity();
    double peak_256_percent = 100.0 * state.allocator_256.get_peak_allocated_count() / state.allocator_256.get_capacity();
    double peak_1024_percent = 100.0 * state.allocator_1024.get_peak_allocated_count() / state.allocator_1024.get_capacity();
    
    bool all_pools_100_percent = (peak_64_percent == 100.0) && 
                                  (peak_256_percent == 100.0) && 
                                  (peak_1024_percent == 100.0);
    
    if (!all_pools_100_percent) {
        printf("  ✗ FAILED: Not all pools reached 100%% utilization!\n");
        printf("    64B: %.0f%%, 256B: %.0f%%, 1024B: %.0f%%\n",
               peak_64_percent, peak_256_percent, peak_1024_percent);
        assert(false && "All pools should reach 100% utilization in fast production test");
    }
    
    printf("  ✓ Fast production test passed\n");
    printf("    All pools reached high utilization (64B:%.0f%%, 256B:%.0f%%, 1024B:%.0f%%)\n",
           peak_64_percent, peak_256_percent, peak_1024_percent);
}

//***************************************************************************
/// Main test runner
//***************************************************************************
int main()
{
    printf("========================================\n");
    printf("shared_block Multi-threaded Test Suite\n");
    printf("========================================\n\n");
    
    test_multithreaded_producer_consumer();
    test_slow_production_fast_consumption();
    test_fast_production_slow_consumption();
    
    printf("\n========================================\n");
    printf("All tests passed! ✓\n");
    printf("========================================\n");
    
    return 0;
}
