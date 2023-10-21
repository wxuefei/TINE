#pragma once
#include <stdlib.h>
#include "poopalloc.h"
#define TD_MALLOC(sz) calloc(1,(sz))
#define TD_CALLOC(n,sz) calloc((sz),(n))
#define TD_REALLOC(p,sz) realloc((p),(sz))
#define TD_FREE(p) free(p)
#include <string.h>
