#pragma once
#include <unordered_map>
#include <mutex>
#include <shared_mutex>

template <typename Key, typename Value>
class concurrent_unordered_map
{
public:
    void insert(const Key& key, const Value& value)
    {
        std::unique_lock lock(mutex);
        map[key] = value;
    }

    bool erase(const Key& key)
    {
        std::unique_lock lock(mutex);
        return map.erase(key);
    }

    std::optional<Value> find(const Key& key) const
    {
        std::shared_lock lock(mutex);
        auto it = map.find(key);
        if (it != map.end())
        {
            return it->second;
        }
        else
        {
            return std::nullopt;
        }
    }

    bool contains(const Key& key) const
    {
        std::shared_lock lock(mutex);
        return map.find(key) != map.end();
    }

    Value& operator[](const Key& key)
    {
        std::unique_lock lock(mutex);
        return map[key];
    }

    Value& at(const Key& key)
    {
        std::shared_lock lock(mutex);
        try
        {
            return map.at(key);
        }
        catch (const std::out_of_range& e) {
            throw std::out_of_range("Key not found");
        }
        catch (const std::exception& e)
        {
            std::cout << "Caught an exception of an unexpected type: " << e.what() << std::endl;
            throw std::exception();
        }
    }

    void clear()
    {
        std::unique_lock lock(mutex);
        map.clear();
    }

private:
    std::unordered_map<Key, Value> map;
    mutable std::shared_mutex mutex;
};