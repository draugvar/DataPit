#pragma once

#include <list>
#include <condition_variable>
#include <optional>
#include <any>
#include <unordered_map>

#define DATA_PIT_MAX_QUEUE_SIZE 1000

class queue
{
private:
    template<typename T>
    bool produce(const T& data)
    {
        std::unique_lock lock(mutex);
        if (data_queue.size() >= max_size) return false;
        data_queue.push_back(data);
        cv.notify_all();
        return true;
    }

    template<typename T>
    std::optional<T> consume(const bool blocking, const uint32_t timeout_ms)
    {
        std::unique_lock lock(mutex);
        if (blocking)
        {
            if (!cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] { return !data_queue.empty(); }))
            {
                return std::nullopt;
            }
        }
        if (data_queue.empty()) return std::nullopt;
        try
        {
            T data = std::any_cast<T>(data_queue.front());
            data_queue.pop_front();
            return data;
        }
        catch ( const std::bad_any_cast&)
        {
            return std::nullopt;
        }
    }

    template<typename T>
    std::optional<T> item_by_position(const unsigned int position, const bool blocking, const uint32_t timeout_ms)
    {
        std::unique_lock lock(mutex);
        if (blocking)
        {
            if (!cv.wait_for(lock, std::chrono::milliseconds(timeout_ms), [&] { return position < data_queue.size(); }))
            {
                return std::nullopt;
            }
        }
        if (position >= data_queue.size()) return std::nullopt;
        auto it = data_queue.begin();
        std::advance(it, position);
        T data = std::any_cast<T>(*it);
        return data;
    }

    void set_max_size(const size_t size)
    {
        std::unique_lock lock(mutex);
        max_size = size;
    }

    void clear()
    {
        std::unique_lock lock(mutex);
        data_queue.clear();
    }
    
    std::list<std::any> data_queue;
    std::condition_variable cv;
    std::mutex mutex;
    size_t max_size = DATA_PIT_MAX_QUEUE_SIZE;

    friend class queue_handle;
    friend class data_pit;
};

class queue_handle
{
public:
    explicit queue_handle(std::shared_ptr<queue> q) : q(std::move(q)), peek_index(0) {}
    
    bool produce(const std::any& data) const
    {
        return q->produce(data);
    }

    template<typename T>
    std::optional<T> consume(const bool blocking = false, const uint32_t timeout_ms = std::numeric_limits<uint32_t>::max())
    {
        return q->consume<T>(blocking, timeout_ms);
    }

    template<typename T>
    std::optional<T> peek(const bool blocking = false, const uint32_t timeout_ms = std::numeric_limits<uint32_t>::max())
    {
        return q->item_by_position<T>(peek_index, blocking, timeout_ms);
    }

    void set_max_size(const size_t size) const
    {
        q->set_max_size(size);
    }

    void clear() const
    {
        q->clear();
    }

private:
    std::shared_ptr<queue> q;
    size_t peek_index;
};

class data_pit
{
public:
    std::shared_ptr<queue_handle> create_queue(const int queue_id)
    {
        auto q = std::make_shared<queue>();
        queues[queue_id] = q;
        return std::make_shared<queue_handle>(q);
    }

    void destroy_queue(const int queue_id)
    {
        queues.erase(queue_id);
    }

    std::shared_ptr<queue_handle> get_queue_handle(const int queue_id)
    {
        if (queues.contains(queue_id))
        {
            return std::make_shared<queue_handle>(queues[queue_id]);
        }
        return nullptr;
    }

private:
    std::unordered_map<int, std::shared_ptr<queue>> queues;
};