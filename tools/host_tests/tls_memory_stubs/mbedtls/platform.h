#pragma once
#include <stddef.h>
int mbedtls_platform_set_calloc_free(void *(*)(size_t,size_t),void(*)(void *));
