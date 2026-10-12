#include "DrawListShaders.h"

IMPLEMENT_GLOBAL_SHADER(FScreenDrawListVS, "/Weltall/Private/ScreenDrawList.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FScreenDrawListPS, "/Weltall/Private/ScreenDrawList.usf", "MainPS", SF_Pixel);
