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

#include "XPLMPlugin.h"
#include "XPLMMenus.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"
#include "XPLMPlanes.h"

// ---------------------------------------------------------------------
// Config / tuning
// ---------------------------------------------------------------------
struct PIDConfig
{
    float kp = 0.05f;
    float ki = 0.02f;
    float kd = 0.01f;
    float update_hz = 10.0f;   // how often the flight loop runs
};

static PIDConfig g_cfg;

static const char *kConfigFileName = "otto-throttle.cfg";

// ---------------------------------------------------------------------
// Plugin / menu state
// ---------------------------------------------------------------------
static XPLMMenuID g_menu_id         = nullptr;
static int        g_menu_container  = -1;
static int        g_enable_item_idx = -1;
static bool       g_at_enabled      = false;

// ---------------------------------------------------------------------
// Datarefs
// ---------------------------------------------------------------------
static XPLMDataRef dr_target_speed_kts   = nullptr; // sim/cockpit2/autopilot/airspeed_dial_kts
static XPLMDataRef dr_current_speed_kts  = nullptr; // sim/flightmodel/position/indicated_airspeed
static XPLMDataRef dr_throttle_setting   = nullptr; // sim/cockpit2/engine/actuators/throttle_ratio_all
static XPLMDataRef dr_override_throttles = nullptr; // sim/operation/override/override_throttles

// ---------------------------------------------------------------------
// PID state
// ---------------------------------------------------------------------
static float g_integral   = 0.0f;
static float g_prev_error = 0.0f;

// ---------------------------------------------------------------------
// Forward declarations
// ---------------------------------------------------------------------
static void        LoadConfig();
static void        MenuHandler(void *inMenuRef, void *inItemRef);
static float       FlightLoopCallback(float elapsedMe, float elapsedSim, int counter, void *refcon);
static std::string GetAircraftDirectory();
static std::string GetPluginDirectory();
static void        EnableAutoThrottle(bool enable);
static void        RefreshMenuCheckmark();

// =====================================================================
// XPluginStart
// =====================================================================
PLUGIN_API int XPluginStart(char *outName, char *outSig, char *outDesc)
{
    strcpy(outName, "Otto Throttle");
    strcpy(outSig, "com.github.otto-throttle");
    strcpy(outDesc, "A adjustable PID based auto-throttle for X-Plane 12");

    // XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);

    // ---- Load PID gains from config file --------------------------
    LoadConfig();

    // ---- Resolve datarefs ------------------------------------------
    dr_target_speed_kts   = XPLMFindDataRef("sim/cockpit2/autopilot/airspeed_dial_kts");
    dr_current_speed_kts  = XPLMFindDataRef("sim/flightmodel/position/indicated_airspeed");
    dr_throttle_setting   = XPLMFindDataRef("sim/cockpit2/engine/actuators/throttle_ratio_all");
    dr_override_throttles = XPLMFindDataRef("sim/operation/override/override_throttles");

    if (!dr_target_speed_kts || !dr_current_speed_kts ||
        !dr_throttle_setting   || !dr_override_throttles)
    {
        XPLMDebugString("[OttoThrottle] ERROR: one or more required datarefs not found.\n");
    }

    // ---- Build "Plugins" submenu ------------------------------------
    g_menu_container = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Otto Throttle", nullptr, 0);
    g_menu_id = XPLMCreateMenu("Otto Throttle", XPLMFindPluginsMenu(), g_menu_container, MenuHandler, nullptr);

    g_enable_item_idx = XPLMAppendMenuItem(g_menu_id, "Enable Otto Throttle", (void *)"toggle_at", 0);
    XPLMAppendMenuItem(g_menu_id, "Reinflate Otto", (void *)"reload_cfg", 0);

    RefreshMenuCheckmark();

    // ---- Register the flight loop (starts inactive) ----------------
    XPLMRegisterFlightLoopCallback(FlightLoopCallback, -1.0f, nullptr);

    return 1;
}

// =====================================================================
// XPluginStop
// =====================================================================
PLUGIN_API void XPluginStop(void)
{
    XPLMUnregisterFlightLoopCallback(FlightLoopCallback, nullptr);

    if (g_menu_id)
        XPLMDestroyMenu(g_menu_id);
}

// =====================================================================
// XPluginEnable / Disable
// =====================================================================
PLUGIN_API int XPluginEnable(void)
{
    return 1;
}

PLUGIN_API void XPluginDisable(void)
{
    // Make sure we hand control back to the pilot if the plugin
    // itself gets disabled while the autothrottle was active.
    EnableAutoThrottle(false);
}

