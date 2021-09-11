#pragma once
///////////////////////// Switchers
//#define VENGINE_REL_WITH_DEBUG_INFO
#define VENGINE_CLANG_COMPILER
#define NOMINMAX
///////////////////////// Clang
#ifdef VENGINE_CLANG_COMPILER
#define _XM_NO_INTRINSICS_
#define m128_f32 vector4_f32
#define m128_u32 vector4_u32
#include <cstdlib>
#define VENGINE_EXIT exit(1)
#else
#include <cstdlib>
#define VENGINE_EXIT throw 0
#endif
#ifndef UNICODE
#define UNICODE//Disable this in non-unicode system
#endif

#ifdef VENGINE_REL_WITH_DEBUG_INFO
#define DEBUG
#endif
#if defined(_DEBUG)
#define DEBUG
#endif

#pragma endregion


#define VENGINE_C_FUNC_COMMON
#define VENGINE_DLL_COMMON

/////////////////////// THREAD PAUSE
#include <stdint.h>

VENGINE_DLL_COMMON void* operator new(size_t n) noexcept;
VENGINE_DLL_COMMON void operator delete(void* p) noexcept;
#define VENGINE_EXTERN extern "C" _declspec(dllexport)