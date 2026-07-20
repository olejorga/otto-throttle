#include <XPLMPlugin.h>
#include <cstring>

PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
    std::strcpy(outName, "Otto Throttle");
    std::strcpy(outSig, "com.github.otto-throttle");
    std::strcpy(outDesc, "A PID based auto-throttle for X-Plane 12");
    return 1;
}

PLUGIN_API void XPluginStop() {}
PLUGIN_API int XPluginEnable() { return 1; }
PLUGIN_API void XPluginDisable() {}
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int, void*) {}
