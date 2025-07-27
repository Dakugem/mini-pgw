#include "queue.h"

#include <gtest/gtest.h>

#include <thread>
#include <atomic>
#include <vector>
#include <memory>
#include <chrono>

using namespace IO_Utils;
using namespace std::chrono_literals;

// Тесты в одном потоке

TEST(QueueTest, PushPopSingleElement)
{
    Queue<int> queue(10);
    auto elem = std::make_unique<int>(42);
    EXPECT_TRUE(queue.push(std::move(elem)));
    auto popped = queue.pop();
    ASSERT_NE(popped, nullptr);
    EXPECT_EQ(*popped, 42);
}

TEST(QueueTest, PushUntilFull)
{
    Queue<int> queue(5);
    for (int i = 0; i < 5; ++i)
    {
        EXPECT_TRUE(queue.push(std::make_unique<int>(i)));
    }
    EXPECT_FALSE(queue.push(std::make_unique<int>(100)));
}

TEST(QueueTest, PopFromEmpty)
{
    Queue<int> queue(5);
    auto elem = queue.pop();
    EXPECT_EQ(elem, nullptr);
}

TEST(QueueTest, FIFOOrder)
{
    Queue<int> queue(5);
    for (int i = 0; i < 5; ++i)
    {
        queue.push(std::make_unique<int>(i));
    }
    for (int i = 0; i < 5; ++i)
    {
        auto elem = queue.pop();
        ASSERT_NE(elem, nullptr);
        EXPECT_EQ(*elem, i);
    }
}

TEST(QueueTest, PushPopSequence)
{
    Queue<int> queue(3);
    for (int i = 0; i < 3; ++i)
    {
        queue.push(std::make_unique<int>(i));
    }
    for (int i = 0; i < 2; ++i)
    {
        queue.pop();
    }
    queue.push(std::make_unique<int>(10));
    queue.push(std::make_unique<int>(11));

    EXPECT_EQ(*queue.pop(), 2);
    EXPECT_EQ(*queue.pop(), 10);
    EXPECT_EQ(*queue.pop(), 11);
}

// Тесты на межпотоковое взаимодействие

TEST(QueueMultiThreadTest, SingleProducerSingleConsumer)
{
    constexpr size_t CAPACITY = 100;
    constexpr size_t ITEMS_COUNT = 1000;
    Queue<int> queue(CAPACITY);

    std::atomic<bool> producer_done{false};
    std::vector<int> consumed_items;

    auto producer = [&]
    {
        for (size_t i = 0; i < ITEMS_COUNT; ++i)
        {
            while (!queue.push(std::make_unique<int>(i)))
            {
                std::this_thread::yield();
            }
        }
        producer_done = true;
    };

    auto consumer = [&]
    {
        size_t ctr = 0;
        while (!producer_done || ctr < ITEMS_COUNT)
        {
            auto item = queue.pop();
            if (item)
            {
                ctr++;
                consumed_items.push_back(*item.release());
            }
            std::this_thread::yield();
        }
    };

    std::thread prod_thread(producer);
    std::thread cons_thread(consumer);

    prod_thread.join();
    cons_thread.join();

    EXPECT_EQ(consumed_items.size(), ITEMS_COUNT);

    for (size_t i = 0; i < ITEMS_COUNT; ++i)
    {
        EXPECT_EQ(consumed_items[i], static_cast<int>(i));
    }
}

TEST(QueueMultiThreadTest, OverflowHandling)
{
    constexpr size_t CAPACITY = 10;
    Queue<int> queue(CAPACITY);

    std::atomic<size_t> pushed_count{0};
    std::atomic<bool> stop{false};

    auto producer = [&]
    {
        for (size_t i = 0; !stop; ++i)
        {
            if (queue.push(std::make_unique<int>(i)))
            {
                pushed_count++;
            }
        }
    };

    std::thread prod_thread(producer);

    std::this_thread::sleep_for(10ms);
    stop = true;
    prod_thread.join();

    EXPECT_LE(pushed_count, CAPACITY);
    EXPECT_GT(pushed_count, 0);
}

TEST(QueueMultiThreadTest, UnderflowHandling)
{
    constexpr size_t CAPACITY = 10;
    constexpr size_t ITEMS_COUNT = 5;
    Queue<int> queue(CAPACITY);

    for (size_t i = 0; i < ITEMS_COUNT; ++i)
    {
        queue.push(std::make_unique<int>(i));
    }

    std::atomic<size_t> popped_count{0};
    std::atomic<bool> consumer_done{false};

    auto consumer = [&]
    {
        while (!consumer_done)
        {
            auto item = queue.pop();
            if (item)
            {
                popped_count++;
            }
            std::this_thread::yield();
        }
    };

    std::thread cons_thread(consumer);

    std::this_thread::sleep_for(10ms);
    consumer_done = true;
    cons_thread.join();

    EXPECT_EQ(popped_count, ITEMS_COUNT);

    EXPECT_EQ(queue.pop(), nullptr);
}

TEST(QueueMultiThreadTest, ConcurrentAccess)
{
    constexpr size_t CAPACITY = 50;
    constexpr size_t ITEMS_COUNT = 1000;
    Queue<int> queue(CAPACITY);

    std::atomic<size_t> produced{0};
    std::atomic<size_t> consumed{0};
    std::atomic<bool> stop{false};

    auto producer = [&]
    {
        while (produced < ITEMS_COUNT)
        {
            if (queue.push(std::make_unique<int>(produced.load())))
            {
                produced++;
            }
        }
    };

    auto consumer = [&]
    {
        while (!stop || consumed < ITEMS_COUNT)
        {
            auto item = queue.pop();
            if (item)
            {
                consumed++;
            }
        }
    };

    std::thread prod_thread(producer);
    std::thread cons_thread(consumer);

    prod_thread.join();
    stop = true;
    cons_thread.join();

    EXPECT_EQ(produced, ITEMS_COUNT);
    EXPECT_EQ(consumed, ITEMS_COUNT);
}

TEST(QueueMultiThreadTest, OrderUnderLoad)
{
    constexpr size_t CAPACITY = 100;
    constexpr size_t ITEMS_COUNT = 10000;
    Queue<size_t> queue(CAPACITY);

    std::vector<size_t> consumed;
    std::atomic<bool> stop{false};

    auto producer = [&]
    {
        for (size_t i = 0; i < ITEMS_COUNT; ++i)
        {
            while (!queue.push(std::make_unique<size_t>(i)))
            {
                std::this_thread::yield();
            }
        }
        stop = true;
    };

    auto consumer = [&]
    {
        size_t ctr = 0;
        while (!stop || ctr < ITEMS_COUNT)
        {
            auto item = queue.pop();
            if (item)
            {
                ctr++;
                consumed.push_back(*item);
            }
            std::this_thread::yield();
        }
    };

    std::thread prod_thread(producer);
    std::thread cons_thread(consumer);

    prod_thread.join();
    cons_thread.join();

    ASSERT_EQ(consumed.size(), ITEMS_COUNT);

    EXPECT_EQ(consumed.back(), ITEMS_COUNT - 1);

    for (size_t i = 0; i < ITEMS_COUNT; ++i)
    {
        EXPECT_EQ(consumed[i], i);
    }
}