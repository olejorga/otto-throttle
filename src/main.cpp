// #include <XPLMPlugin.h>
// #include <cstring>

// PLUGIN_API int XPluginStart(char* outName, char* outSig, char* outDesc) {
//     std::strcpy(outName, "Otto Throttle");
//     std::strcpy(outSig, "com.github.otto-throttle");
//     std::strcpy(outDesc, "A PID based auto-throttle for X-Plane 12");
//     return 1;
// }

// PLUGIN_API void XPluginStop() {}
// PLUGIN_API int XPluginEnable() { return 1; }
// PLUGIN_API void XPluginDisable() {}
// PLUGIN_API void XPluginReceiveMessage(XPLMPluginID, int, void*) {}

// =====================================================================
// AutoThrottle.cpp
// A simple PID-based autothrottle plugin for X-Plane 12 (SDK 4.3.0)
// =====================================================================

#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <algorithm>

#include <XPLMPlugin.h>
#include <XPLMMenus.h>
#include <XPLMDataAccess.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>
#include <XPLMPlanes.h>

struct PIDConfig
{
    float kp = 0.05f;
    float ki = 0.02f;
    float kd = 0.01f;
    float update_hz = 10.0f;
};

static PIDConfig gCfg;
static const char* cCfgFileName = "otto-throttle.cfg";

static XPLMMenuID gMenuId = nullptr;
static int gMenuContainer = -1;
static int gEnableItemIdx = -1;
static bool gOttoThrottleEnabled = false;

static XPLMDataRef drTargetSpeedKts = nullptr; // sim/cockpit2/autopilot/airspeed_dial_kts
static XPLMDataRef drCurrentSpeedKts = nullptr; // sim/flightmodel/position/indicated_airspeed
static XPLMDataRef drNumEngines = nullptr; // sim/aircraft/engine/acf_num_engines
static XPLMDataRef drThrottleArray = nullptr; // sim/flightmodel/engine/ENGN_thro
static XPLMDataRef drThrottleSetting = nullptr; // sim/cockpit2/engine/actuators/throttle_ratio_all
static XPLMDataRef drOverrideThrottles = nullptr; // sim/operation/override/override_throttles

static float gIntegral = 0.0f;
static float gPrevError = 0.0f;

static const int cMaxEngines = 8;

static void LoadConfig();
static void MenuHandler(void *inMenuRef, void *inItemRef);
static float FlightLoopCallback(float elapsedMe, float elapsedSim, int counter, void *refcon);
static std::string GetAircraftDirectory();
static std::string GetPluginDirectory();
static void EnableOttoThrottle(bool enable);
static void RefreshMenuCheckmark();

PLUGIN_API int XPluginStart(char *outName, char *outSig, char *outDesc)
{
    strcpy(outName, "Otto Throttle");
    strcpy(outSig, "com.github.otto-throttle");
    strcpy(outDesc, "A adjustable PID based auto-throttle for X-Plane 12");

    // XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    
    LoadConfig();

    drTargetSpeedKts = XPLMFindDataRef("sim/cockpit2/autopilot/airspeed_dial_kts");
    drCurrentSpeedKts = XPLMFindDataRef("sim/flightmodel/position/indicated_airspeed");
    drThrottleSetting = XPLMFindDataRef("sim/cockpit2/engine/actuators/throttle_ratio_all");
    drNumEngines = XPLMFindDataRef("sim/aircraft/engine/acf_num_engines");
    drThrottleArray = XPLMFindDataRef("sim/flightmodel/engine/ENGN_thro");
    drOverrideThrottles = XPLMFindDataRef("sim/operation/override/override_throttles");

    if (!drTargetSpeedKts || !drCurrentSpeedKts || !drThrottleSetting || !drNumEngines || !drThrottleArray || !drOverrideThrottles)
    {
        XPLMDebugString("[OttoThrottle] ERROR: one or more required datarefs not found.\n");
    }

    gMenuContainer = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Otto Throttle", nullptr, 0);
    gMenuId = XPLMCreateMenu("Otto Throttle", XPLMFindPluginsMenu(), gMenuContainer, MenuHandler, nullptr);

    gEnableItemIdx = XPLMAppendMenuItem(gMenuId, "Enable Otto Throttle", (void*)"toggle_at", 0);
    XPLMAppendMenuItem(gMenuId, "Reinflate Otto", (void*)"reload_cfg", 0);

    RefreshMenuCheckmark();

    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, nullptr);

    return 1;
}

