#include <XPLMPlugin.h>
#include <cstring>

extern "C" {

PLUGIN_API int XPluginStart(char* outName,
                            char* outSig,
                            char* outDesc)
{
    std::strcpy(outName, "Otto Throttle");
    std::strcpy(outSig, "com.example.otto-throttle");
    std::strcpy(outDesc, "This is version 3.0");
    return 1;
}

PLUGIN_API void XPluginStop() {}

PLUGIN_API int XPluginEnable() { return 1; }

PLUGIN_API void XPluginDisable() {}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID,
                                      int,
                                      void*)
{
}

}
