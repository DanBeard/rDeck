/**
 * Unit tests for the thread-safe action queue pattern used by RnsService.
 *
 * Tests verify:
 * 1. Actions are queued and executed correctly
 * 2. Actions execute in FIFO order
 * 3. Queue drain uses swap (queue is empty after drain)
 * 4. Concurrent pushes from multiple threads are safe
 * 5. Actions queued during drain are not lost
 * 6. Rate-limited drain processes at most N actions per tick
 * 7. Exception in one action doesn't prevent others from running
 */

#include <unity.h>
#include <functional>
#include <vector>
#include <thread>
#include <atomic>
#include <chrono>
#include <iterator>
#include <stdexcept>
#include "retOS/PlatformMutex.h"

// Replicate the exact queue + drain pattern from RnsService
// so we're testing the real mechanism, not a simplified version.
class ActionQueue {
public:
    static const size_t MAX_ACTIONS_PER_DRAIN = 3;

    void queueAction(std::function<void()> action) {
        PlatformMutexGuard guard(_action_mutex);
        _pending_actions.push_back(std::move(action));
    }

    // Unlimited drain (original pattern, used in some tests)
    void drain() {
        std::vector<std::function<void()>> actions;
        {
            PlatformMutexGuard guard(_action_mutex);
            actions.swap(_pending_actions);
        }
        for (auto& action : actions) {
            action();
        }
    }

    // Rate-limited drain identical to RnsService::tick()
    void drainLimited() {
        std::vector<std::function<void()>> actions;
        {
            PlatformMutexGuard guard(_action_mutex);
            if (_pending_actions.size() <= MAX_ACTIONS_PER_DRAIN) {
                actions.swap(_pending_actions);
            } else {
                actions.assign(
                    std::make_move_iterator(_pending_actions.begin()),
                    std::make_move_iterator(_pending_actions.begin() + MAX_ACTIONS_PER_DRAIN)
                );
                _pending_actions.erase(_pending_actions.begin(),
                                       _pending_actions.begin() + MAX_ACTIONS_PER_DRAIN);
            }
        }
        for (auto& action : actions) {
            try {
                action();
            } catch (const std::exception& e) {
                // Log but don't propagate - matches RnsService behavior
            }
        }
    }

    size_t pendingCount() {
        PlatformMutexGuard guard(_action_mutex);
        return _pending_actions.size();
    }

private:
    PlatformMutex _action_mutex;
    std::vector<std::function<void()>> _pending_actions;
};

// Use a pointer so we can recreate between tests
static ActionQueue* queue = nullptr;

void setUp(void) {
    delete queue;
    queue = new ActionQueue();
}

void tearDown(void) {
    delete queue;
    queue = nullptr;
}

// ============================================================================
// Basic functionality
// ============================================================================

