#include "FieldSceneShaders.h"

IMPLEMENT_GLOBAL_SHADER(FFieldSceneVS, "/Weltall/Private/FieldScene.usf", "MainVS", SF_Vertex);
IMPLEMENT_GLOBAL_SHADER(FFieldScenePS, "/Weltall/Private/FieldScene.usf", "MainPS", SF_Pixel);
IMPLEMENT_GLOBAL_SHADER(FFieldSceneResolvePS, "/Weltall/Private/FieldScene.usf", "ResolvePS", SF_Pixel);
