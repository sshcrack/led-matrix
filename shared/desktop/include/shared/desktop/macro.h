#pragma once

#ifdef _WIN32
  #ifdef SHARED_DESKTOP_EXPORTS
    #define SHARED_DESKTOP_API __declspec(dllexport)
  #else
    #define SHARED_DESKTOP_API __declspec(dllimport)
  #endif
#else
  #define SHARED_DESKTOP_API
#endif