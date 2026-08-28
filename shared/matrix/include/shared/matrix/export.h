#pragma once

#ifdef _WIN32
#ifdef SHARED_MATRIX_EXPORTS
#define SHARED_MATRIX_API __declspec(dllexport)
#else
#define SHARED_MATRIX_API __declspec(dllimport)
#endif
#else
#define SHARED_MATRIX_API
#endif
