#include <stdlib.h>
#include <string.h>
#include <stdio.h>

typedef unsigned char uchar;
typedef unsigned int uint;
typedef unsigned long ulong;
typedef __int64 vlong;
typedef unsigned __int64 uvlong;


#define nil 0
#define sysfatal(x) abort()

/* disable various silly warnings */
#pragma warning( disable : 4018 4305 4244 4102 4761 4164 4769 )

#include "libsec_prefix.h"
