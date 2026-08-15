#include "pch.h"

#include "signatures.h"

extern PFN_CreateDXGIFactory OriginalCreateDXGIFactory;
extern PFN_CreateDXGIFactory1 OriginalCreateDXGIFactory1;
extern PFN_CreateDXGIFactory2 OriginalCreateDXGIFactory2;
extern PFN_DXGIDeclareAdapterRemovalSupport OriginalDXGIDeclareAdapterRemovalSupport;
extern PFN_DXGIGetDebugInterface1 OriginalDXGIGetDebugInterface1;

extern "C"
{
    HRESULT ProxyCreateDXGIFactory(REFIID riid, void** ppFactory)
    {
        return OriginalCreateDXGIFactory(riid, ppFactory);
    }
    HRESULT ProxyCreateDXGIFactory1(REFIID riid, void** ppFactory)
    {
        return OriginalCreateDXGIFactory1(riid, ppFactory);
    }
    HRESULT ProxyCreateDXGIFactory2(UINT Flags, REFIID riid, void** ppFactory)
    {
        return OriginalCreateDXGIFactory2(Flags, riid, ppFactory);
    }
    HRESULT ProxyDXGIDeclareAdapterRemovalSupport()
    {
        return OriginalDXGIDeclareAdapterRemovalSupport();
    }
    HRESULT ProxyDXGIGetDebugInterface1(UINT Flags, REFIID riid, void** pDebug)
    {
        return OriginalDXGIGetDebugInterface1(Flags, riid, pDebug);
    }
}
