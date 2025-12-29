#pragma once

#include "bakkesmod/plugin/bakkesmodplugin.h"
#include "bakkesmod/plugin/pluginwindow.h"
#include "bakkesmod/plugin/PluginSettingsWindow.h"

#include "version.h"
constexpr auto plugin_version = stringify(VERSION_MAJOR) "." stringify(VERSION_MINOR) "." stringify(VERSION_PATCH) "." stringify(VERSION_BUILD);

class RollLockGhost: public BakkesMod::Plugin::BakkesModPlugin
{
private:
	bool rollLockActive = false;
	float baseRoll = 0.0f;
	
	float rawYaw = 0.0f;
	float rawPitch = 0.0f;
	float correctedYaw = 0.0f;
	float correctedPitch = 0.0f;
	
	float timeSinceLastLog = 0.0f;
	const float LOG_INTERVAL = 0.2f;
	
	float NormalizeAngle(float angle);
	bool IsActivationButtonPressed(CarWrapper car);
	void DrawGhostJoystick(CanvasWrapper canvas);

public:
	virtual void onLoad();
	virtual void onUnload();
	
	void OnSetVehicleInput(CarWrapper caller, void* params, std::string eventName);
	
	void ToggleRollLock(std::vector<std::string> params);
	void ToggleOverlay(std::vector<std::string> params);
};