// =====================================================================
// XPluginReceiveMessage
// =====================================================================
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void *inParam)
{
    (void)inFrom;

    if (inMsg == XPLM_MSG_PLANE_LOADED)
    {
        // inParam is the plane index; 0 = user's aircraft
        if (reinterpret_cast<intptr_t>(inParam) == 0)
        {
            LoadConfig();
            XPLMDebugString("[AutoThrottle] Aircraft changed, config reloaded.\n");
        }
    }
}

// =====================================================================
// Menu handling
// =====================================================================
static void MenuHandler(void *inMenuRef, void *inItemRef)
{
    (void)inMenuRef;
    const char *item = static_cast<const char *>(inItemRef);

    if (strcmp(item, "toggle_at") == 0)
    {
        EnableAutoThrottle(!g_at_enabled);
    }
    else if (strcmp(item, "reload_cfg") == 0)
    {
        LoadConfig();
        XPLMDebugString("[OttoThrottle] Config reloaded.\n");
    }
}

static void RefreshMenuCheckmark()
{
    if (g_menu_id && g_enable_item_idx >= 0)
    {
        XPLMCheckMenuItem(g_menu_id, g_enable_item_idx,
                           g_at_enabled ? xplm_Menu_Checked : xplm_Menu_Unchecked);
    }
}

static void EnableAutoThrottle(bool enable)
{
    g_at_enabled = enable;

    // Reset PID state whenever we (re)engage, to avoid a kick from
    // stale integrator/derivative values.
    g_integral   = 0.0f;
    g_prev_error = 0.0f;

    // if (dr_override_throttles)
    //     XPLMSetDatai(dr_override_throttles, enable ? 1 : 0);

    if (!enable)
    {
        // Leave throttles wherever they are; the sim resumes normal
        // (joystick/manual) control since the override flag is cleared.
    }

    RefreshMenuCheckmark();

    XPLMDebugString(enable ? "[OttoThrottle] Engaged.\n" : "[OttoThrottle] Disengaged.\n");
}

// =====================================================================
// Flight loop: runs the PID and writes throttle commands
// =====================================================================
static float FlightLoopCallback(float elapsedMe, float elapsedSim, int counter, void *refcon)
{
    (void)elapsedSim; (void)counter; (void)refcon;

    const float interval = 1.0f / g_cfg.update_hz;

    if (!g_at_enabled || !dr_target_speed_kts || !dr_current_speed_kts || !dr_throttle_setting)
        return interval;

    float target_kts  = XPLMGetDataf(dr_target_speed_kts);
    float current_kts = XPLMGetDataf(dr_current_speed_kts);

    // If the MCP speed bug is at 0 (not set), don't fight the pilot.
    // if (target_kts <= 0.0f)
    //     return interval;

    float dt = elapsedMe > 0.0f ? elapsedMe : interval;

    float error = target_kts - current_kts;

    // --- PID terms ----------------------------------------------------
    g_integral += error * dt;

    float derivative = (error - g_prev_error) / dt;
    float output = g_cfg.kp * error + g_cfg.ki * g_integral + g_cfg.kd * derivative;
    
    // Throttle is a 0..1 ratio.
    float current_throttle = XPLMGetDataf(dr_throttle_setting);
    float new_throttle = std::max(0.0f, std::min(1.0f, current_throttle + output));
    
    XPLMSetDataf(dr_throttle_setting, new_throttle);

    g_prev_error = error;
    
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

        if (key == "kp")        g_cfg.kp = fval;
        else if (key == "ki")   g_cfg.ki = fval;
        else if (key == "kd")   g_cfg.kd = fval;
        else if (key == "hz")   g_cfg.update_hz = fval;
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
        std::string candidateAcf = acfDir + sep + kConfigFileName;
        if (TryLoadFrom(candidateAcf))
        {
            XPLMDebugString(("[AutoThrottle] Loaded config: " + candidateAcf + "\n").c_str());
            return;
        }
    }

    // 2) Plugin root directory (fallback): …/AutoThrottle/autothrottle.cfg
    std::string root = GetPluginDirectory();
    std::string candidate1 = root + sep + kConfigFileName;

    // 3) Same directory as the binary (fallback): …/AutoThrottle/64/autothrottle.cfg
    char path[512];
    XPLMGetPluginInfo(XPLMGetMyID(), nullptr, path, nullptr, nullptr);
    std::string binDir(path);
    size_t last = binDir.find_last_of(sep);
    std::string candidate2 = (last == std::string::npos)
        ? std::string(kConfigFileName)
        : binDir.substr(0, last) + sep + kConfigFileName;

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
              g_cfg.kp, g_cfg.ki, g_cfg.kd);
    XPLMDebugString(msg);
}
