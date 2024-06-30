/*
 *  data_pit.h
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
#pragma once

#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <any>
#include <chrono>
#include <typeindex>

#define DATA_PIT_VERSION_MAJOR 1
#define DATA_PIT_VERSION_MINOR 0
#define DATA_PIT_VERSION_PATCH 0
#define DATA_PIT_VERSION (DATA_PIT_VERSION_MAJOR << 16 | DATA_PIT_VERSION_MINOR << 8 | DATA_PIT_VERSION_PATCH)

#define DATA_PIT_MAX_QUEUE_SIZE 1000

/**
 * @brief data_pit_result enum class
 */
enum class data_pit_result : int
{
    success             = 0,
    consumer_not_found  = -1,
    timeout_expired     = -2,
    no_data_available   = -3,
    type_mismatch       = -4,
    queue_is_full       = -5
};

/**
 * @brief data_pit class
 */
class data_pit
{
public:
    /**
     * @brief Constructor
     */
    data_pit() : m_next_consumer_id(1)
    {
        m_queues_data.clear();
    }

    /**
     * @brief               This function is used to produce data in the queue
     * @tparam  T           The type of the data to be produced
     * @param   queue_id    The id of the queue
     * @param   data        The data to be produced
     * @return              The result of the operation
     */
    template<typename T>
    data_pit_result produce(int queue_id, const T& data)
    {
        // Lock the mutex to ensure thread safety
        std::unique_lock global_lock(m_mtx);

        // Check if the queue with the given ID already exists and if the type of the data matches
        if (m_queues_data.contains(queue_id))
        {
            if(!get_queue(queue_id).empty() && get_type(queue_id) != std::type_index(typeid(T)).name())
            {
                // If the type of the data does not match, return a type mismatch error
                return data_pit_result::type_mismatch;
            }
        }
        else
        {
            // If the queue does not exist, initialize it
            init_queue(queue_id);
        }

        // Lock the mutex for the specific queue
        std::lock_guard lock(get_mutex(queue_id));

        // Unlock the global mutex
        global_lock.unlock();

        // If the queue is full, return a queue is full error
        if (get_queue(queue_id).size() >= get_queue_size(queue_id)) return data_pit_result::queue_is_full;

        // Set the type of the data for the queue
        get_type(queue_id) = std::type_index(typeid(T)).name();

        // Add the data to the queue
        get_queue(queue_id).push_back(data);

        // Notify all waiting threads that new data has been added
        get_cv(queue_id).notify_all();

        // Return success
        return data_pit_result::success;
    }

    template<typename T>
    std::optional<T> consume(unsigned int consumer_id, bool blocking = false,
                             uint32_t timeout_ms = std::numeric_limits<uint32_t>::max())
    {
        std::unique_lock lock(m_mtx);
        // check if consumer_id exists
        if (!m_consumers_data.contains(consumer_id))
        {
            lock.unlock();
            set_last_error(consumer_id, data_pit_result::consumer_not_found);
            return std::nullopt;
        }

        auto queue_id = std::get<0>(m_consumers_data.at(consumer_id));
        // check if queue_id exists
        if (m_queues_data.contains(queue_id))
        {
            if(get_type(queue_id) != std::type_index(typeid(T)).name())
            {
                lock.unlock();
                set_last_error(consumer_id, data_pit_result::type_mismatch);
                return std::nullopt;
            }
        }
        else
        {
            init_queue(queue_id);
            get_type(queue_id) = std::type_index(typeid(T)).name();
        }

        std::unique_lock queue_lock(get_mutex(queue_id));
        lock.unlock();

        if (blocking)
        {
            if(get_cv(queue_id).wait_for(queue_lock, std::chrono::milliseconds(timeout_ms),
                           [&]()
                           {
                                return std::get<1>(m_consumers_data.at(consumer_id)) < get_queue(queue_id).size();
                           }) == false)
            {
                // unlock the mutex before returning
                queue_lock.unlock();
                // Timeout expired.
                set_last_error(consumer_id, data_pit_result::timeout_expired);
                return std::nullopt;
            }
        }
        if (std::get<1>(m_consumers_data.at(consumer_id)) >= get_queue(queue_id).size())
        {
            queue_lock.unlock();
            set_last_error(consumer_id, data_pit_result::no_data_available);
            return std::nullopt;
        }
        T data = std::any_cast<T>(get_queue(queue_id)[std::get<1>(m_consumers_data.at(consumer_id))]);
        std::get<1>(m_consumers_data.at(consumer_id))++;
        return data;
    }

