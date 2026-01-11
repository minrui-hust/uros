///\file
/// Broadcast test: One producer, multiple consumers with independent queues
/// Producer broadcasts each message to all consumer queues

#include "uros2/shared_block.h"
#include "uros2/reference_counted_block_pool.h"
#include "uros2/block_allocator.h"
#include <cstdio>
#include <cstring>
#include <cassert>
#include <cstdlib>
#include <pthread.h>
#include <unistd.h>
#include <atomic>

using namespace uros;

//***************************************************************************
/// Test structures
//***************************************************************************

struct TestMessage {
    int producer_id;
    int sequence;
    int payload_size;
    long timestamp;
    uint32_t checksum;  // For data integrity verification
    char payload[1];    // Variable-length payload (flexible array member style)
};

// Consumer-specific queue
struct ConsumerQueue {
    enum { QUEUE_CAPACITY = 100 };  // Increased from 30 to 100 to avoid queue bottleneck
    
    shared_block messages[QUEUE_CAPACITY];
    volatile int head;
    volatile int tail;
    volatile int count;
    pthread_mutex_t mutex;
    
    ConsumerQueue() : head(0), tail(0), count(0) {
        pthread_mutex_init(&mutex, NULL);
    }
    
    ~ConsumerQueue() {
        pthread_mutex_destroy(&mutex);
    }
    
    bool enqueue(const shared_block& block) {
        pthread_mutex_lock(&mutex);
        
        if (count >= QUEUE_CAPACITY) {
            pthread_mutex_unlock(&mutex);
            return false;
        }
        
        messages[tail] = block;  // shared_block copy (reference count increment)
        tail = (tail + 1) % QUEUE_CAPACITY;
        count++;
        
        pthread_mutex_unlock(&mutex);
        return true;
    }
    
    bool dequeue(shared_block& block) {
        pthread_mutex_lock(&mutex);
        
        if (count == 0) {
            pthread_mutex_unlock(&mutex);
            return false;
        }
        
        block = messages[head];
        messages[head].reset();
        head = (head + 1) % QUEUE_CAPACITY;
        count--;
        
        pthread_mutex_unlock(&mutex);
        return true;
    }
};

// Shared state for broadcast test
struct BroadcastTestState {
    // Memory pools with cascading: 64 -> 128 -> 256 -> 512 -> 1024
    uros::block_allocator allocator_64;
    uros::block_allocator allocator_128;
    uros::block_allocator allocator_256;
    uros::block_allocator allocator_512;
    uros::block_allocator allocator_1024;
    reference_counted_block_pool pool;
    
    // Consumer queues
    static const int NUM_CONSUMERS = 5;
    ConsumerQueue consumer_queues[NUM_CONSUMERS];
    
    // Statistics
    std::atomic<int> produced_count;
    std::atomic<int> broadcast_failures;
    std::atomic<int> consumed_count[NUM_CONSUMERS];
    std::atomic<int> checksum_errors[NUM_CONSUMERS];
    std::atomic<int> sequence_errors[NUM_CONSUMERS];
    
    // Control
    volatile bool stop_producer;
    volatile bool stop_consumers;
    
    BroadcastTestState()
        : allocator_64(64, 8, 20)    // Reduced from 30 to 20
        , allocator_128(128, 8, 20)  // Reduced from 25 to 20
        , allocator_256(256, 8, 20)  // Keep at 20
        , allocator_512(512, 8, 20)  // Increased from 15 to 20
        , allocator_1024(1024, 8, 20) // Increased from 10 to 20
        , pool(allocator_64)
        , produced_count(0)
        , broadcast_failures(0)
        , stop_producer(false)
        , stop_consumers(false)
    {
        // Set up cascading: 64 -> 128 -> 256 -> 512 -> 1024
        allocator_64.set_successor(allocator_128);
        allocator_128.set_successor(allocator_256);
        allocator_256.set_successor(allocator_512);
        allocator_512.set_successor(allocator_1024);
        
        for (int i = 0; i < NUM_CONSUMERS; ++i) {
            consumed_count[i] = 0;
            checksum_errors[i] = 0;
            sequence_errors[i] = 0;
        }
    }
};

//***************************************************************************
/// Helper functions
//***************************************************************************

// Simple checksum calculation
uint32_t calculate_checksum(const char* data, size_t size) {
    uint32_t sum = 0;
    for (size_t i = 0; i < size; ++i) {
        sum += static_cast<uint32_t>(data[i]);
        sum = (sum << 1) | (sum >> 31);  // Rotate left
    }
    return sum;
}

//***************************************************************************
/// Producer thread - broadcasts messages to all consumers
//***************************************************************************

struct ProducerArgs {
    BroadcastTestState* state;
    int messages_to_produce;
};

