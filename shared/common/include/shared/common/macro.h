#pragma once

#ifdef _WIN32
  #ifdef SHARED_COMMON_EXPORTS
    #define SHARED_COMMON_API __declspec(dllexport)
  #else
    #define SHARED_COMMON_API __declspec(dllimport)
  #endif
#else
  #define SHARED_COMMON_API
#endif