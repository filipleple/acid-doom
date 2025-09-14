#pragma once
#include "doomtype.h"

void PP_BoxBlur8(byte *buf, int w, int h, int stride, int radius);
void PP_Fog8(byte *buf, int w, int h, int stride, byte fog_index, int strength);
void ApplyPost(byte *video); // convenience wrapper
