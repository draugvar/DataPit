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
#include <optional>
#include <any>
#include <chrono>
#include <typeindex>

#include "concurrent_hash_map.h"

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
    data_pit_result produce(const int queue_id, const T& data)
    {
        // Lock the mutex to ensure thread safety
        std::unique_lock global_lock(m_mtx);

        // Check if the queue with the given ID already exists and if the type of the data matches
        if (m_queues_data.contains(queue_id))
        {
            if(!queue(queue_id).empty() && queue_type(queue_id) != std::type_index(typeid(T)).name())
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
        std::unique_lock lock(*queue_mutex(queue_id));

        // Unlock the global mutex
        global_lock.unlock();

        // If the queue is full, return a queue is full error
        if (queue(queue_id).size() >= queue_size(queue_id)) return data_pit_result::queue_is_full;

        // Set the type of the data for the queue
        queue_type(queue_id) = std::type_index(typeid(T)).name();

        // Add the data to the queue
        queue(queue_id).push_back(data);

        // Notify all waiting threads that new data has been added
        queue_cv(queue_id).notify_all();

        // Return success
        return data_pit_result::success;
    }

    /**
     * @brief               This function is used to consume data from the queue
     * @tparam  T           The type of data to be consumed
     * @param   consumer_id The ID of the consumer
     * @param   blocking    If true, the function will wait until there are data available
     * @param   timeout_ms  The maximum waiting time in milliseconds
     * @return              The consumed data, or std::nullopt if no data are available
     */
    template<typename T>
    std::optional<T> consume(const unsigned int consumer_id, const bool blocking = false,
                             const uint32_t timeout_ms = std::numeric_limits<uint32_t>::max())
    {
        // Lock the mutex to ensure thread safety
        std::unique_lock lock(m_mtx);

        // Check if the consumer_id exists
        if (!m_consumers_data.contains(consumer_id))
        {
            lock.unlock();
            set_last_error(consumer_id, data_pit_result::consumer_not_found);
            return std::nullopt;
        }

        const auto queue_id = std::get<0>(m_consumers_data.at(consumer_id));

        // Check if the queue_id exists
        if (m_queues_data.contains(queue_id))
        {
            // TODO FIXME - This is a temporary fix to avoid empty strings
            while (queue_type(queue_id).empty()) {}

            if(queue_type(queue_id) != std::type_index(typeid(T)).name())
            {
                lock.unlock();
                set_last_error(consumer_id, data_pit_result::type_mismatch);
                return std::nullopt;
            }
        }
        else
        {
            init_queue(queue_id);
            queue_type(queue_id) = std::type_index(typeid(T)).name();
        }

        // Lock the mutex for the specific queue
        std::unique_lock queue_lock(*queue_mutex(queue_id));

        // Unlock the global mutex
        lock.unlock();

        // Get the index of the consumer
        auto& index = consumer_index(consumer_id);

        // If blocking is true, wait until there are data available
        if (blocking)
        {
            if(queue_cv(queue_id).
                wait_for(queue_lock, std::chrono::milliseconds(timeout_ms),
                         [&]()
                         {
                             return index < queue(queue_id).size();
                         }) == false)
            {
                // Unlock the mutex before returning
                queue_lock.unlock();

                // Timeout expired
                set_last_error(consumer_id, data_pit_result::timeout_expired);
                return std::nullopt;
            }
        }

        // If there are no data available, return std::nullopt
        if (index >= queue(queue_id).size())
        {
            queue_lock.unlock();
            set_last_error(consumer_id, data_pit_result::no_data_available);
            return std::nullopt;
        }

        // Fetch the data from the queue
        T data = std::any_cast<T>(queue(queue_id).at(index));

        // Increment the consumer's index
        index++;

        // Return the data
        return data;
    }

    /**
     * @brief               This function is used to register a consumer to a queue
     * @param   queue_id    The id of the queue
     * @return              The id of the registered consumer or 0 if the maximum number of consumers has been reached
     */
    unsigned int register_consumer(int queue_id)
    {
        // Register a new consumer and get its ID
        const unsigned int consumer_id = register_id();

        // Lock the mutex to ensure thread safety
        std::unique_lock lock(m_mtx);

        // If the consumer_id is 0, it means that the maximum number of consumers has been reached
        if(consumer_id == 0) return 0;

        // Add the consumer to the map of consumers
        // The consumer is associated with the queue_id, its index in the queue (initially 0), and its last error (initially success)
        m_consumers_data[consumer_id] = std::make_tuple(queue_id, 0, data_pit_result::success);

        // Return the consumer_id
        return consumer_id;
    }

    /**
     * @brief               This function is used to unregister a consumer from a queue
     * @param   consumer_id The id of the consumer to be unregistered
     */
    void unregister_consumer(const unsigned int consumer_id)
    {
        // Release the consumer_id for future use
        unregister_id(consumer_id);

        // Lock the mutex to ensure thread safety
        std::unique_lock lock(m_mtx);

        // Remove the consumer from the map of consumers
        m_consumers_data.erase(consumer_id);
    }

    /**
     * @brief               This function is used to clear a specific queue
     * @param   queue_id    The id of the queue to be cleared
     */
    void clear_queue(const int queue_id)
    {
        // Lock the mutex to ensure thread safety
        std::unique_lock lock(m_mtx);

        // Clear the specific queue
        queue(queue_id).clear();
    }

    /**
     * @brief This function is used to clear all queues
     */
    void clear_all_queues()
    {
        // Lock the mutex to ensure thread safety
        std::unique_lock lock(m_mtx);

        // Clear all queues
        m_queues_data.clear();
    }

    /**
     * @brief               This function is used to reset a consumer's position in the queue
     * @param   consumer_id The id of the consumer to be reset
     */
    void reset_consumer(const unsigned int consumer_id)
    {
        // Lock the mutex to ensure thread safety
        std::unique_lock lock(m_mtx);

        // Check if consumer_id exists
        if (!m_consumers_data.find(consumer_id).has_value()) return;

        // Reset the consumer's index in the queue to 0
        std::get<1>(m_consumers_data[consumer_id]) = 0;
    }

    /**
     * @brief               This function is used to set the maximum size of a specific queue
     * @param   queue_id    The id of the queue
     * @param   size        The maximum size of the queue
     */
    void set_queue_size(const int queue_id, const size_t size)
    {
        // Lock the mutex to ensure thread safety
        std::unique_lock lock(m_mtx);

        // Check if the queue exists
        if(!m_queues_data.contains(queue_id))
        {
            // Initialize the queue if it doesn't exist
            init_queue(queue_id);
        }

        // Set the maximum size of the queue
        queue_size(queue_id) = size;
    }

    /**
     * @brief               This function is used to get the last error of a specific consumer
     * @param   consumer_id The id of the consumer
     * @return              The last error of the consumer
     */
    data_pit_result get_last_error(const unsigned int consumer_id)
    {
        // Lock the mutex to ensure thread safety
        std::unique_lock lock(m_mtx);

        // Check if consumer_id exists
        if (!m_consumers_data.find(consumer_id).has_value())
        {
            // If the consumer_id does not exist, return a consumer not found error
            return data_pit_result::consumer_not_found;
        }

        // Return the last error of the consumer
        return data_pit_error(consumer_id);
    }

private:

    /**
     * @brief               This function is used to set the last error of a specific consumer
     * @param   consumer_id The id of the consumer
     * @param   error       The error to be set
     */
    void set_last_error(const unsigned int consumer_id, const data_pit_result error)
    {
        // Lock the mutex to ensure thread safety
        std::unique_lock lock(m_mtx);

        // Check if consumer_id exists
        if (!m_consumers_data.find(consumer_id).has_value()) return;

        // Set the last error of the consumer
        data_pit_error(consumer_id) = error;
    }

    /**
     * @brief               This function is used to initialize a queue
     * @param   queue_id    The id of the queue
     */
    inline void init_queue(const int queue_id)
    {
        // Clear the vector and set the maximum size of the queue
        auto&[vector, size] = std::get<0>(m_queues_data[queue_id]);
        vector.clear();
        size = DATA_PIT_MAX_QUEUE_SIZE;

        // Create a new mutex for the queue
        std::get<2>(m_queues_data[queue_id]) = std::make_unique<std::mutex>();
    }

    /**
     * @brief               This function is used to get the queue with a specific id
     * @param   queue_id    The id of the queue
     * @return              The queue
     */
    inline std::vector<std::any>& queue(const int queue_id)
    {
        return std::get<0>(m_queues_data.at(queue_id)).first;
    }

    /**
     * @brief               This function is used to get the maximum size of a specific queue
     * @param   queue_id    The id of the queue
     * @return              The maximum size of the queue
     */
    inline size_t& queue_size(const int queue_id)
    {
        return std::get<0>( m_queues_data.at(queue_id)).second;
    }

    /**
     * @brief               This function is used to get the type of the data in a specific queue
     * @param   queue_id    The id of the queue
     * @return              The type of the data in the queue
     */
    inline std::string& queue_type(const int queue_id)
    {
        return std::get<1>(m_queues_data.at(queue_id));
    }

    /**
     * @brief               This function is used to get the mutex for a specific queue
     * @param   queue_id    The id of the queue
     * @return              The mutex for the queue
     */
    inline std::mutex* queue_mutex(const int queue_id)
    {
        return std::get<2>(m_queues_data.at(queue_id)).get();
    }

    /**
     * @brief               This function is used to get the condition variable for a specific queue
     * @param   queue_id    The id of the queue
     * @return              The condition variable for the queue
     */
    inline std::condition_variable& queue_cv(const int queue_id)
    {
        return std::get<3>(m_queues_data.at(queue_id));
    }

    /**
     * @brief               This function is used to get the index of a specific consumer
     * @param   consumer_id The id of the consumer
     * @return              The index of the consumer
     */
    inline size_t& consumer_index(const unsigned int consumer_id)
    {
        return std::get<1>(m_consumers_data.at(consumer_id));
    }

    /**
     * @brief               This function is used to get the last error of a specific consumer
     * @param   consumer_id The id of the consumer
     * @return              The last error of the consumer
     */
    inline data_pit_result& data_pit_error(const unsigned int consumer_id)
    {
        return std::get<2>(m_consumers_data.at(consumer_id));
    }


    /**
     * @brief This function is used to register a new consumer ID
     * @return The registered consumer ID or 0 if the maximum number of consumers has been reached
     */
    unsigned int register_id()
    {
        // Lock the mutex to ensure thread safety
        std::lock_guard lock(m_mtx);

        unsigned int consumer_id;

        // Check if there are any released IDs available
        if (!released_ids.empty())
        {
            // If there are, use the first one
            consumer_id = released_ids.front();
            released_ids.pop();
        }
        else
        {
            // If there are no released IDs available, check if the next consumer ID has reached its maximum value
            if(m_next_consumer_id == std::numeric_limits<unsigned int>::max())
            {
                // If it has, return 0
                return 0;
            }

            // If the next consumer ID has not reached its maximum value, use it and increment it
            consumer_id = m_next_consumer_id++;
        }

        // Return the registered consumer ID
        return consumer_id;
    }

    /**
     * @brief               This function is used to unregister a specific consumer ID
     * @param   consumer_id The ID of the consumer to be unregistered
     */
    void unregister_id(const unsigned int consumer_id)
    {
        std::lock_guard lock(m_mtx);
        released_ids.push(consumer_id);
    }

    // Type aliases for data structure to store the data for each queue
    typedef std::tuple<std::pair<std::vector<std::any>, size_t>,
                        std::string, std::unique_ptr<std::mutex>, std::condition_variable> data_t;
    // Type aliases for queue id
    typedef int queue_id_t;
    // Type aliases for consumer id
    typedef unsigned int consumer_id_t;
    // Type aliases for index
    typedef size_t index_t;
    // Type aliases for data structure to store the data for each consumer
    typedef std::tuple<queue_id_t, index_t, data_pit_result> consumer_data_t;

    // Data structure to store the data for each queue
    concurrent_hash_map<queue_id_t, data_t> m_queues_data;
    // Data structure to store the data for each consumer
    concurrent_hash_map<consumer_id_t, consumer_data_t> m_consumers_data;
    // Mutex for thread safety
    std::mutex m_mtx;
    // Next consumer ID
    unsigned int m_next_consumer_id;
    // Queue of released IDs
    std::queue<unsigned int> released_ids;
};