#pragma once

#include "common/basic_types.h"

enum unittest_output
{
	unittest_output_invalid = 0,
	unittest_output_error,
	unittest_output_warn,
	unittest_output_normal,
	unittest_output_debug,
};

void TESTHDR(const char* format, ...);

void TESTBEGIN(const char* format, ...);
void TESTOUT(unittest_output type, const char* format, ...);
void TESTEND();

void TESTPARTBEGIN(const char* format, ...);
void TESTPARTEND();

void SUITEBEGIN(const char* format, ...);
void SUITEEND();

extern bool g_verbose;