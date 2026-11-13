#pragma once
#include <stdio.h>
#include <stdlib.h>
#define OLED_CHECK(x) do {if(!(x)){fprintf(stderr,"FAIL %s:%d %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
