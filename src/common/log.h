// Enable toggle logging via cmake
#pragma once

#if defined(TINYKV_ENABLE_LOGGING) && TINYKV_ENABLE_LOGGING
#include <iostream>
#define TINYKV_LOG(x)           \
    do                          \
    {                           \
        std::cout << x << "\n"; \
    } while (0)
#else
#define TINYKV_LOG(x) \
    do                \
    {                 \
    } while (0)
#endif
