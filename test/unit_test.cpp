//
// Created by Salvatore Rivieccio on 25/06/24.
//
#include <gtest/gtest.h>
#include <thread>
#include <DataPit.h>

enum queue_id
{
    queue_1,
    queue_2
};

TEST(DataPit, test_produce_consume)
{
    DataPit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    int data = 42;
    dp.produce(queue_1, data);
    auto result = dp.consume<int>(consumer_id, queue_1);
    ASSERT_TRUE(result.has_value());
    ASSERT_EQ(data, result.value());
}

TEST(DataPit, test_produce_consume_multiple)
{
    DataPit dp;
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

TEST(DataPit, test_produce_consume_multiple_queues)
{
    DataPit dp;
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

TEST(DataPit, test_produce_consume_multiple_consumers)
{
    DataPit dp;
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

TEST(DataPit, test_produce_consume_blocking)
{
    DataPit dp;

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

TEST(DataPit, test_produce_consume_blocking_timeout)
{
    DataPit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id, true, 100);
    ASSERT_FALSE(result.has_value());
}

TEST(DataPit, test_produce_consume_wrong_type)
{
    DataPit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    dp.produce(queue_1, 42);
    auto result = dp.consume<float>(consumer_id);
    ASSERT_FALSE(result.has_value());
    std::cout << dp.get_last_error(consumer_id) << std::endl;
}

TEST(DataPit, test_produce_consume_wrong_queue)
{
    DataPit dp;
    auto consumer_id = dp.register_consumer(queue_2);
    dp.produce(queue_1, 42);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
    std::cout << dp.get_last_error(consumer_id) << std::endl;
}

TEST(DataPit, test_produce_consume_no_data)
{
    DataPit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
    std::cout << dp.get_last_error(consumer_id) << std::endl;
}

TEST(DataPit, test_produce_consume_no_data_blocking_timeout_thread)
{
    DataPit dp;

    std::thread t([&dp]()
    {
        auto consumer_id = dp.register_consumer(queue_1);
        auto result = dp.consume<int>(consumer_id, true, 100);
        ASSERT_FALSE(result.has_value());
    });

    t.join();
}

TEST(DataPit, test_produce_consume_no_data_blocking_timeout)
{
    DataPit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id, true, 100);
    ASSERT_FALSE(result.has_value());
}

TEST(DataPit, test_produce_consume_mismatched_type)
{
    DataPit dp;
    auto consumer_id = dp.register_consumer(queue_1);
    dp.produce(queue_1, 42);
    auto result = dp.consume<float>(consumer_id);
    ASSERT_FALSE(result.has_value());
    std::cout << dp.get_last_error(consumer_id) << std::endl;
}

TEST(DataPit, test_consume_before_register)
{
    DataPit dp;
    auto result = dp.consume<int>(1);
    ASSERT_FALSE(result.has_value());
    std::cout << dp.get_last_error(1) << std::endl;
}

TEST(DataPit, test_produce_wrong_type)
{
    DataPit dp;
    int message = 0;
    dp.produce(queue_1, std::ref(message));
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
}

TEST(DataPit, test_produce_consume_error_reference)
{
    DataPit dp;
    int message = 0;
    dp.produce(queue_1, &message);
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int>(consumer_id);
    ASSERT_FALSE(result.has_value());
}

TEST(DataPit, test_produce_consume_reference)
{
    DataPit dp;
    int message = 0;
    dp.produce(queue_1, &message);
    auto consumer_id = dp.register_consumer(queue_1);
    auto result = dp.consume<int*>(consumer_id);
    ASSERT_TRUE(result.has_value());
    auto message_consume = result.value();
    *message_consume = 42;
    ASSERT_EQ(message, 42);
}

int main(int argc, char **argv)
{
    ::testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}