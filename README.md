# DataPit

data_pit is a versatile and efficient synchronization data structure designed for scenarios where a producer inserts data
and multiple consumers retrieve it. Ideal for concurrent programming, data_pit ensures thread-safe operations and 
seamless data flow between producers and consumers.

## Features

- **Thread-Safe**: Built to handle concurrent access with robust synchronization mechanisms.
- **Flexible Usage**: Suitable for various use cases including task dispatching, message passing, and data buffering.
- **Scalable**: Supports multiple producers and consumers, making it perfect for high-load environments.
- **Blocking and Non-Blocking Operations**: Choose the best approach for your needs with support for both blocking and non-blocking data retrieval.

## Known Bugs

- Multithreading was not tested extensively. Please report any issues you encounter.

## Getting Started

To use data_pit in your C++ project, include the `data_pit.h` header file in your source code.

```cpp
#include "data_pit.h"
```

## Usage

Here is a basic example of how to use data_pit:

```cpp
// Create a data_pit instance
data_pit dp;

// Register a consumer and get its ID
unsigned int consumer_id = dp.register_consumer(0);
std::cout << "Registered consumer with ID: " << consumer_id << std::endl;

// Produce some data
std::string data_string = "Hello, World!";
dp.produce(0, data_string);

// Consume the data
auto data = dp.consume<std::string>(consumer_id);
if(data.has_value())
std::cout << "Consumed data: " << data.value() << std::endl;

// Unregister the consumer
dp.unregister_consumer(consumer_id);
```

## Version

- Current version: 1.0.0

## Contributing

Contributions are welcome! Please read the contributing guide to get started.

## License

This project is licensed under the MIT License. See the LICENSE file for details.
