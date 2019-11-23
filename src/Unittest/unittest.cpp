#include "pch.h"

#include <iostream>

#include "common/string.h"

#include "unittest_common.h"

#include "unittest.h"

bool g_verbose;
u32 g_testNum = 0;
u32 g_testCount;
u32 g_testPartCount;
u32 g_errorCount;
u32 g_errorPartCount;
u32 g_lineCount;
bool g_isFreshLine;
bool g_isInTestPart = false;

int main(int argc, char* argv[])
{
	g_verbose = false;
	g_isFreshLine = true;

	for (int argI = 1; argI < argc; ++argI)
	{
		const char* arg = argv[argI];
		if (_strnicmp(arg, "-v", 2) == 0)
		{
			g_verbose = true;
		}
		if ((_strnicmp(arg, "-t", 2) == 0) && ((argI + 1) < argc))
		{
			g_testNum = static_cast<u32>(atol(argv[argI + 1]));
			++argI;
		}
        
        if (_strnicmp(arg, "-d", 2) == 0)
        {
            printf("\x1b[34mPausing for 15s, attach debugger now.\x1b[0m\n");
            Sleep(15*1000);
        }
	}

	TESTHDR("Staring unit tests ...");

	if (g_testNum == 0)
	{
		unittest_common();
	}
	else
	{
		switch (g_testNum)
		{
		case 1:
			unittest_common();
			break;
		default:
			break;
		}
	}
}

void EnsureFreshLine()
{
	if (g_isFreshLine)
		return;

	printf("\n");
	g_isFreshLine = true;
}

void TESTHDR(const char* format, ...)
{
	printf("\x1b[34m");
	va_list argptr;
	va_start(argptr, format);
	vprintf(format, argptr);
	va_end(argptr);
	printf("\x1b[0m\n");
	g_isFreshLine = true;

	std::cout.flush();
}

void SUITEBEGIN(const char* format, ...)
{
	g_testCount = 0;

	va_list argptr;
	va_start(argptr, format);
	vprintf(format, argptr);
	va_end(argptr);

	printf("\n");
	g_isFreshLine = true;

	std::cout.flush();
}

void SUITEEND()
{
}

void TESTBEGIN(const char* format, ...)
{
	g_errorCount = 0;
	g_errorPartCount = 0;
	g_lineCount = 0;
	g_testPartCount = 0;
	g_isInTestPart = false;

	EnsureFreshLine();

	printf("  %d) ", ++g_testCount);

	va_list argptr;
	va_start(argptr, format);
	vprintf(format, argptr);
	va_end(argptr);

	printf(" ... ");
	g_isFreshLine = false;

	std::cout.flush();
}

void TESTEND()
{
	if (g_isFreshLine)
	{
		printf("      ");
	}

	if (g_errorCount > 0)
	{
		printf("\x1b[41mFailed\x1b[0m\n");
	}
	else
	{
		printf("\x1b[32mSucceeded\x1b[0m\n");
	}
	g_isFreshLine = true;

	std::cout.flush();
}

void TESTPARTBEGIN(const char* format, ...)
{
	if (g_isInTestPart)
	{
		TESTPARTEND();
	}

	EnsureFreshLine();

	++g_testPartCount;
	std::string part;
	u32 testPartCount = g_testPartCount - 1;
	while (true)
	{
		const u32 v = testPartCount % 26;
		part.insert(part.begin(), 'a' + v);
		if (testPartCount < 26)
			break;
		testPartCount = (testPartCount / 26) - 1;
	}

	printf("    %s) ", part.c_str());

	va_list argptr;
	va_start(argptr, format);
	vprintf(format, argptr);
	va_end(argptr);

	printf(" ... ");
	g_isFreshLine = false;

	std::cout.flush();

	g_lineCount = 0;
	g_errorPartCount = 0;
	g_isInTestPart = true;
}

void TESTPARTEND()
{
	if (g_isFreshLine)
	{
		printf("      ");
	}

	if (g_errorPartCount > 0)
	{
		printf("\x1b[41mFailed\x1b[0m\n");
	}
	else
	{
		printf("\x1b[32mSucceeded\x1b[0m\n");
	}
	g_isFreshLine = true;

	std::cout.flush();

	g_isInTestPart = false;
}

void TESTOUT(unittest_output type, const char* format, ...)
{
	if (type == unittest_output_debug && !g_verbose)
		return;

	EnsureFreshLine();

	printf("    ");

	bool revertColor = false;
	if (type == unittest_output_error)
	{
		printf("\x1b[41m");
		revertColor = true;
	}
	else if (type == unittest_output_warn)
	{
		printf("\x1b[33m");
		revertColor = true;
	}

	va_list argptr;
	va_start(argptr, format);
	vprintf(format, argptr);
	va_end(argptr);

	if (type == unittest_output_error)
	{
		++g_errorCount;
		++g_errorPartCount;
	}

	if (revertColor)
	{
		printf("\x1b[0m");
	}

	printf("\n");
	g_isFreshLine = true;

	std::cout.flush();

	++g_lineCount;
}