    unsigned int register_consumer(int queue_id)
    {
        std::lock_guard lock(m_mtx);
        unsigned int consumer_id = register_id();
        if(consumer_id == 0) return 0;
        m_consumers_data[consumer_id] = std::make_tuple(queue_id, 0, data_pit_result::success);
        return consumer_id;
    }

    void unregister_consumer(unsigned int consumer_id)
    {
        std::lock_guard lock(m_mtx);
        m_consumers_data.erase(consumer_id);
        unregister_id(consumer_id);
    }

    void clear_queue(int queue_id)
    {
        std::lock_guard lock(m_mtx);
        get_queue(queue_id).clear();
    }

    void clear_all_queues()
    {
        std::lock_guard lock(m_mtx);
        m_queues_data.clear();
    }

    void reset_consumer(unsigned int consumer_id)
    {
        std::lock_guard lock(m_mtx);
        // check if consumer_id exists
        if (m_consumers_data.find(consumer_id) == m_consumers_data.end()) return;
        std::get<1>(m_consumers_data[consumer_id]) = 0;
    }

    void set_queue_size(int queue_id, size_t size)
    {
        std::lock_guard lock(m_mtx);
        init_queue(queue_id);
        get_queue_size(queue_id) = size;
    }

    data_pit_result get_last_error(unsigned int consumer_id)
    {
        std::lock_guard lock(m_mtx);
        // check if consumer_id exists
        if (m_consumers_data.find(consumer_id) == m_consumers_data.end())
        {
            return data_pit_result::consumer_not_found;
        }
        return get_data_pit_error(consumer_id);
    }

private:

    void set_last_error(unsigned int consumer_id, data_pit_result error)
    {
        std::lock_guard lock(m_mtx);

        // check if consumer_id exists
        if (m_consumers_data.find(consumer_id) == m_consumers_data.end()) return;

        get_data_pit_error(consumer_id) = error;
    }

    inline void init_queue(int queue_id)
    {
        auto&[vector, size] = std::get<0>(m_queues_data[queue_id]);
        vector.clear();
        size = DATA_PIT_MAX_QUEUE_SIZE;
    }

    inline std::vector<std::any>& get_queue(int queue_id)
    {
        return std::get<0>(m_queues_data.at(queue_id)).first;
    }

    inline size_t& get_queue_size(int queue_id)
    {
        return std::get<0>(m_queues_data.at(queue_id)).second;
    }

    inline std::string& get_type(int queue_id)
    {
        return std::get<1>(m_queues_data.at(queue_id));
    }

    inline std::mutex& get_mutex(int queue_id)
    {
        return std::get<2>(m_queues_data.at(queue_id));
    }

    inline std::condition_variable& get_cv(int queue_id)
    {
        return std::get<3>(m_queues_data.at(queue_id));
    }

    inline data_pit_result& get_data_pit_error(unsigned int consumer_id)
    {
        return std::get<2>(m_consumers_data.at(consumer_id));
    }

    unsigned int register_id()
    {
        std::lock_guard lock(m_mtx);
        unsigned int consumer_id;
        if (!released_ids.empty())
        {
            consumer_id = released_ids.front();
            released_ids.pop();
        }
        else
        {
            if(m_next_consumer_id == std::numeric_limits<unsigned int>::max()) return 0;

            consumer_id = m_next_consumer_id++;
        }
        return consumer_id;
    }

    void unregister_id(unsigned int consumer_id)
    {
        std::lock_guard lock(m_mtx);
        released_ids.push(consumer_id);
    }

    // Type aliases for data structure to store the data for each queue
    typedef std::tuple<std::pair<std::vector<std::any>, size_t>, std::string, std::mutex, std::condition_variable> data_t;
    // Type aliases for queue id
    typedef int queue_id_t;
    // Type aliases for consumer id
    typedef unsigned int consumer_id_t;
    // Type aliases for index
    typedef size_t index_t;
    // Type aliases for data structure to store the data for each consumer
    typedef std::tuple<queue_id_t, index_t, data_pit_result> consumer_data_t;

    // Data structure to store the data for each queue
    std::unordered_map<queue_id_t, data_t> m_queues_data;
    std::unordered_map<consumer_id_t, consumer_data_t> m_consumers_data;
    std::recursive_mutex m_mtx;
    unsigned int m_next_consumer_id;
    std::queue<unsigned int> released_ids;
};