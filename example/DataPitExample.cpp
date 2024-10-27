//
// Created by Salvatore Rivieccio on 27/10/24.
//
#include <syncstream>
#include <iostream>
#include <thread>
#include "DataPit.h"

void producer(const std::shared_ptr<queue_handle>& q, const int id)
{
    //std::osyncstream out(std::cout);
    for (int i = 0; i < 10; ++i)
    {
        if (q->produce(id * 10 + i))
        {
            std::cout << "Producer " << id << " produced: " << id * 10 + i << std::endl;
        }
        else
        {
            std::cout << "Producer " << id << " failed to produce: " << id * 10 + i << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
    }
}

void consumer(const std::shared_ptr<queue_handle>& q, const int id)
{
    for (int i = 0; i < 10; ++i)
    {
        if (auto data = q->consume<int>(true, 500); data.has_value())
        {
            std::cout << "Consumer " << id << " consumed: " << data.value() << std::endl;
        }
        else
        {
            std::cout << "Consumer " << id << " timed out" << std::endl;
        }
    }
}

void peeker(const std::shared_ptr<queue_handle>& q, const int id)
{
    for (int i = 0; i < 10; ++i)
    {
        if (auto data = q->peek<int>(true, 500); data.has_value())
        {
            std::cout << "Peeker " << id << " peeked: " << data.value() << std::endl;
        }
        else
        {
            std::cout << "Peeker " << id << " failed to peek" << std::endl;
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
    }
}

int main()
{
    data_pit dp;
    constexpr int queue_id = 0;
    auto q = dp.create_queue(queue_id);
    auto q2 = dp.get_queue_handle(queue_id);

    std::thread producer_thread(producer, q, 1);
    std::thread peeker_thread(peeker, q, 2);
    std::thread peeker_thread2(peeker, q2, 3);
    
    producer_thread.join();
    peeker_thread.join();
    peeker_thread2.join();
    
    std::thread consumer_thread(consumer, q, 1);
    consumer_thread.join();

    if(q->produce("Hello, World!") == true)
    {
        std::cout << "Produced: Hello, World!" << std::endl;
    }
    
    if (const auto data = q->consume<const char*>(true, 500); data.has_value())
    {
        std::cout << "Consumed: " << data.value() << std::endl;
    }
    else
    {
        std::cout << "Error" << std::endl;
    }

    dp.destroy_queue(queue_id);

    return 0;
}