void* broadcast_producer_thread_func(void* arg) {
    ProducerArgs* args = static_cast<ProducerArgs*>(arg);
    BroadcastTestState* state = args->state;
    int target = args->messages_to_produce;
    
    printf("  [Producer] Started, target: %d messages (broadcast to %d consumers)\n", 
           target, BroadcastTestState::NUM_CONSUMERS);
    
    // Debug: Print header size
    size_t header_size = offsetof(TestMessage, payload);
    printf("  [Producer] TestMessage header size: %zu bytes\n", header_size);
    printf("  [Producer] Message sizes (header + payload + ~24B ref overhead):\n");
    printf("    4B payload:   %zu + 4   + ~24 = ~%zu -> 64B pool\n", header_size, header_size + 4 + 24);
    printf("    70B payload:  %zu + 70  + ~24 = ~%zu -> 128B pool\n", header_size, header_size + 70 + 24);
    printf("    180B payload: %zu + 180 + ~24 = ~%zu -> 256B pool\n", header_size, header_size + 180 + 24);
    printf("    400B payload: %zu + 400 + ~24 = ~%zu -> 512B pool\n", header_size, header_size + 400 + 24);
    printf("    900B payload: %zu + 900 + ~24 = ~%zu -> 1024B pool\n\n", header_size, header_size + 900 + 24);
    
    int produced = 0;
    int total_broadcasts = 0;
    int failed_broadcasts = 0;
    
    while (produced < target && !state->stop_producer) {
        // Vary message sizes to cover all pools
        // Note: Each block has ~24 bytes overhead (reference_counted_block)
        int size_category = produced % 5;
        size_t payload_size;
        
        switch (size_category) {
            case 0: payload_size = 4; break;    // Tiny:   28+4+24=56   -> 64B pool
            case 1: payload_size = 70; break;   // Small:  28+70+24=122 -> 128B pool
            case 2: payload_size = 180; break;  // Medium: 28+180+24=232 -> 256B pool
            case 3: payload_size = 400; break;  // Large:  28+400+24=452 -> 512B pool
            case 4: payload_size = 900; break;  // XLarge: 28+900+24=952 -> 1024B pool
            default: payload_size = 4; break;
        }
        
        // Allocate message with variable size (header + payload)
        size_t header_size = offsetof(TestMessage, payload);
        size_t total_size = header_size + payload_size;
        shared_block block = shared_block::create(state->pool, total_size, 8);
        
        if (!block.is_valid()) {
            printf("  [Producer] WARNING: Failed to allocate block for message %d\n", produced);
            state->broadcast_failures++;
            usleep(1000);
            continue;
        }
        
        // Fill message
        TestMessage* msg = static_cast<TestMessage*>(block.get());
        msg->producer_id = 1;
        msg->sequence = produced;
        msg->payload_size = payload_size;
        msg->timestamp = produced * 1000;
        
        // Fill payload with pattern
        for (size_t i = 0; i < payload_size; ++i) {
            msg->payload[i] = static_cast<char>((produced + i) % 256);
        }
        
        // Calculate checksum
        msg->checksum = calculate_checksum(msg->payload, payload_size);
        
        // Broadcast to all consumers
        int successful_broadcasts = 0;
        for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
            if (state->consumer_queues[i].enqueue(block)) {
                successful_broadcasts++;
                total_broadcasts++;
            } else {
                failed_broadcasts++;
                // Note: Queue full for this consumer, but continue broadcasting to others
            }
        }
        
        if (successful_broadcasts == BroadcastTestState::NUM_CONSUMERS) {
            // Successfully broadcast to all consumers
            produced++;
            state->produced_count++;
        } else if (successful_broadcasts > 0) {
            // Partial broadcast - some consumers got it, some didn't
            produced++;
            state->produced_count++;
            state->broadcast_failures++;
        } else {
            // Failed to broadcast to any consumer
            state->broadcast_failures++;
        }
        
        // Small delay between messages
        if ((produced % 10) == 0 && produced > 0) {
            usleep(2000);  // Increased from 500 to 2000 (2ms delay every 10 messages)
        } else {
            usleep(100);   // Small delay between each message
        }
    }
    
    printf("  [Producer] Finished: %d messages produced, %d total broadcasts, %d failed\n",
           produced, total_broadcasts, failed_broadcasts);
    
    return NULL;
}

