#ifdef PUREMIRRORCORE_EXPORTS
#define PUREMIRRORCORE_API __declspec(dllexport)
#else
#define PUREMIRRORCORE_API __declspec(dllimport)
#endif

#include <CoreApi.h>

extern "C"
{
    PUREMIRRORCORE_API CoreAPI* GetCoreAPI();
}