PLUGIN_API void XPluginStop(void)
{
    XPLMUnregisterFlightLoopCallback(FlightLoopCallback, nullptr);

    if (gMenuId)
    {
        XPLMDestroyMenu(gMenuId);
    }
}

PLUGIN_API int XPluginEnable(void)
{
    return 1;
}

PLUGIN_API void XPluginDisable(void)
{
    EnableOttoThrottle(false);
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void *inParam)
{
    (void)inFrom;

    if (inMsg == XPLM_MSG_PLANE_LOADED)
    {
        if (reinterpret_cast<intptr_t>(inParam) == 0)
        {
            LoadConfig();
            XPLMDebugString("[AutoThrottle] Aircraft changed, config reloaded.\n");
        }
    }
}

static void MenuHandler(void *inMenuRef, void *inItemRef)
{
    (void)inMenuRef;
    const char *item = static_cast<const char *>(inItemRef);

    if (strcmp(item, "toggle_at") == 0)
    {
        EnableOttoThrottle(!gOttoThrottleEnabled);
    }
    else if (strcmp(item, "reload_cfg") == 0)
    {
        LoadConfig();
        XPLMDebugString("[OttoThrottle] Config reloaded.\n");
    }
}

static void RefreshMenuCheckmark()
{
    if (gMenuId && gEnableItemIdx >= 0)
    {
        XPLMCheckMenuItem(gMenuId, gEnableItemIdx, gOttoThrottleEnabled ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    }
}

static void EnableOttoThrottle(bool enable)
{
    gOttoThrottleEnabled = enable;

    // Reset PID state whenever we (re)engage, to avoid a kick from
    // stale integrator/derivative values.
    gIntegral   = 0.0f;
    gPrevError = 0.0f;

    if (drOverrideThrottles)
    {
        XPLMSetDatai(drOverrideThrottles, enable ? 1 : 0);
    }

    if (!enable)
    {
        // Leave throttles wherever they are; the sim resumes normal
        // (joystick/manual) control since the override flag is cleared.
    }

    RefreshMenuCheckmark();

    XPLMDebugString(enable ? "[OttoThrottle] Engaged.\n" : "[OttoThrottle] Disengaged.\n");
}

static float FlightLoopCallback(float elapsedMe, float elapsedSim, int counter, void *refcon)
{
    (void)elapsedSim;
    (void)counter;
    (void)refcon;

    const float interval = 1.0f / gCfg.update_hz;

    if (!gOttoThrottleEnabled || !drTargetSpeedKts || !drCurrentSpeedKts || !drThrottleSetting)
    {
        return interval;
    }

    float target_kts  = XPLMGetDataf(drTargetSpeedKts);
    float current_kts = XPLMGetDataf(drCurrentSpeedKts);

    // If the MCP speed bug is at 0 (not set), don't fight the pilot.
    // if (target_kts <= 0.0f)
    // {
    //     return interval;
    // }

    float dt = elapsedMe > 0.0f ? elapsedMe : interval;

    float error = target_kts - current_kts;

    gIntegral += error * dt;

    float derivative = (error - gPrevError) / dt;

    gPrevError = error;

    float output = gCfg.kp * error + gCfg.ki * gIntegral + gCfg.kd * derivative;

    // float current_throttle = XPLMGetDataf(drThrottleSetting);
    // float new_throttle = std::max(0.0f, std::min(1.0f, current_throttle + output));
    
    // XPLMSetDataf(drThrottleSetting, new_throttle);

    float throttle = std::max(0.0f, std::min(1.0f, output));

    int num_engines = drNumEngines ? XPLMGetDatai(drNumEngines) : cMaxEngines;
    if (num_engines <= 0 || num_engines > cMaxEngines)
    {
        num_engines = cMaxEngines;
    }

    float throttle_values[cMaxEngines];
    XPLMGetDatavf(drThrottleArray, throttle_values, 0, cMaxEngines);

    for (int i = 0; i < num_engines; ++i)
    {
        throttle_values[i] = throttle;
    }

    XPLMSetDatavf(drThrottleArray, throttle_values, 0, num_engines);
    
    return interval; // reschedule at fixed rate
}

// =====================================================================
// Config file loading
// =====================================================================
static std::string Trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos) return "";
    size_t b = s.find_last_not_of(" \t\r\n");
    return s.substr(a, b - a + 1);
}

