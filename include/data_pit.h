#pragma once

#include <queue>
#include <unordered_map>
#include <mutex>
#include <condition_variable>
#include <optional>
#include <any>
#include <chrono>
#include <typeindex>

#define DATA_PIT_MAX_QUEUE_SIZE 1000

enum class data_pit_error : int
{
    success             = 0,
    consumer_not_found  = -1,
    timeout_expired     = -2,
    no_data_available   = -3,
    type_mismatch       = -4,
    queue_is_full       = -5
};

class data_pit
{
public:
    data_pit() : m_next_consumer_id(1)
    {
        m_data_queues.clear();
    }
    
    template<typename T>
    data_pit_error produce(int queue_id, const T& data)
    {
        std::unique_lock global_lock(m_mtx);
        // check if the type is already registered and is the same
        if (m_data_queues.contains(queue_id))
        {
            if(!get_queue(queue_id).empty() && get_type(queue_id) != std::type_index(typeid(T)).name())
            {
                return data_pit_error::type_mismatch;
            }
        }
        else
        {
            init_queue(queue_id);
        }
        std::lock_guard lock(get_mutex(queue_id));
        global_lock.unlock();

        if (get_queue(queue_id).size() >= get_queue_size(queue_id)) return data_pit_error::queue_is_full;

        get_type(queue_id) = std::type_index(typeid(T)).name();
        get_queue(queue_id).push_back(data);
        get_cv(queue_id).notify_all();
        return data_pit_error::success;
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

        auto queue_id = std::get<0>(m_consumer_indices.at(consumer_id));
        // check if queue_id exists
        if (m_data_queues.contains(queue_id))
        {
            if(get_type(queue_id) != std::type_index(typeid(T)).name())
            {
                lock.unlock();
                set_last_error(consumer_id, data_pit_error::type_mismatch);
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
                                return std::get<1>(m_consumer_indices.at(consumer_id)) < get_queue(queue_id).size();
                           }) == false)
            {
                // unlock the mutex before returning
                queue_lock.unlock();
                // Timeout expired.
                set_last_error(consumer_id, data_pit_error::timeout_expired);
                return std::nullopt;
            }
        }
        if (std::get<1>(m_consumer_indices.at(consumer_id)) >= get_queue(queue_id).size())
        {
            queue_lock.unlock();
            set_last_error(consumer_id, data_pit_error::no_data_available);
            return std::nullopt;
        }
        T data = std::any_cast<T>(get_queue(queue_id)[std::get<1>(m_consumer_indices.at(consumer_id))]);
        std::get<1>(m_consumer_indices.at(consumer_id))++;
        return data;
    }

    unsigned int register_consumer(int queue_id)
    {
        std::lock_guard lock(m_mtx);
        unsigned int consumer_id = m_next_consumer_id++;
        m_consumer_indices[consumer_id] = std::make_tuple(queue_id, 0);
        return consumer_id;
    }

    void unregister_consumer(unsigned int consumer_id)
    {
        std::lock_guard lock(m_mtx);
        m_consumer_indices.erase(consumer_id);
        m_consumer_last_error.erase(consumer_id);
    }

    void clear_queue(int queue_id)
    {
        std::lock_guard lock(m_mtx);
        get_queue(queue_id).clear();
    }

    void clear_all_queues()
    {
        std::lock_guard lock(m_mtx);
        m_data_queues.clear();
    }

    void reset_consumer(unsigned int consumer_id)
    {
        std::lock_guard lock(m_mtx);
        // check if consumer_id exists
        if (m_consumer_indices.find(consumer_id) == m_consumer_indices.end()) return;
        std::get<1>(m_consumer_indices[consumer_id]) = 0;
    }

    void set_queue_size(int queue_id, size_t size)
    {
        std::lock_guard lock(m_mtx);
        init_queue(queue_id);
        get_queue_size(queue_id) = size;
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

    inline void init_queue(int queue_id)
    {
        auto&[vector, size] = std::get<0>(m_data_queues[queue_id]);
        vector.clear();
        size = DATA_PIT_MAX_QUEUE_SIZE;
    }

    inline std::vector<std::any>& get_queue(int queue_id)
    {
        return std::get<0>(m_data_queues.at(queue_id)).first;
    }

    inline size_t& get_queue_size(int queue_id)
    {
        return std::get<0>(m_data_queues.at(queue_id)).second;
    }

    inline std::string& get_type(int queue_id)
    {
        return std::get<1>(m_data_queues.at(queue_id));
    }

    inline std::mutex& get_mutex(int queue_id)
    {
        return std::get<2>(m_data_queues.at(queue_id));
    }

    inline std::condition_variable& get_cv(int queue_id)
    {
        return std::get<3>(m_data_queues.at(queue_id));
    }

    typedef std::tuple<std::pair<std::vector<std::any>, size_t>, std::string, std::mutex, std::condition_variable> data;

    std::unordered_map<int, data> m_data_queues;
    std::unordered_map<unsigned int, std::tuple<int, size_t>> m_consumer_indices;
    std::unordered_map<unsigned int, data_pit_error> m_consumer_last_error;
    std::mutex m_mtx;
    unsigned int m_next_consumer_id;
};