//***************************************************************************
/// Fast producer thread (no delays) - for pool exhaustion test
//***************************************************************************
void* fast_producer_thread_func(void* arg) {
    ProducerArgs* args = static_cast<ProducerArgs*>(arg);
    BroadcastTestState* state = args->state;
    int target = args->messages_to_produce;
    
    printf("  [Fast Producer] Started, target: %d messages\n", target);
    
    int produced = 0;
    int total_broadcasts = 0;
    int failed_broadcasts = 0;
    int allocation_failures = 0;
    int skipped_messages = 0;
    
    while (produced < target && !state->stop_producer) {
        // Vary message sizes to cover all pools
        int size_category = produced % 5;
        size_t payload_size;
        
        switch (size_category) {
            case 0: payload_size = 4; break;
            case 1: payload_size = 70; break;
            case 2: payload_size = 180; break;
            case 3: payload_size = 400; break;
            case 4: payload_size = 900; break;
            default: payload_size = 4; break;
        }
        
        // Allocate message
        size_t header_size = offsetof(TestMessage, payload);
        size_t total_size = header_size + payload_size;
        shared_block block = shared_block::create(state->pool, total_size, 8);
        
        if (!block.is_valid()) {
            // Pool exhausted for this size - SKIP and try next size
            // This prevents large messages from blocking small messages
            allocation_failures++;
            state->broadcast_failures++;
            skipped_messages++;
            produced++;  // Move to next message immediately
            // NO DELAY - continue to next message
            continue;
        }
        
        // Fill message
        TestMessage* msg = static_cast<TestMessage*>(block.get());
        msg->producer_id = 1;
        msg->sequence = produced;
        msg->payload_size = payload_size;
        msg->timestamp = produced * 1000;
        
        // Fill payload with pattern
        for (size_t i = 0; i < payload_size; ++i) {
            msg->payload[i] = static_cast<char>((produced + i) % 256);
        }
        
        // Calculate checksum
        msg->checksum = calculate_checksum(msg->payload, payload_size);
        
        // Broadcast to all consumers (keep trying even if some queues are full)
        int successful_broadcasts = 0;
        for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
            if (state->consumer_queues[i].enqueue(block)) {
                successful_broadcasts++;
                total_broadcasts++;
            } else {
                failed_broadcasts++;
            }
        }
        
        // Always advance
        produced++;
        if (successful_broadcasts > 0) {
            state->produced_count++;  // Only count successfully broadcast messages
        }
        if (successful_broadcasts < BroadcastTestState::NUM_CONSUMERS) {
            state->broadcast_failures++;
        }
        
        // NO DELAY - produce as fast as possible
    }
    
    printf("  [Fast Producer] Finished: %d messages attempted, %d successfully produced\n",
           produced, state->produced_count.load());
    printf("                  %d total broadcasts, %d failed broadcasts\n",
           total_broadcasts, failed_broadcasts);
    printf("                  %d allocation failures, %d messages skipped\n",
           allocation_failures, skipped_messages);
    
    return NULL;
}

//***************************************************************************
/// Consumer thread - reads from its own queue
//***************************************************************************

struct ConsumerArgs {
    BroadcastTestState* state;
    int consumer_id;
};

void* consumer_thread_func(void* arg) {
    ConsumerArgs* args = static_cast<ConsumerArgs*>(arg);
    BroadcastTestState* state = args->state;
    int consumer_id = args->consumer_id;
    
    printf("  [Consumer %d] Started\n", consumer_id);
    
    int consumed = 0;
    int expected_sequence = 0;
    int empty_reads = 0;
    
    while (!state->stop_consumers) {
        shared_block block;
        
        if (state->consumer_queues[consumer_id - 1].dequeue(block)) {
            // Got a message
            TestMessage* msg = static_cast<TestMessage*>(block.get());
            
            // Verify producer ID
            assert(msg->producer_id == 1);
            
            // Check sequence (allow gaps due to queue overflow)
            if (msg->sequence < expected_sequence) {
                printf("  [Consumer %d] ERROR: Sequence out of order! Expected >= %d, got %d\n",
                       consumer_id, expected_sequence, msg->sequence);
                state->sequence_errors[consumer_id - 1]++;
            }
            expected_sequence = msg->sequence + 1;
            
            // Verify checksum
            uint32_t computed_checksum = calculate_checksum(msg->payload, msg->payload_size);
            if (computed_checksum != msg->checksum) {
                printf("  [Consumer %d] ERROR: Checksum mismatch! Seq=%d, Expected=%u, Got=%u\n",
                       consumer_id, msg->sequence, msg->checksum, computed_checksum);
                state->checksum_errors[consumer_id - 1]++;
            }
            
            // Verify payload pattern
            for (size_t i = 0; i < msg->payload_size; ++i) {
                char expected_byte = static_cast<char>((msg->sequence + i) % 256);
                if (msg->payload[i] != expected_byte) {
                    printf("  [Consumer %d] ERROR: Payload corruption at seq=%d, offset=%zu\n",
                           consumer_id, msg->sequence, i);
                    break;
                }
            }
            
            consumed++;
            state->consumed_count[consumer_id - 1]++;
            empty_reads = 0;
            
            // Simulate fast processing - consumers are faster than producer
            usleep(50);  // Reduced from 200-1000 to consistent 50us (fast processing)
            
        } else {
            // Queue empty
            empty_reads++;
            usleep(100);
            
            // Exit if producer stopped and queue empty for a while
            if (empty_reads > 50 && state->stop_producer) {
                break;
            }
        }
    }
    
    printf("  [Consumer %d] Finished: %d messages consumed\n", consumer_id, consumed);
    
    return NULL;
}

