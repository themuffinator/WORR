#pragma once

#ifndef _WIN32
#include <strings.h>

#ifndef _stricmp
#define _stricmp strcasecmp
#endif

#ifndef _strnicmp
#define _strnicmp strncasecmp
#endif

#endif