static std::string GetAircraftDirectory()
{
    char acf_file[256] = {0};
    char acf_path[512] = {0};

    // Index 0 = the user's aircraft (as opposed to AI/multiplayer planes)
    XPLMGetNthAircraftModel(0, acf_file, acf_path);

    std::string full(acf_path);
    if (full.empty())
        return "";

    char sep = XPLMGetDirectorySeparator()[0];
    size_t last = full.find_last_of(sep);
    if (last == std::string::npos)
        return "";

    return full.substr(0, last); // strip the .acf filename, keep the directory
}

static std::string GetPluginDirectory()
{
    char path[512];
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, path, nullptr, nullptr);

    std::string full(path);
    char sep = XPLMGetDirectorySeparator()[0];

    // full path points at the plugin binary, e.g.:
    //   .../plugins/AutoThrottle/64/AutoThrottle.xpl
    // Strip the filename, then the platform folder, to get the plugin root.
    size_t last = full.find_last_of(sep);
    std::string dir = (last == std::string::npos) ? full : full.substr(0, last);

    size_t prev = dir.find_last_of(sep);
    std::string parentDir = (prev == std::string::npos) ? dir : dir.substr(0, prev);

    return parentDir; // plugin root (…/AutoThrottle)
}

static bool TryLoadFrom(const std::string &fullPath)
{
    std::ifstream file(fullPath.c_str());
    if (!file.is_open())
        return false;

    std::string line;
    while (std::getline(file, line))
    {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;

        size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));
        if (val.empty())
            continue;

        float fval = static_cast<float>(atof(val.c_str()));

        if (key == "kp")        gCfg.kp = fval;
        else if (key == "ki")   gCfg.ki = fval;
        else if (key == "kd")   gCfg.kd = fval;
        else if (key == "hz")   gCfg.update_hz = fval;
    }

    return true;
}

static void LoadConfig()
{
    char sep = XPLMGetDirectorySeparator()[0];

    // 1) Aircraft directory: …/Aircraft/<SomePlane>/autothrottle.cfg
    std::string acfDir = GetAircraftDirectory();
    if (!acfDir.empty())
    {
        std::string candidateAcf = acfDir + sep + cCfgFileName;
        if (TryLoadFrom(candidateAcf))
        {
            XPLMDebugString(("[AutoThrottle] Loaded config: " + candidateAcf + "\n").c_str());
            return;
        }
    }

    // 2) Plugin root directory (fallback): …/AutoThrottle/autothrottle.cfg
    std::string root = GetPluginDirectory();
    std::string candidate1 = root + sep + cCfgFileName;

    // 3) Same directory as the binary (fallback): …/AutoThrottle/64/autothrottle.cfg
    char path[512];
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, path, nullptr, nullptr);
    std::string binDir(path);
    size_t last = binDir.find_last_of(sep);
    std::string candidate2 = (last == std::string::npos)
        ? std::string(cCfgFileName)
        : binDir.substr(0, last) + sep + cCfgFileName;

    if (TryLoadFrom(candidate1))
    {
        XPLMDebugString(("[AutoThrottle] Loaded config: " + candidate1 + "\n").c_str());
        return;
    }
    if (TryLoadFrom(candidate2))
    {
        XPLMDebugString(("[AutoThrottle] Loaded config: " + candidate2 + "\n").c_str());
        return;
    }

    char msg[256];
    snprintf(msg, sizeof(msg),
              "[AutoThrottle] No config file found, using defaults "
              "(kp=%.4f ki=%.4f kd=%.4f)\n",
              gCfg.kp, gCfg.ki, gCfg.kd);
    XPLMDebugString(msg);
}
