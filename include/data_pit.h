#pragma once

#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <any>
#include <chrono>
#include <typeindex>

enum class data_pit_error : int
{
    success             = 0,
    consumer_not_found  = -1,
    timeout_expired     = -2,
    no_data_available   = -3,
    type_mismatch       = -4
};

class data_pit
{
public:
    data_pit() : m_next_consumer_id(1) {}
    
    template<typename T>
    bool produce(int queue_id, const T& data)
    {
        std::lock_guard lock(m_mtx);
        // check if the type is already registered and is the same
        if (m_data_types.find(queue_id) != m_data_types.end() &&
            m_data_types[queue_id] != std::type_index(typeid(T)).name())
        {
            return false;
        }
        m_data_types[queue_id] = std::type_index(typeid(T)).name();
        m_data_queues[queue_id].push_back(data);
        m_cv.notify_all();
        return true;
    }

    template<typename T>
    std::optional<T> consume(unsigned int consumer_id, bool blocking = false,
                             uint32_t timeout_ms = std::numeric_limits<uint32_t>::max())
    {
        std::unique_lock lock(m_mtx);
        // check if consumer_id exists
        if (!m_consumer_indices.contains(consumer_id))
        {
            lock.unlock();
            set_last_error(consumer_id, data_pit_error::consumer_not_found);
            return std::nullopt;
        }

        auto queue_id = std::get<0>(m_consumer_indices[consumer_id]);
        // check if queue_id exists
        if (m_data_queues.find(queue_id) != m_data_queues.end())
        {
            if(m_data_types[queue_id] != std::type_index(typeid(T)).name())
            {
                lock.unlock();
                set_last_error(consumer_id, data_pit_error::type_mismatch);
                return std::nullopt;
            }
        }

        if (blocking)
        {
            if(m_cv.wait_for(lock, std::chrono::milliseconds(timeout_ms),
                           [&]()
                           {
                                return std::get<1>(m_consumer_indices[consumer_id]) < m_data_queues[queue_id].size();
                           }) == false)
            {
                // unlock the mutex before returning
                lock.unlock();
                // Timeout expired.
                set_last_error(consumer_id, data_pit_error::timeout_expired);
                return std::nullopt;
            }
        }
        if (std::get<1>(m_consumer_indices[consumer_id]) >= m_data_queues[queue_id].size())
        {
            lock.unlock();
            set_last_error(consumer_id, data_pit_error::no_data_available);
            return std::nullopt;
        }
        T data = std::any_cast<T>(m_data_queues[queue_id][std::get<1>(m_consumer_indices[consumer_id])]);
        std::get<1>(m_consumer_indices[consumer_id])++;
        return data;
    }

    unsigned int register_consumer(int queue_id)
    {
        std::lock_guard lock(m_mtx);
        unsigned int consumer_id = m_next_consumer_id++;
        m_consumer_indices[consumer_id] = std::make_tuple(queue_id, 0);
        return consumer_id;
    }

    data_pit_error get_last_error(unsigned int consumer_id)
    {
        std::lock_guard lock(m_mtx);
        // check if consumer_id exists
        if (m_consumer_indices.find(consumer_id) == m_consumer_indices.end())
        {
            return data_pit_error::consumer_not_found;
        }
        return m_consumer_last_error[consumer_id];
    }

private:

    void set_last_error(unsigned int consumer_id, data_pit_error error)
    {
        std::lock_guard lock(m_mtx);

        // check if consumer_id exists
        if (m_consumer_indices.find(consumer_id) == m_consumer_indices.end()) return;

        m_consumer_last_error[consumer_id] = error;
    }

    std::unordered_map<int, std::vector<std::any>> m_data_queues;
    std::unordered_map<unsigned int, std::tuple<int, size_t>> m_consumer_indices;
    std::unordered_map<int, std::string> m_data_types;
    std::unordered_map<unsigned int, data_pit_error> m_consumer_last_error;
    std::mutex m_mtx;
    std::condition_variable m_cv;
    unsigned int m_next_consumer_id;
};