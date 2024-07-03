/*
 *  unit_test.cpp
 *  data_pit
 *
 *  Copyright (c) 2024 Salvatore Rivieccio. All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NON-INFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */
#include <gtest/gtest.h>
#include <thread>
#include <atomic>
#include <list>
#include <data_pit.h>

enum queue_id
{
    queue_1,
    queue_2
};

std::string error_to_string(data_pit_result error)
{
    switch (error)
    {
    case data_pit_result::success:
        return "success";
    case data_pit_result::timeout_expired:
        return "timeout_expired";
    case data_pit_result::no_data_available:
        return "no_data_available";
    case data_pit_result::consumer_not_found:
        return "consumer_not_found";
    case data_pit_result::type_mismatch:
        return "type_mismatch";
    default:
        return "unknown";
    }
}

TEST(data_pit, test_version)
{
    printf("data_pit version: %d.%d.%d\n", DATA_PIT_VERSION_MAJOR, DATA_PIT_VERSION_MINOR, DATA_PIT_VERSION_PATCH);
    printf("data_pit version hex: %.6x\n", DATA_PIT_VERSION);
}

TEST(data_pit, test_produce_consume)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    int data = 42;
    dp.produce(queue_1, data);
    auto result = dp.consume<int>(consumer_id, queue_1);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(data, result.value());
}

TEST(data_pit, test_produce_consume_multiple)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    int data1 = 42;
    int data2 = 43;
    dp.produce(queue_1, data1);
    dp.produce(queue_1, data2);
    auto result1 = dp.consume<int>(consumer_id, queue_1);
    auto result2 = dp.consume<int>(consumer_id, queue_1);
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(data1, result1.value());
    ASSERT_EQ(data2, result2.value());
}

TEST(data_pit, test_produce_consume_multiple_queues)
{
    data_pit dp;
    auto consumer_id_1 = dp.register_consumer(queue_1);
    auto consumer_id_2 = dp.register_consumer(queue_2);
    int data1 = 42;
    int data2 = 43;
    dp.produce(queue_1, data1);
    dp.produce(queue_2, data2);
    auto result1 = dp.consume<int>(consumer_id_1);
    auto result2 = dp.consume<int>(consumer_id_2);
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(data1, result1.value());
    ASSERT_EQ(data2, result2.value());
}

TEST(data_pit, test_produce_consume_multiple_consumers)
{
    data_pit dp;
    auto consumer_id1 = dp.register_consumer(queue_1);
    auto consumer_id2 = dp.register_consumer(queue_1);
    int data1 = 42;
    int data2 = 43;
    dp.produce(queue_1, data1);
    dp.produce(queue_1, data2);
    auto result1 = dp.consume<int>(consumer_id1);
    auto result2 = dp.consume<int>(consumer_id2);
    ASSERT_TRUE(result1.has_value());
    ASSERT_TRUE(result2.has_value());
    ASSERT_EQ(data1, result1.value());
    ASSERT_EQ(data1, result2.value());
}

TEST(data_pit, test_produce_consume_blocking)
{
    data_pit dp;

    std::thread t([&dp]()
    {
        auto consumer_id = dp.register_consumer(queue_1);
        auto result = dp.consume<int>(consumer_id, true);
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(42, result.value());
    });

    std::thread t2([&dp]()
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        dp.produce(queue_1, 42);
    });

    t2.join();
    t.join();
}

TEST(data_pit, test_produce_consume_blocking_timeout)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id, true, 100);
    ASSERT_FALSE(result.has_value());
}

TEST(data_pit, test_produce_consume_wrong_type)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    dp.produce(queue_1, 42);
    auto result = dp.consume<float>(consumer_id);
    ASSERT_FALSE(result.has_value());
    std::cout << error_to_string(dp.get_last_error(consumer_id)) << std::endl;
}

TEST(data_pit, test_produce_consume_wrong_queue)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_2);
    dp.produce(queue_1, 42);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
    std::cout << error_to_string(dp.get_last_error(consumer_id)) << std::endl;
}

TEST(data_pit, test_produce_consume_no_data)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
    std::cout << error_to_string(dp.get_last_error(consumer_id)) << std::endl;
}

TEST(data_pit, test_produce_consume_no_data_blocking_timeout_thread)
{
    data_pit dp;

    std::thread t([&dp]()
    {
        auto consumer_id = dp.register_consumer(queue_1);
        auto result = dp.consume<int>(consumer_id, true, 100);
        ASSERT_FALSE(result.has_value());
    });

    t.join();
}

TEST(data_pit, test_produce_consume_no_data_blocking_timeout)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id, true, 100);
    ASSERT_FALSE(result.has_value());
}

TEST(data_pit, test_produce_consume_mismatched_type)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    dp.produce(queue_1, 42);
    auto result = dp.consume<float>(consumer_id);
    ASSERT_FALSE(result.has_value());
    std::cout << error_to_string(dp.get_last_error(consumer_id)) << std::endl;
}

TEST(data_pit, test_consume_before_register)
{
    data_pit dp;
    auto result = dp.consume<int>(1);
    ASSERT_FALSE(result.has_value());
    std::cout << error_to_string(dp.get_last_error(1)) << std::endl;
}

