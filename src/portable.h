// File:  portable.h
// Date:  5/26/2015
// Auth:  K. Loux
// Desc:  Type definitions for writing portable code.

#ifndef PORTABLE_H_
#define PORTABLE_H_

#ifdef _WIN32

typedef unsigned long long ULongLong;
typedef long long LongLong;

#else
#include <stdint.h>

typedef uint64_t ULongLong;
typedef int64_t LongLong;


#endif

#endif// PORTABLE_H_