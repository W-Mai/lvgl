# LVGL Cache Framework Documentation

This documentation provides a comprehensive guide to the LVGL Cache Framework, a flexible and efficient caching system designed for resource management in embedded graphics systems.

## Table of Contents

1. [Overview](overview_en.md) - Introduction to the LVGL Cache Framework
2. [Architecture](architecture_en.md) - Detailed architecture design and core components
3. [API Reference](api_reference_en.md) - Complete API documentation
4. [Implementation Details](implementation_details_en.md) - In-depth explanation of implementation
5. [Performance Analysis](performance_en.md) - Performance considerations and benchmarks

## Introduction

The LVGL Cache Framework is a general-purpose, efficient caching system designed to optimize resource usage and improve graphics rendering performance. The framework adopts a modular design, supports multiple caching strategies and eviction algorithms, and is primarily used for image resource cache management, but its design is flexible enough to extend to other types of resource caching.

Key features include:

- **Generality**: Can be used for various types of resource caching, not limited to images
- **Efficiency**: Uses a combination of red-black trees and linked lists for efficient lookup and LRU management
- **Thread Safety**: Ensures safe access in multi-threaded environments through mutex lock mechanisms
- **Reference Counting**: Uses a reference counting mechanism to ensure resources are not released while in use
- **Extensibility**: Supports custom caching strategies and eviction algorithms through the cache class interface

## Getting Started

To start using the LVGL Cache Framework, refer to the [Overview](overview_en.md) section for a general introduction, followed by the [Architecture](architecture_en.md) section to understand the design principles. For implementation details, check the [Implementation Details](implementation_details_en.md) section.

For API documentation, see the [API Reference](api_reference_en.md) section.