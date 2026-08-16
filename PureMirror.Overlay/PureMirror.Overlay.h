#pragma once

#ifdef PUREMIRROROVERLAY_EXPORTS
#define PUREMIRROROVERLAY_API __declspec(dllexport)
#else
#define PUREMIRROROVERLAY_API __declspec(dllimport)
#endif

#include <OverlayApi.h>

extern "C"
{
    PUREMIRROROVERLAY_API OverlayAPI* GetOverlayAPI();
}
