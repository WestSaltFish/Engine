//
// platform.h : This file contains basic platform types and tools. Also, it exposes
// the necessary functions for the Engine to communicate with the Platform layer.
//

#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include <stdio.h>
#include <assert.h>
#include <math.h>
#include <vector>
#include <string>
#include "Globals.h"

#pragma warning(disable : 4267) // conversion from X to Y, possible loss of data

String MakeString(const char *cstr);

String MakePath(String dir, String filename);

String GetDirectoryPart(String path);

/**
 * Reads a whole file and returns a string with its contents. The returned string
 * is temporary and should be copied if it needs to persist for several frames.
 */
String ReadTextFile(const char *filepath);

/**
 * It retrieves a timestamp indicating the last time the file was modified.
 * Can be useful in order to check for file modifications to implement hot reloads.
 */
u64 GetFileLastWriteTimestamp(const char *filepath);

/**
 * It logs a string to whichever outputs are configured in the platform layer.
 * By default, the string is printed in the output console of VisualStudio.
 */
void LogString(const char* str);

#define ILOG(...)                 \
{                                 \
char logBuffer[1024] = {};        \
sprintf(logBuffer, __VA_ARGS__);  \
LogString(logBuffer);             \
}

#define ELOG(...) ILOG(__VA_ARGS__)

#define ARRAY_COUNT(array) (sizeof(array)/sizeof(array[0]))

#define ASSERT(condition, message) assert((condition) && message)

#define KB(count) (1024*(count))
#define MB(count) (1024*KB(count))
#define GB(count) (1024*MB(count))

#define PI  3.14159265359f
#define TAU 6.28318530718f

