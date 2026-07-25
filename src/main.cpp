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
    float update_hz = 60.0f;
};

static PIDConfig gCfg;
static const char* cCfgFileName = "otto-throttle.cfg";

static XPLMMenuID gMenuId = nullptr;
static int gMenuContainer = -1;
static int gEnableItemIdx = -1;
static bool gOttoThrottleEnabled = false;

static XPLMDataRef drTargetSpeedKts = nullptr;
static XPLMDataRef drCurrentSpeedKts = nullptr;
static XPLMDataRef drThrottleSetting = nullptr;
static XPLMDataRef drOverrideThrottles = nullptr;

static float gIntegral = 0.0f;
static float gPrevError = 0.0f;

static const int cMaxEngines = 8;

static void LoadConfig();
static void MenuHandler(void *inMenuRef, void *inItemRef);
static float FlightLoopCallback(float elapsedMe, float elapsedSim, int counter, void *refcon);
static std::string GetAircraftDirectory();
static void EnableOttoThrottle(bool enable);
static void RefreshMenuCheckmark();

PLUGIN_API int XPluginStart(char *outName, char *outSig, char *outDesc)
{
    strcpy(outName, "Otto Throttle");
    strcpy(outSig, "com.github.otto-throttle");
    strcpy(outDesc, "A adjustable PID based auto-throttle for X-Plane 12");
    
    LoadConfig();

    drTargetSpeedKts = XPLMFindDataRef("sim/cockpit2/autopilot/airspeed_dial_kts");
    drCurrentSpeedKts = XPLMFindDataRef("sim/flightmodel/position/indicated_airspeed");
    drThrottleSetting = XPLMFindDataRef("sim/cockpit2/engine/actuators/throttle_ratio_all");
    drOverrideThrottles = XPLMFindDataRef("sim/operation/override/override_throttles");

    if (!drTargetSpeedKts || !drCurrentSpeedKts || !drThrottleSetting || !drOverrideThrottles)
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
    gIntegral   = 0.0f;
    gPrevError = 0.0f;

    if (drOverrideThrottles)
    {
        XPLMSetDatai(drOverrideThrottles, enable ? 1 : 0);
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

    float dt = elapsedMe > 0.0f ? elapsedMe : interval;

    float error = target_kts - current_kts;
    gIntegral += error * dt;

    float derivative = (error - gPrevError) / dt;
    gPrevError = error;

    float output = gCfg.kp * error + gCfg.ki * gIntegral + gCfg.kd * derivative;

    float current_throttle = XPLMGetDataf(drThrottleSetting);
    float new_throttle = std::max(0.0f, std::min(1.0f, current_throttle + output));
    
    XPLMSetDataf(drThrottleSetting, new_throttle);
    
    return interval;
}

static std::string Trim(const std::string &s)
{
    size_t a = s.find_first_not_of(" \t\r\n");
    if (a == std::string::npos)
    {
        return "";
    }

    size_t b = s.find_last_not_of(" \t\r\n");

    return s.substr(a, b - a + 1);
}

static std::string GetAircraftDirectory()
{
    char acf_file[256] = {0};
    char acf_path[512] = {0};

    XPLMGetNthAircraftModel(0, acf_file, acf_path);

    std::string full(acf_path);
    if (full.empty())
    {
        return "";
    }

    char sep = XPLMGetDirectorySeparator()[0];
    size_t last = full.find_last_of(sep);
    if (last == std::string::npos)
    {
        return "";
    }

    return full.substr(0, last);
}

static bool TryLoadFrom(const std::string &fullPath)
{
    std::ifstream file(fullPath.c_str());
    if (!file.is_open())
    {
        return false;
    }

    std::string line;
    while (std::getline(file, line))
    {
        line = Trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
        {
            continue;
        }

        size_t eq = line.find('=');
        if (eq == std::string::npos)
        {
            continue;
        }

        std::string key = Trim(line.substr(0, eq));
        std::string val = Trim(line.substr(eq + 1));
        if (val.empty())
        {
            continue;
        }

        float fval = static_cast<float>(atof(val.c_str()));

        if (key == "kp")
        {
            gCfg.kp = fval;
        }
        else if (key == "ki")
        {
            gCfg.ki = fval;
        }
        else if (key == "kd")
        {
            gCfg.kd = fval;
        }
        else if (key == "hz")
        {
            gCfg.update_hz = fval;
        }
    }

    return true;
}

static void LoadConfig()
{
    char sep = XPLMGetDirectorySeparator()[0];

    std::string acfDir = GetAircraftDirectory();
    if (!acfDir.empty())
    {
        std::string candidateAcf = acfDir + sep + cCfgFileName;
        if (TryLoadFrom(candidateAcf))
        {
            XPLMDebugString(("[OttoThrottle] Loaded config: " + candidateAcf + "\n").c_str());
            return;
        }
    }

    char msg[256];
    snprintf(msg, sizeof(msg), "[OttoThrottle] No config file found, using defaults (kp=%.4f ki=%.4f kd=%.4f)\n", gCfg.kp, gCfg.ki, gCfg.kd);
    XPLMDebugString(msg);
}