void test_empty_drain_does_nothing(void) {
    // Draining an empty queue should not crash or have side effects
    queue->drain();
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_single_action_executes(void) {
    bool executed = false;
    queue->queueAction([&executed]() { executed = true; });

    TEST_ASSERT_FALSE(executed);
    TEST_ASSERT_EQUAL(1, queue->pendingCount());

    queue->drain();

    TEST_ASSERT_TRUE(executed);
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_multiple_actions_execute_in_order(void) {
    std::vector<int> order;

    queue->queueAction([&order]() { order.push_back(1); });
    queue->queueAction([&order]() { order.push_back(2); });
    queue->queueAction([&order]() { order.push_back(3); });

    TEST_ASSERT_EQUAL(3, queue->pendingCount());

    queue->drain();

    TEST_ASSERT_EQUAL(3, order.size());
    TEST_ASSERT_EQUAL(1, order[0]);
    TEST_ASSERT_EQUAL(2, order[1]);
    TEST_ASSERT_EQUAL(3, order[2]);
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_drain_clears_queue(void) {
    int count = 0;
    queue->queueAction([&count]() { count++; });
    queue->queueAction([&count]() { count++; });

    queue->drain();
    TEST_ASSERT_EQUAL(2, count);

    // Second drain should do nothing
    queue->drain();
    TEST_ASSERT_EQUAL(2, count);
}

void test_actions_queued_during_drain_survive(void) {
    // An action that queues another action during execution
    int count = 0;
    ActionQueue* q = queue;  // capture raw pointer for inner lambda
    queue->queueAction([&count, q]() {
        count++;
        // Queue a new action while draining
        q->queueAction([&count]() { count += 10; });
    });

    queue->drain();
    // First action ran, second was queued but not yet drained
    TEST_ASSERT_EQUAL(1, count);
    TEST_ASSERT_EQUAL(1, queue->pendingCount());

    // Second drain picks up the newly queued action
    queue->drain();
    TEST_ASSERT_EQUAL(11, count);
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_action_captures_values_by_copy(void) {
    // Simulate the pattern used in Maps.cpp / WebSearch.cpp
    // where we copy values into the lambda to avoid dangling references
    int result = 0;
    {
        int localValue = 42;
        queue->queueAction([localValue, &result]() {
            result = localValue;
        });
        localValue = 999;  // Modify after queuing - should not affect lambda
    }

    queue->drain();
    TEST_ASSERT_EQUAL(42, result);
}

void test_action_with_string_capture(void) {
    // Simulate the pattern in WebSearch/Maps where query strings are captured
    std::string result;
    {
        std::string query = "test search";
        std::string queryCopy = query;  // Copy like our code does
        queue->queueAction([queryCopy, &result]() {
            result = queryCopy;
        });
        query = "modified";  // Original changed - copy should be unaffected
    }

    queue->drain();
    TEST_ASSERT_EQUAL_STRING("test search", result.c_str());
}

// ============================================================================
// Rate-limited drain tests
// ============================================================================

void test_rate_limited_drain_processes_at_most_n(void) {
    int count = 0;
    // Queue more actions than the limit
    for (int i = 0; i < 9; i++) {
        queue->queueAction([&count]() { count++; });
    }

    TEST_ASSERT_EQUAL(9, queue->pendingCount());

    // First drain: should process MAX_ACTIONS_PER_DRAIN (3)
    queue->drainLimited();
    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(6, queue->pendingCount());

    // Second drain: 3 more
    queue->drainLimited();
    TEST_ASSERT_EQUAL(6, count);
    TEST_ASSERT_EQUAL(3, queue->pendingCount());

    // Third drain: remaining 3
    queue->drainLimited();
    TEST_ASSERT_EQUAL(9, count);
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_rate_limited_drain_preserves_order(void) {
    std::vector<int> order;
    for (int i = 0; i < 7; i++) {
        int val = i;
        queue->queueAction([val, &order]() { order.push_back(val); });
    }

    // Drain in batches of 3
    queue->drainLimited();  // 0, 1, 2
    queue->drainLimited();  // 3, 4, 5
    queue->drainLimited();  // 6

    TEST_ASSERT_EQUAL(7, order.size());
    for (int i = 0; i < 7; i++) {
        TEST_ASSERT_EQUAL(i, order[i]);
    }
}

void test_rate_limited_drain_fewer_than_limit(void) {
    int count = 0;
    queue->queueAction([&count]() { count++; });
    queue->queueAction([&count]() { count++; });

    // Only 2 actions queued, less than limit of 3
    queue->drainLimited();
    TEST_ASSERT_EQUAL(2, count);
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_rate_limited_empty_drain(void) {
    queue->drainLimited();
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_rate_limited_exactly_at_limit(void) {
    int count = 0;
    for (size_t i = 0; i < ActionQueue::MAX_ACTIONS_PER_DRAIN; i++) {
        queue->queueAction([&count]() { count++; });
    }

    queue->drainLimited();
    TEST_ASSERT_EQUAL((int)ActionQueue::MAX_ACTIONS_PER_DRAIN, count);
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

// ============================================================================
// Exception safety tests
// ============================================================================

void test_exception_in_action_does_not_crash(void) {
    // drainLimited catches exceptions, so a throwing action
    // should not prevent subsequent actions from running
    int count = 0;
    queue->queueAction([&count]() { count++; });
    queue->queueAction([]() { throw std::runtime_error("test error"); });
    queue->queueAction([&count]() { count += 10; });

    queue->drainLimited();  // All 3 within limit

    // First action ran, second threw (caught), third ran
    TEST_ASSERT_EQUAL(11, count);
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_exception_in_batch_doesnt_lose_remaining(void) {
    // Queue 6 actions, one in the first batch throws
    int count = 0;
    queue->queueAction([&count]() { count++; });
    queue->queueAction([]() { throw std::runtime_error("boom"); });
    queue->queueAction([&count]() { count++; });
    queue->queueAction([&count]() { count += 10; });
    queue->queueAction([&count]() { count += 10; });
    queue->queueAction([&count]() { count += 10; });

    // First batch: 3 actions (one throws)
    queue->drainLimited();
    TEST_ASSERT_EQUAL(2, count);  // 1 + 0(threw) + 1
    TEST_ASSERT_EQUAL(3, queue->pendingCount());

    // Second batch: remaining 3
    queue->drainLimited();
    TEST_ASSERT_EQUAL(32, count);  // 2 + 10 + 10 + 10
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

// ============================================================================
// Simulated rapid-fire tile request pattern
// ============================================================================

void test_simulated_tile_burst(void) {
    // Simulate Maps::requestVisibleTiles() queuing 9 tile requests,
    // then services thread draining them across multiple ticks
    std::vector<std::pair<int, int>> requested_tiles;
    int ticks = 0;

    // Simulate UI thread queuing 9 tile requests at once (3x3 grid)
    for (int dy = -1; dy <= 1; dy++) {
        for (int dx = -1; dx <= 1; dx++) {
            int x = dx, y = dy;
            queue->queueAction([x, y, &requested_tiles]() {
                requested_tiles.push_back({x, y});
            });
        }
    }

    TEST_ASSERT_EQUAL(9, queue->pendingCount());

    // Simulate services thread tick loop with rate-limited drain
    while (queue->pendingCount() > 0) {
        queue->drainLimited();
        ticks++;
    }

    // All 9 tiles should have been requested
    TEST_ASSERT_EQUAL(9, requested_tiles.size());
    // Should have taken ceil(9/3) = 3 ticks
    TEST_ASSERT_EQUAL(3, ticks);
}

void test_interleaved_queue_and_drain(void) {
    // Simulate: UI queues burst, services drains some, UI queues more
    int count = 0;

    // Initial burst (simulates Maps startup)
    for (int i = 0; i < 5; i++) {
        queue->queueAction([&count]() { count++; });
    }

    // First tick: drain 3
    queue->drainLimited();
    TEST_ASSERT_EQUAL(3, count);
    TEST_ASSERT_EQUAL(2, queue->pendingCount());

    // UI queues 2 more (simulates user panning map)
    queue->queueAction([&count]() { count += 10; });
    queue->queueAction([&count]() { count += 10; });

    // Second tick: drain 3 (2 remaining + first new one? No, queue is FIFO)
    queue->drainLimited();
    // Should drain the 2 remaining original + 1 new
    TEST_ASSERT_EQUAL(5 + 10, count);  // 3 + 1 + 1 + 10
    TEST_ASSERT_EQUAL(1, queue->pendingCount());

    // Third tick: drain the last one
    queue->drainLimited();
    TEST_ASSERT_EQUAL(25, count);
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

// ============================================================================
// Thread safety tests
// ============================================================================

void test_concurrent_queue_from_multiple_threads(void) {
    std::atomic<int> counter{0};
    const int THREADS = 4;
    const int ACTIONS_PER_THREAD = 100;
    ActionQueue* q = queue;

    std::vector<std::thread> threads;
    for (int t = 0; t < THREADS; t++) {
        threads.emplace_back([&counter, q, ACTIONS_PER_THREAD]() {
            for (int i = 0; i < ACTIONS_PER_THREAD; i++) {
                q->queueAction([&counter]() {
                    counter.fetch_add(1, std::memory_order_relaxed);
                });
            }
        });
    }

    // Wait for all threads to finish queuing
    for (auto& t : threads) {
        t.join();
    }

    TEST_ASSERT_EQUAL(THREADS * ACTIONS_PER_THREAD, queue->pendingCount());

    // Drain all actions (single-threaded, like tick())
    queue->drain();

    TEST_ASSERT_EQUAL(THREADS * ACTIONS_PER_THREAD, counter.load());
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_concurrent_queue_while_draining(void) {
    // Simulate the real scenario: services thread drains while UI thread queues
    std::atomic<int> drained{0};
    std::atomic<bool> stop_producer{false};
    std::atomic<int> produced{0};
    ActionQueue* q = queue;

    const int DRAIN_ROUNDS = 50;

    // Producer thread (simulates UI thread)
    std::thread producer([&drained, &stop_producer, &produced, q]() {
        while (!stop_producer.load(std::memory_order_acquire)) {
            q->queueAction([&drained]() {
                drained.fetch_add(1, std::memory_order_relaxed);
            });
            produced.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    // Consumer (simulates services thread tick() loop)
    for (int i = 0; i < DRAIN_ROUNDS; i++) {
        queue->drain();
        std::this_thread::sleep_for(std::chrono::microseconds(100));
    }

    stop_producer.store(true, std::memory_order_release);
    producer.join();

    // Final drain to get any remaining actions
    queue->drain();

    // All produced actions should have been drained
    TEST_ASSERT_EQUAL(produced.load(), drained.load());
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_concurrent_queue_with_rate_limited_drain(void) {
    // Like the above but using drainLimited - verifies nothing lost with rate limiting
    std::atomic<int> drained{0};
    std::atomic<bool> stop_producer{false};
    std::atomic<int> produced{0};
    ActionQueue* q = queue;

    const int DRAIN_ROUNDS = 100;

    // Producer thread (simulates UI thread queuing tile requests)
    std::thread producer([&drained, &stop_producer, &produced, q]() {
        while (!stop_producer.load(std::memory_order_acquire)) {
            q->queueAction([&drained]() {
                drained.fetch_add(1, std::memory_order_relaxed);
            });
            produced.fetch_add(1, std::memory_order_relaxed);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
        }
    });

    // Consumer (simulates services thread with rate-limited tick)
    for (int i = 0; i < DRAIN_ROUNDS; i++) {
        queue->drainLimited();
        std::this_thread::sleep_for(std::chrono::microseconds(50));
    }

    stop_producer.store(true, std::memory_order_release);
    producer.join();

    // Drain remaining with unlimited drain to finish
    queue->drain();

    // All produced actions should have been drained
    TEST_ASSERT_EQUAL(produced.load(), drained.load());
    TEST_ASSERT_EQUAL(0, queue->pendingCount());
}

void test_no_actions_lost_under_contention(void) {
    // Stress test: many producers, periodic drains, verify nothing lost
    std::atomic<int> total_queued{0};
    std::atomic<int> total_executed{0};
    std::atomic<bool> stop{false};
    ActionQueue* q = queue;

    const int PRODUCER_THREADS = 4;
    const int ACTIONS_PER_PRODUCER = 200;

    // Producers queue known number of actions
    std::vector<std::thread> producers;
    for (int t = 0; t < PRODUCER_THREADS; t++) {
        producers.emplace_back([&total_queued, &total_executed, q, ACTIONS_PER_PRODUCER]() {
            for (int i = 0; i < ACTIONS_PER_PRODUCER; i++) {
                q->queueAction([&total_executed]() {
                    total_executed.fetch_add(1, std::memory_order_relaxed);
                });
                total_queued.fetch_add(1, std::memory_order_relaxed);
                // Small delay to interleave with drains
                if (i % 10 == 0) {
                    std::this_thread::yield();
                }
            }
        });
    }

    // Consumer drains periodically while producers are running
    std::thread consumer([&stop, q]() {
        while (!stop.load(std::memory_order_acquire)) {
            q->drain();
            std::this_thread::sleep_for(std::chrono::microseconds(50));
        }
        // Final drain
        q->drain();
    });

    // Wait for all producers
    for (auto& t : producers) {
        t.join();
    }

    // Signal consumer to stop and do final drain
    stop.store(true, std::memory_order_release);
    consumer.join();

    TEST_ASSERT_EQUAL(PRODUCER_THREADS * ACTIONS_PER_PRODUCER, total_queued.load());
    TEST_ASSERT_EQUAL(total_queued.load(), total_executed.load());
}

// ============================================================================
// Test Runner
// ============================================================================

int main(int argc, char **argv) {
    UNITY_BEGIN();

    // Basic functionality
    RUN_TEST(test_empty_drain_does_nothing);
    RUN_TEST(test_single_action_executes);
    RUN_TEST(test_multiple_actions_execute_in_order);
    RUN_TEST(test_drain_clears_queue);
    RUN_TEST(test_actions_queued_during_drain_survive);
    RUN_TEST(test_action_captures_values_by_copy);
    RUN_TEST(test_action_with_string_capture);

    // Rate-limited drain
    RUN_TEST(test_rate_limited_drain_processes_at_most_n);
    RUN_TEST(test_rate_limited_drain_preserves_order);
    RUN_TEST(test_rate_limited_drain_fewer_than_limit);
    RUN_TEST(test_rate_limited_empty_drain);
    RUN_TEST(test_rate_limited_exactly_at_limit);

    // Exception safety
    RUN_TEST(test_exception_in_action_does_not_crash);
    RUN_TEST(test_exception_in_batch_doesnt_lose_remaining);

    // Simulated real patterns
    RUN_TEST(test_simulated_tile_burst);
    RUN_TEST(test_interleaved_queue_and_drain);

    // Thread safety
    RUN_TEST(test_concurrent_queue_from_multiple_threads);
    RUN_TEST(test_concurrent_queue_while_draining);
    RUN_TEST(test_concurrent_queue_with_rate_limited_drain);
    RUN_TEST(test_no_actions_lost_under_contention);

    return UNITY_END();
}