//***************************************************************************
/// Slow consumer thread - for pool exhaustion test
//***************************************************************************
void* slow_consumer_thread_func(void* arg) {
    ConsumerArgs* args = static_cast<ConsumerArgs*>(arg);
    BroadcastTestState* state = args->state;
    int consumer_id = args->consumer_id;
    
    printf("  [Slow Consumer %d] Started\n", consumer_id);
    
    int consumed = 0;
    int expected_sequence = 0;
    int empty_reads = 0;
    
    while (!state->stop_consumers) {
        shared_block block;
        
        if (state->consumer_queues[consumer_id - 1].dequeue(block)) {
            // Got a message
            TestMessage* msg = static_cast<TestMessage*>(block.get());
            
            // Verify producer ID
            assert(msg->producer_id == 1);
            
            // Check sequence (allow gaps due to queue overflow)
            if (msg->sequence < expected_sequence) {
                printf("  [Slow Consumer %d] ERROR: Sequence out of order! Expected >= %d, got %d\n",
                       consumer_id, expected_sequence, msg->sequence);
                state->sequence_errors[consumer_id - 1]++;
            }
            expected_sequence = msg->sequence + 1;
            
            // Verify checksum
            uint32_t computed_checksum = calculate_checksum(msg->payload, msg->payload_size);
            if (computed_checksum != msg->checksum) {
                printf("  [Slow Consumer %d] ERROR: Checksum mismatch! Seq=%d, Expected=%u, Got=%u\n",
                       consumer_id, msg->sequence, msg->checksum, computed_checksum);
                state->checksum_errors[consumer_id - 1]++;
            }
            
            // Verify payload pattern
            for (size_t i = 0; i < msg->payload_size; ++i) {
                char expected_byte = static_cast<char>((msg->sequence + i) % 256);
                if (msg->payload[i] != expected_byte) {
                    printf("  [Slow Consumer %d] ERROR: Payload corruption at seq=%d, offset=%zu\n",
                           consumer_id, msg->sequence, i);
                    break;
                }
            }
            
            consumed++;
            state->consumed_count[consumer_id - 1]++;
            empty_reads = 0;
            
            // VERY SLOW processing - 20ms per message (increased from 10ms)
            // This ensures memory pool exhaustion before queue overflow
            usleep(20000);
            
        } else {
            // Queue empty
            empty_reads++;
            usleep(100);
            
            // Exit if producer stopped and queue empty for a while
            if (empty_reads > 50 && state->stop_producer) {
                break;
            }
        }
    }
    
    printf("  [Slow Consumer %d] Finished: %d messages consumed\n", consumer_id, consumed);
    
    return NULL;
}