TEST(data_pit, test_produce_wrong_type)
{
    data_pit dp;
    int message = 0;
    dp.produce(queue_1, std::ref(message));
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
}

TEST(data_pit, test_produce_consume_error_reference)
{
    data_pit dp;
    int message = 0;
    dp.produce(queue_1, &message);
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
}

TEST(data_pit, test_produce_consume_reference)
{
    data_pit dp;
    int message = 0;
    dp.produce(queue_1, &message);
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int*>(consumer_id);
    ASSERT_TRUE(result.has_value());
    auto message_consume = result.value();
    *message_consume = 42;
    ASSERT_EQ(message, 42);
}

TEST(data_pit, test_reset_index)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    for(auto i = 0; i < 100; ++i)
    {
        dp.produce(queue_1, i);
    }
    // consume 50 messages
    for(auto i = 0; i < 50; ++i)
    {
        auto result = dp.consume<int>(consumer_id);
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(i, result.value());
    }
    dp.reset_consumer(consumer_id);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(0, result.value());
}

TEST(data_pit, test_set_queue_size)
{
    data_pit dp;
    dp.set_queue_size(queue_1, 10);
    auto consumer_id = dp.register_consumer(queue_1);
    for(auto i = 0; i < 100; ++i)
    {
        auto ret = dp.produce(queue_1, i);
        if(i >= 10)
        {
            ASSERT_EQ(data_pit_result::queue_is_full, ret);
        }
    }
    // consume 10 messages
    for(auto i = 0; i < 10; ++i)
    {
        auto result = dp.consume<int>(consumer_id);
        ASSERT_TRUE(result.has_value());
        ASSERT_EQ(i, result.value());
    }
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
}

TEST(data_pit, test_clear_queue)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    for(auto i = 0; i < 100; ++i)
    {
        dp.produce(queue_1, i);
    }
    dp.clear_queue(queue_1);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
}

TEST(data_pit, test_clear_all_queues)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    auto consumer_id_2 = dp.register_consumer(queue_2);
    for(auto i = 0; i < 100; ++i)
    {
        dp.produce(queue_1, i);
        dp.produce(queue_2, i);
    }
    dp.clear_all_queues();
    auto result1 = dp.consume<int>(consumer_id);
    auto result2 = dp.consume<int>(consumer_id_2);
    ASSERT_FALSE(result1.has_value());
    ASSERT_FALSE(result2.has_value());
}

TEST(data_pit, test_unregister_consumer)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    for(auto i = 0; i < 100; ++i)
    {
        dp.produce(queue_1, i);
    }
    dp.unregister_consumer(consumer_id);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
}

TEST(data_pit, test_produce_consume_error)
{
    data_pit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    ASSERT_EQ(consumer_id, 1);
    for(auto i = 0; i < 100; ++i)
    {
        dp.produce(queue_1, i);
    }
    dp.unregister_consumer(consumer_id);
    consumer_id = dp.register_consumer(queue_1);
    ASSERT_EQ(consumer_id, 1);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_TRUE(result.has_value());
}

TEST(data_pit, test_multi_threading)
{
    data_pit dp;
    std::atomic_int counter = 0;
    std::list<std::thread> threads;
    for(auto i = 0; i < 10; ++i)
    {
        threads.emplace_back([&dp, &counter]()
        {
            for(auto j = 0; j < 10; ++j)
            {
                dp.produce(queue_1, counter.fetch_add(1));
            }
        });
    }
    for(auto i = 0; i < 10; ++i)
    {
        threads.emplace_back([&dp]()
        {
            auto consumer_id = dp.register_consumer(queue_1);
            std::list<int> results;
            for(auto j = 0; j < 100; ++j)
            {
                auto result = dp.consume<int>(consumer_id, true);
                if(result.has_value())
                {
                    results.push_back(result.value());
                }
            }
            results.sort();
            for(auto i = 0; i < 100; ++i)
            {
                ASSERT_EQ(i, results.front());
                results.pop_front();
            }
        });
    }
    for(auto &t : threads)
    {
        t.join();
    }
}

TEST(data_pit, test_multi_thread_multi_queue)
{
    for(auto iterations = 0; iterations < 10; ++iterations)
    {
        data_pit dp;

        std::list<std::thread> threads;
        for(auto i = 0; i < 10; ++i)
        {
            threads.emplace_back([&dp, i]()
            {
                for(auto j = 0; j < 100; ++j)
                {
                    ASSERT_EQ(dp.produce(i, j), data_pit_result::success);
                }
            });
        }
        for(auto i = 0; i < 10; ++i)
        {
            threads.emplace_back([&dp, i]()
            {
                auto consumer_id = dp.register_consumer(i);
                std::list<int> results;
                for(auto j = 0; j < 100; ++j)
                {
                    auto result = dp.consume<int>(consumer_id, true);
                    if(result.has_value())
                    {
                        results.push_back(result.value());
                    }
                }

                for(auto j = 0; j < 100; ++j)
                {
                    ASSERT_EQ(j, results.front());
                    results.pop_front();
                }
            });
        }
        for(auto &t : threads)
        {
            t.join();
        }
    }
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}