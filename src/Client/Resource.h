#ifndef VS_FFI_FILEFLAGSMASK
#include "verrsrc.h"
#endif

#define IDI_CLIENT				100

#define IDS_TITLE				110

#define VER_FILEVERSION             0,0,0,1
#define VER_FILEVERSION_STR         "0.0.0.1\0"

#define VER_PRODUCTVERSION          0,0,0,1
#define VER_PRODUCTVERSION_STR      "0.0.0.1\0"

#ifndef DEBUG
#define VER_DEBUG                   0
#else
#define VER_DEBUG                   VS_FF_DEBUG
#endif

#if defined(_DEBUG)
#define VS_FFI_FILEFLAGS		(VS_FF_DEBUG | VS_FF_PRERELEASE | VS_FF_PRIVATEBUILD)
#else
#define VS_FFI_FILEFLAGS		(VS_FF_PRERELEASE | VS_FF_PRIVATEBUILD)
#endif