//***************************************************************************
/// Main broadcast test
//***************************************************************************
void test_broadcast_one_producer_multiple_consumers()
{
    printf("\n========================================\n");
    printf("Test: One producer broadcasting to multiple consumers\n");
    printf("========================================\n\n");
    
    BroadcastTestState state;
    
    printf("→ System initialized\n");
    printf("  • Memory pools (cascaded): 64B(20) -> 128B(20) -> 256B(20) -> 512B(20) -> 1024B(20)\n");
    printf("  • Consumers: %d (each with independent queue)\n", BroadcastTestState::NUM_CONSUMERS);
    printf("  • Queue capacity per consumer: %d messages\n", ConsumerQueue::QUEUE_CAPACITY);
    printf("  • Producer speed: ~300us/msg ≈ 3,333 msg/sec\n");
    printf("  • Consumer speed: 50us/msg × 5 consumers ≈ 100,000 msg/sec total\n");
    printf("  • Message sizes: 4B(→64B), 70B(→128B), 180B(→256B), 400B(→512B), 900B(→1024B)\n\n");
    
    const int MESSAGES_TO_PRODUCE = 100;
    
    pthread_t producer_thread;
    pthread_t consumer_threads[BroadcastTestState::NUM_CONSUMERS];
    ProducerArgs producer_args;
    ConsumerArgs consumer_args[BroadcastTestState::NUM_CONSUMERS];
    
    // Start consumers first
    printf("→ Starting %d consumer threads\n", BroadcastTestState::NUM_CONSUMERS);
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        consumer_args[i].state = &state;
        consumer_args[i].consumer_id = i + 1;
        pthread_create(&consumer_threads[i], NULL, consumer_thread_func, &consumer_args[i]);
    }
    
    // Start producer
    printf("→ Starting producer thread (target: %d messages)\n\n", MESSAGES_TO_PRODUCE);
    producer_args.state = &state;
    producer_args.messages_to_produce = MESSAGES_TO_PRODUCE;
    pthread_create(&producer_thread, NULL, broadcast_producer_thread_func, &producer_args);
    
    // Wait for producer to finish
    pthread_join(producer_thread, NULL);
    state.stop_producer = true;
    
    printf("\n→ Producer finished, waiting for consumers to drain queues\n");
    
    // Wait for consumers to finish
    usleep(100000);  // Give time for consumers to process remaining messages
    state.stop_consumers = true;
    
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        pthread_join(consumer_threads[i], NULL);
    }
    
    // Print statistics
    printf("\n→ Final Statistics\n");
    printf("  Production:\n");
    printf("    Messages produced: %d (target: %d)\n", 
           state.produced_count.load(), MESSAGES_TO_PRODUCE);
    printf("    Broadcast failures: %d\n", state.broadcast_failures.load());
    
    printf("\n  Consumption:\n");
    int total_consumed = 0;
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        int consumed = state.consumed_count[i].load();
        total_consumed += consumed;
        double delivery_rate = (state.produced_count.load() > 0) ?
            (100.0 * consumed / state.produced_count.load()) : 0.0;
        
        printf("    Consumer %d: %d messages (%.1f%% delivery rate)\n",
               i + 1, consumed, delivery_rate);
    }
    
    printf("\n  Data Integrity:\n");
    bool all_valid = true;
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        int checksum_errs = state.checksum_errors[i].load();
        int sequence_errs = state.sequence_errors[i].load();
        
        if (checksum_errs > 0 || sequence_errs > 0) {
            printf("    Consumer %d: ✗ Checksum errors: %d, Sequence errors: %d\n",
                   i + 1, checksum_errs, sequence_errs);
            all_valid = false;
        } else {
            printf("    Consumer %d: ✓ No errors\n", i + 1);
        }
    }
    
    printf("\n  Memory pools:\n");
    printf("    64B:   current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_64.get_allocated_count(),
           state.allocator_64.get_capacity(),
           state.allocator_64.get_peak_allocated_count(),
           100.0 * state.allocator_64.get_peak_allocated_count() / state.allocator_64.get_capacity());
    printf("    128B:  current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_128.get_allocated_count(),
           state.allocator_128.get_capacity(),
           state.allocator_128.get_peak_allocated_count(),
           100.0 * state.allocator_128.get_peak_allocated_count() / state.allocator_128.get_capacity());
    printf("    256B:  current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_256.get_allocated_count(),
           state.allocator_256.get_capacity(),
           state.allocator_256.get_peak_allocated_count(),
           100.0 * state.allocator_256.get_peak_allocated_count() / state.allocator_256.get_capacity());
    printf("    512B:  current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_512.get_allocated_count(),
           state.allocator_512.get_capacity(),
           state.allocator_512.get_peak_allocated_count(),
           100.0 * state.allocator_512.get_peak_allocated_count() / state.allocator_512.get_capacity());
    printf("    1024B: current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_1024.get_allocated_count(),
           state.allocator_1024.get_capacity(),
           state.allocator_1024.get_peak_allocated_count(),
           100.0 * state.allocator_1024.get_peak_allocated_count() / state.allocator_1024.get_capacity());
    
    // Calculate average reference count at peak
    size_t total_peak = state.allocator_64.get_peak_allocated_count() +
                        state.allocator_128.get_peak_allocated_count() +
                        state.allocator_256.get_peak_allocated_count() +
                        state.allocator_512.get_peak_allocated_count() +
                        state.allocator_1024.get_peak_allocated_count();
    printf("\n  Reference counting efficiency:\n");
    printf("    Total peak blocks in use: %zu\n", total_peak);
    printf("    Each block shared among %d consumers (avg ref count ~%d)\n",
           BroadcastTestState::NUM_CONSUMERS, BroadcastTestState::NUM_CONSUMERS);
    
    // Cleanup - drain all queues
    printf("\n→ Cleaning up\n");
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        shared_block block;
        int drained = 0;
        while (state.consumer_queues[i].dequeue(block)) {
            drained++;
        }
        if (drained > 0) {
            printf("  Consumer %d: drained %d remaining messages\n", i + 1, drained);
        }
    }
    
    usleep(10000);  // Wait for cleanup
    
    printf("\n→ Verifying memory cleanup\n");
    printf("  64B pool: %zu allocated\n", state.allocator_64.get_allocated_count());
    printf("  128B pool: %zu allocated\n", state.allocator_128.get_allocated_count());
    printf("  256B pool: %zu allocated\n", state.allocator_256.get_allocated_count());
    printf("  512B pool: %zu allocated\n", state.allocator_512.get_allocated_count());
    printf("  1024B pool: %zu allocated\n", state.allocator_1024.get_allocated_count());
    
    // Assertions
    assert(state.allocator_64.get_allocated_count() == 0);
    assert(state.allocator_128.get_allocated_count() == 0);
    assert(state.allocator_256.get_allocated_count() == 0);
    assert(state.allocator_512.get_allocated_count() == 0);
    assert(state.allocator_1024.get_allocated_count() == 0);
    assert(state.produced_count.load() > 0);
    
    // All consumers should have received messages
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        if (state.consumed_count[i].load() == 0) {
            printf("\n✗ FAILED: Consumer %d received no messages!\n", i + 1);
            exit(1);  // Force failure
        }
    }
    
    // No data integrity errors
    if (!all_valid) {
        printf("\n✗ FAILED: Data integrity errors detected!\n");
        exit(1);  // Force failure
    }
    
    // ALL cascaded pools MUST be used (peak > 0)
    printf("\n→ Verifying all pools were utilized\n");
    bool all_pools_used = true;
    
    if (state.allocator_64.get_peak_allocated_count() == 0) {
        printf("  ✗ 64B pool: NOT USED (peak=0)\n");
        all_pools_used = false;
    } else {
        printf("  ✓ 64B pool: used (peak=%zu)\n", state.allocator_64.get_peak_allocated_count());
    }
    
    if (state.allocator_128.get_peak_allocated_count() == 0) {
        printf("  ✗ 128B pool: NOT USED (peak=0)\n");
        all_pools_used = false;
    } else {
        printf("  ✓ 128B pool: used (peak=%zu)\n", state.allocator_128.get_peak_allocated_count());
    }
    
    if (state.allocator_256.get_peak_allocated_count() == 0) {
        printf("  ✗ 256B pool: NOT USED (peak=0)\n");
        all_pools_used = false;
    } else {
        printf("  ✓ 256B pool: used (peak=%zu)\n", state.allocator_256.get_peak_allocated_count());
    }
    
    if (state.allocator_512.get_peak_allocated_count() == 0) {
        printf("  ✗ 512B pool: NOT USED (peak=0)\n");
        all_pools_used = false;
    } else {
        printf("  ✓ 512B pool: used (peak=%zu)\n", state.allocator_512.get_peak_allocated_count());
    }
    
    if (state.allocator_1024.get_peak_allocated_count() == 0) {
        printf("  ✗ 1024B pool: NOT USED (peak=0)\n");
        all_pools_used = false;
    } else {
        printf("  ✓ 1024B pool: used (peak=%zu)\n", state.allocator_1024.get_peak_allocated_count());
    }
    
    if (!all_pools_used) {
        printf("\n✗ FAILED: Not all cascaded pools were utilized!\n");
        printf("  This test is designed to exercise all pool sizes.\n");
        printf("  If a pool is not used, the message sizes need adjustment.\n");
        exit(1);  // Force failure (assert may be disabled in Release builds)
    }
    
    printf("\n✓ All memory properly released\n");
    printf("✓ All data integrity checks passed\n");
    printf("✓ All cascaded pools utilized\n");
    printf("\n========================================\n");
    printf("Broadcast test completed successfully! ✓\n");
    printf("========================================\n");
}

