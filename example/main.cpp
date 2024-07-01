/*
 *  main.cpp
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

#include "data_pit.h" // Include the header file for DataPit
#include <iostream>

int main()
{
    data_pit dp; // Create an instance of DataPit
    int queue_id = 1; // The queue ID

    // Register a consumer and get its ID
    unsigned int consumer_id = dp.register_consumer(queue_id);
    std::cout << "Registered consumer with ID: " << consumer_id << std::endl;

    // Produce some data
    for(int i = 0; i < 10; ++i)
    {
        dp.produce(queue_id, i);
        std::cout << "Produced data: " << i << std::endl;
    }

    // Consume the data
    for(int i = 0; i < 10; ++i)
    {
        std::optional<int> data = dp.consume<int>(consumer_id);
        if(data.has_value())
        {
            std::cout << "Consumed data: " << data.value() << std::endl;
        }
        else
        {
            std::cout << "No data available to consume." << std::endl;
        }
    }

    // Unregister the consumer
    dp.unregister_consumer(consumer_id);
    std::cout << "Unregistered consumer with ID: " << consumer_id << std::endl;

    return 0;
}