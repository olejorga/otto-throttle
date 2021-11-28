#include <string>
#include <cstdlib>

#include "XPLMMenus.h"
#include "XPLMDataAccess.h"
#include "XPLMProcessing.h"
#include "XPLMUtilities.h"

#if IBM
	#include <windows.h>
#endif

#ifndef XPLM300
	#error This is made to be compiled against the XPLM300 SDK
#endif

int g_is_engaged = 0;
int g_menu_container_idx; // The index of our menu item in the Plugins menu

XPLMMenuID g_menu_id; // The menu container we'll append all our menu items to

XPLMDataRef g_current_speed = XPLMFindDataRef("sim/flightmodel/position/indicated_airspeed"); // FLOAT
XPLMDataRef g_target_speed = XPLMFindDataRef("sim/cockpit2/autopilot/airspeed_dial_kts_mach"); // FLOAT
XPLMDataRef g_throttle_setting = XPLMFindDataRef("sim/cockpit2/engine/actuators/throttle_ratio_all"); // FLOAT
XPLMDataRef g_sim_rate = XPLMFindDataRef("sim/time/sim_speed"); // INT

float g_last_speed = XPLMGetDataf(g_current_speed);

void menu_handler(void*, void*);
float adjust_thrust(float elapsed1, float elapsed2, int ctr, void* refcon);
void increase_thrust(float factor);
void decrease_thrust(float factor);

PLUGIN_API int XPluginStart(
	char* outName,
	char* outSig,
	char* outDesc)
{
	strcpy(outName, "Otto Throttle");
	strcpy(outSig, "olejorga");
	strcpy(outDesc, "A sample plug-in that demonstrates and exercises the X-Plane menu API.");

	g_menu_container_idx = XPLMAppendMenuItem(XPLMFindPluginsMenu(), "Otto Throttle", 0, 0);
	g_menu_id = XPLMCreateMenu("Sample Menu", XPLMFindPluginsMenu(), g_menu_container_idx, menu_handler, NULL);

	XPLMAppendMenuItem(g_menu_id, "Engage", (void*)"engage", 1);
	XPLMAppendMenuItem(g_menu_id, "Disengage", (void*)"disengage", 1);
	// XPLMAppendMenuItemWithCommand(g_menu_id, "Toggle Flight Configuration (Command-Based)", XPLMFindCommand("sim/operation/toggle_flight_config"));

	// Changed your mind? You can destroy the submenu you created with XPLMDestroyMenu(),
	// then remove the "Sample Menu" item from the "Plugins" menu with XPLMRemoveMenuItem().
	// XPLMDestroyMenu(g_menu_id);
	// XPLMRemoveMenuItem(XPLMFindPluginsMenu(), g_menu_container_idx);

	XPLMRegisterFlightLoopCallback(adjust_thrust, 1, NULL);

	return 1;
}

PLUGIN_API void	XPluginStop(void)
{
	// Since we created this menu, we'll be good citizens and clean it up as well
	XPLMDestroyMenu(g_menu_id);
	// If we were able to add a command to the aircraft menu, it will be automatically removed for us when we're unloaded
}

PLUGIN_API void XPluginDisable(void)
{
	// XPLMDestroyMenu(g_menu_id);
}

PLUGIN_API int XPluginEnable(void)
{
	return 1;
}

PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFrom, int inMsg, void* inParam) { }

void menu_handler(void* in_menu_ref, void* in_item_ref)
{
	if (!strcmp((const char*)in_item_ref, "engage"))
	{
		g_is_engaged = 1;
	}

	if (!strcmp((const char*)in_item_ref, "disengage"))
	{
		g_is_engaged = 0;
	}
}

float adjust_thrust(float elapsed1, float elapsed2, int ctr, void* refcon)
{
	if (g_is_engaged == 1 && XPLMGetDatai(g_sim_rate) != 0)
	{
		float speed_diff = abs(XPLMGetDataf(g_current_speed) - XPLMGetDataf(g_target_speed));
		float current_speed = XPLMGetDataf(g_current_speed);
		float target_speed = XPLMGetDataf(g_target_speed);

		if ((current_speed < target_speed) && (current_speed < g_last_speed))
		{
			if (speed_diff < 5)
			{
				increase_thrust(0.0001);
			}
			else {
				increase_thrust(0.001);
			}
		}

		if ((current_speed > target_speed) && (current_speed > g_last_speed))
		{
			if (speed_diff < 5)
			{
				decrease_thrust(0.0001);
			}
			else {
				decrease_thrust(0.001);
			}
		}

		g_last_speed = current_speed;
	}

	return -1;
}

void increase_thrust(float factor)
{
	float throttle_setting = XPLMGetDataf(g_throttle_setting);

	if ((throttle_setting + factor) == 1.0)
	{
		XPLMSetDataf(g_throttle_setting, 1.0);
	}
	else if (throttle_setting < 1) {
		XPLMSetDataf(g_throttle_setting, (throttle_setting + factor));
	}
}

void decrease_thrust(float factor)
{
	float throttle_setting = XPLMGetDataf(g_throttle_setting);

	if ((throttle_setting - factor) == 1.0)
	{
		XPLMSetDataf(g_throttle_setting, 0.0);
	}
	else if (throttle_setting > 0) {
		XPLMSetDataf(g_throttle_setting, (throttle_setting - factor));
	}
}