//***************************************************************************
/// Slow consumer broadcast test - tests pool exhaustion with data integrity
//***************************************************************************
void test_broadcast_slow_consumers()
{
    printf("\n========================================\n");
    printf("Test: Broadcast with SLOW consumers (pool exhaustion scenario)\n");
    printf("========================================\n\n");
    
    BroadcastTestState state;
    
    printf("→ System initialized\n");
    printf("  • Memory pools (cascaded): 64B(20) -> 128B(20) -> 256B(20) -> 512B(20) -> 1024B(20)\n");
    printf("  • Consumers: %d (each with independent queue)\n", BroadcastTestState::NUM_CONSUMERS);
    printf("  • Queue capacity per consumer: %d messages\n", ConsumerQueue::QUEUE_CAPACITY);
    printf("  • Producer speed: FAST (no delays) ≈ 50,000+ msg/sec\n");
    printf("  • Consumer speed: VERY SLOW (20ms/msg) × 5 consumers ≈ 250 msg/sec total\n");
    printf("  • Total pool capacity: 100 blocks (20 each size), Queue capacity: 5×100=500 slots\n");
    printf("  • With reference counting: 100 blocks can fill all 500 queue slots\n");
    printf("  • EXPECTED: 100%% pool utilization + message drops + NO data integrity errors\n\n");
    
    const int MESSAGES_TO_PRODUCE = 1000;  // Many messages to fully exhaust all pools
    
    pthread_t producer_thread;
    pthread_t consumer_threads[BroadcastTestState::NUM_CONSUMERS];
    ProducerArgs producer_args;
    ConsumerArgs consumer_args[BroadcastTestState::NUM_CONSUMERS];
    
    // Start consumers first
    printf("→ Starting %d SLOW consumer threads\n", BroadcastTestState::NUM_CONSUMERS);
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        consumer_args[i].state = &state;
        consumer_args[i].consumer_id = i + 1;
        pthread_create(&consumer_threads[i], NULL, slow_consumer_thread_func, &consumer_args[i]);
    }
    
    // Start fast producer
    printf("→ Starting FAST producer thread (target: %d messages)\n\n", MESSAGES_TO_PRODUCE);
    producer_args.state = &state;
    producer_args.messages_to_produce = MESSAGES_TO_PRODUCE;
    pthread_create(&producer_thread, NULL, fast_producer_thread_func, &producer_args);
    
    // Wait for producer to finish
    pthread_join(producer_thread, NULL);
    state.stop_producer = true;
    
    printf("\n→ Producer finished, waiting for consumers to drain queues\n");
    
    // Wait for consumers to finish
    usleep(500000);  // Give more time for slow consumers
    state.stop_consumers = true;
    
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        pthread_join(consumer_threads[i], NULL);
    }
    
    // Print statistics
    printf("\n→ Final Statistics\n");
    printf("  Production:\n");
    printf("    Messages produced: %d (target: %d)\n", 
           state.produced_count.load(), MESSAGES_TO_PRODUCE);
    printf("    Broadcast failures: %d\n", state.broadcast_failures.load());
    
    printf("\n  Consumption:\n");
    int total_consumed = 0;
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        int consumed = state.consumed_count[i].load();
        total_consumed += consumed;
        double delivery_rate = (state.produced_count.load() > 0) ?
            (100.0 * consumed / state.produced_count.load()) : 0.0;
        
        printf("    Consumer %d: %d messages (%.1f%% delivery rate)\n",
               i + 1, consumed, delivery_rate);
    }
    
    double avg_delivery = (state.produced_count.load() > 0) ?
        (100.0 * total_consumed / (state.produced_count.load() * BroadcastTestState::NUM_CONSUMERS)) : 0.0;
    printf("    Average delivery rate: %.1f%%\n", avg_delivery);
    
    printf("\n  Data Integrity:\n");
    bool all_valid = true;
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        int checksum_errs = state.checksum_errors[i].load();
        int sequence_errs = state.sequence_errors[i].load();
        
        if (checksum_errs > 0 || sequence_errs > 0) {
            printf("    Consumer %d: ✗ Checksum errors: %d, Sequence errors: %d\n",
                   i + 1, checksum_errs, sequence_errs);
            all_valid = false;
        } else {
            printf("    Consumer %d: ✓ No errors\n", i + 1);
        }
    }
    
    printf("\n  Memory pools:\n");
    printf("    64B:   current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_64.get_allocated_count(),
           state.allocator_64.get_capacity(),
           state.allocator_64.get_peak_allocated_count(),
           100.0 * state.allocator_64.get_peak_allocated_count() / state.allocator_64.get_capacity());
    printf("    128B:  current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_128.get_allocated_count(),
           state.allocator_128.get_capacity(),
           state.allocator_128.get_peak_allocated_count(),
           100.0 * state.allocator_128.get_peak_allocated_count() / state.allocator_128.get_capacity());
    printf("    256B:  current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_256.get_allocated_count(),
           state.allocator_256.get_capacity(),
           state.allocator_256.get_peak_allocated_count(),
           100.0 * state.allocator_256.get_peak_allocated_count() / state.allocator_256.get_capacity());
    printf("    512B:  current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_512.get_allocated_count(),
           state.allocator_512.get_capacity(),
           state.allocator_512.get_peak_allocated_count(),
           100.0 * state.allocator_512.get_peak_allocated_count() / state.allocator_512.get_capacity());
    printf("    1024B: current=%zu/%zu, peak=%zu (%.0f%% peak utilization)\n",
           state.allocator_1024.get_allocated_count(),
           state.allocator_1024.get_capacity(),
           state.allocator_1024.get_peak_allocated_count(),
           100.0 * state.allocator_1024.get_peak_allocated_count() / state.allocator_1024.get_capacity());
    
    // Calculate total
    size_t total_peak = state.allocator_64.get_peak_allocated_count() +
                        state.allocator_128.get_peak_allocated_count() +
                        state.allocator_256.get_peak_allocated_count() +
                        state.allocator_512.get_peak_allocated_count() +
                        state.allocator_1024.get_peak_allocated_count();
    size_t total_capacity = state.allocator_64.get_capacity() + 
                            state.allocator_128.get_capacity() + 
                            state.allocator_256.get_capacity() + 
                            state.allocator_512.get_capacity() + 
                            state.allocator_1024.get_capacity();
    
    printf("\n  Pool exhaustion:\n");
    printf("    Total peak blocks in use: %zu / %zu\n", total_peak, total_capacity);
    
    // Cleanup
    printf("\n→ Cleaning up\n");
    for (int i = 0; i < BroadcastTestState::NUM_CONSUMERS; ++i) {
        shared_block block;
        int drained = 0;
        while (state.consumer_queues[i].dequeue(block)) {
            drained++;
        }
        if (drained > 0) {
            printf("  Consumer %d: drained %d remaining messages\n", i + 1, drained);
        }
    }
    
    usleep(10000);
    
    printf("\n→ Verifying memory cleanup\n");
    printf("  All pools: %zu blocks remaining\n", 
           state.allocator_64.get_allocated_count() + 
           state.allocator_128.get_allocated_count() + 
           state.allocator_256.get_allocated_count() + 
           state.allocator_512.get_allocated_count() + 
           state.allocator_1024.get_allocated_count());
    
    // Assertions for slow consumer scenario
    assert(state.allocator_64.get_allocated_count() == 0);
    assert(state.allocator_128.get_allocated_count() == 0);
    assert(state.allocator_256.get_allocated_count() == 0);
    assert(state.allocator_512.get_allocated_count() == 0);
    assert(state.allocator_1024.get_allocated_count() == 0);
    
    // Data integrity MUST be preserved
    if (!all_valid) {
        printf("\n✗ FAILED: Data integrity errors detected!\n");
        exit(1);
    }
    
    // ALL pools MUST reach 100% utilization
    printf("\n→ Verifying pool exhaustion (ALL pools should hit 100%%)\n");
    bool all_pools_exhausted = true;
    
    if (state.allocator_64.get_peak_allocated_count() < state.allocator_64.get_capacity()) {
        printf("  ✗ 64B pool: Only %.0f%% utilized (expected 100%%)\n",
               100.0 * state.allocator_64.get_peak_allocated_count() / state.allocator_64.get_capacity());
        all_pools_exhausted = false;
    } else {
        printf("  ✓ 64B pool: 100%% exhausted\n");
    }
    
    if (state.allocator_128.get_peak_allocated_count() < state.allocator_128.get_capacity()) {
        printf("  ✗ 128B pool: Only %.0f%% utilized (expected 100%%)\n",
               100.0 * state.allocator_128.get_peak_allocated_count() / state.allocator_128.get_capacity());
        all_pools_exhausted = false;
    } else {
        printf("  ✓ 128B pool: 100%% exhausted\n");
    }
    
    if (state.allocator_256.get_peak_allocated_count() < state.allocator_256.get_capacity()) {
        printf("  ✗ 256B pool: Only %.0f%% utilized (expected 100%%)\n",
               100.0 * state.allocator_256.get_peak_allocated_count() / state.allocator_256.get_capacity());
        all_pools_exhausted = false;
    } else {
        printf("  ✓ 256B pool: 100%% exhausted\n");
    }
    
    if (state.allocator_512.get_peak_allocated_count() < state.allocator_512.get_capacity()) {
        printf("  ✗ 512B pool: Only %.0f%% utilized (expected 100%%)\n",
               100.0 * state.allocator_512.get_peak_allocated_count() / state.allocator_512.get_capacity());
        all_pools_exhausted = false;
    } else {
        printf("  ✓ 512B pool: 100%% exhausted\n");
    }
    
    if (state.allocator_1024.get_peak_allocated_count() < state.allocator_1024.get_capacity()) {
        printf("  ✗ 1024B pool: Only %.0f%% utilized (expected 100%%)\n",
               100.0 * state.allocator_1024.get_peak_allocated_count() / state.allocator_1024.get_capacity());
        all_pools_exhausted = false;
    } else {
        printf("  ✓ 1024B pool: 100%% exhausted\n");
    }
    
    if (!all_pools_exhausted) {
        printf("\n✗ FAILED: Not all pools reached 100%% utilization!\n");
        printf("  In slow consumer scenario, all pools should be exhausted.\n");
        printf("  Producer may need to be faster or consumers slower.\n");
        exit(1);
    }
    
    // Should have message drops
    if (avg_delivery >= 99.0) {
        printf("\n✗ FAILED: No message drops detected (%.1f%% delivery)!\n", avg_delivery);
        printf("  In slow consumer scenario, there should be drops due to queue overflow.\n");
        exit(1);
    }
    
    printf("\n✓ All memory properly released\n");
    printf("✓ All data integrity checks passed\n");
    printf("✓ All pools exhausted (100%% utilization)\n");
    printf("✓ Message drops detected (%.1f%% delivery rate)\n", avg_delivery);
    printf("\n========================================\n");
    printf("Slow consumer test completed successfully! ✓\n");
    printf("========================================\n");
}

//***************************************************************************
/// Main
//***************************************************************************
int main()
{
    // Test 1: Normal speed - all pools used, no drops
    test_broadcast_one_producer_multiple_consumers();
    
    printf("\n\n\n");
    printf("================================================================================\n");
    printf("================================================================================\n");
    printf("\n\n\n");
    
    // Test 2: Slow consumers - all pools exhausted, drops expected
    test_broadcast_slow_consumers();
    
    return 0;
}
