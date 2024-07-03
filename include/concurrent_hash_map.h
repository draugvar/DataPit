/*
 *  concurrent_hash_map.h
 *  concurrent_hash_map
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

#include <unordered_map>
#include <mutex>
#include <shared_mutex>

/**
 * @brief A concurrent hash map.
 *
 * @tparam Key The key type.
 * @tparam Value The value type.
 */
template <typename Key, typename Value>
class concurrent_hash_map
{
public:
    /**
     * @brief           Erase a key-value pair into the map.
     *
     * @param key       The key.
     * @return          True if the key-value pair was erased, false otherwise.
     */
    bool erase(const Key& key)
    {
        std::unique_lock lock(mutex);
        return map.erase(key);
    }

    /**
     * @brief           Find a key-value pair into the map.
     *
     * @param key       The key.
     * @return          The value if the key was found, std::nullopt otherwise.
     */
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

    /**
     * @brief           Check if the map contains a key.
     *
     * @param key       The key.
     * @return          True if the key was found, false otherwise.
     */
    bool contains(const Key& key) const
    {
        std::shared_lock lock(mutex);
        return map.find(key) != map.end();
    }

    /**
     * @brief           Operator[] overload to access the map.
     *
     * @param key       The key.
     * @return          The value.
     */
    Value& operator[](const Key& key)
    {
        std::unique_lock lock(mutex);
        return map[key];
    }

    /**
     * @brief           Return a reference to the value associated with the key.
     *
     * @param key       The key.
     * @return          The value.
     */
    Value& at(const Key& key)
    {
        std::shared_lock lock(mutex);
        return map.at(key);
    }

    /**
     * @brief           Clear the map.
     */
    void clear()
    {
        std::unique_lock lock(mutex);
        map.clear();
    }

private:
    // The map
    std::unordered_map<Key, Value> map;
    // The mutex
    mutable std::shared_mutex mutex;
};