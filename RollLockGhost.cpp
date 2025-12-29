#include "pch.h"
#include "RollLockGhost.h"

BAKKESMOD_PLUGIN(RollLockGhost, "Roll-Locked Stick Training Tool", plugin_version, PLUGINTYPE_FREEPLAY)

std::shared_ptr<CVarManagerWrapper> _globalCvarManager;

void RollLockGhost::onLoad()
{
	_globalCvarManager = cvarManager;
	
	cvarManager->registerCvar("rolllock_enabled", "0", "Enable roll-locked stick behavior", true, true, 0, true, 1)
		.addOnValueChanged([this](std::string oldValue, CVarWrapper cvar) {
			bool enabled = cvar.getBoolValue();
			if (enabled) {
				cvarManager->log("Roll-Lock enabled - hold activation button (Handbrake) to engage");
			} else {
				rollLockActive = false;
				cvarManager->log("Roll-Lock disabled");
			}
		});
	
	cvarManager->registerCvar("rolllock_show_overlay", "1", "Show ghost joystick overlay", true, true, 0, true, 1);
	cvarManager->registerCvar("rolllock_button_mode", "0", "Activation button: 0=Handbrake, 1=Boost (future)", true, true, 0, true, 1);
	cvarManager->registerCvar("rolllock_overlay_x", "100", "Overlay X position (pixels from right)", true, true, 0, true, 1920);
	cvarManager->registerCvar("rolllock_overlay_y", "100", "Overlay Y position (pixels from bottom)", true, true, 0, true, 1080);
	cvarManager->registerCvar("rolllock_overlay_radius", "60", "Overlay circle radius", true, true, 20, true, 200);
	
	cvarManager->registerNotifier("rolllock_toggle", [this](std::vector<std::string> params) {
		ToggleRollLock(params);
	}, "Toggle roll-lock enabled/disabled", PERMISSION_FREEPLAY);
	
	cvarManager->registerNotifier("rolllock_overlay_toggle", [this](std::vector<std::string> params) {
		ToggleOverlay(params);
	}, "Toggle ghost joystick overlay", PERMISSION_FREEPLAY);
	
	gameWrapper->HookEventWithCallerPost<CarWrapper>("Function TAGame.Car_TA.SetVehicleInput",
		[this](CarWrapper caller, void* params, std::string eventName) {
			OnSetVehicleInput(caller, params, eventName);
		});
	
	gameWrapper->RegisterDrawable([this](CanvasWrapper canvas) {
		DrawGhostJoystick(canvas);
	});
	
	cvarManager->log("RollLockGhost plugin loaded");
	cvarManager->log("Use 'rolllock_toggle' to enable/disable the feature");
	cvarManager->log("Use 'rolllock_overlay_toggle' to show/hide the overlay");
}

void RollLockGhost::onUnload()
{
	gameWrapper->UnhookEventPost("Function TAGame.Car_TA.SetVehicleInput");
	gameWrapper->UnregisterDrawables();
}

void RollLockGhost::OnSetVehicleInput(CarWrapper caller, void* params, std::string eventName)
{
	CVarWrapper enabledCvar = cvarManager->getCvar("rolllock_enabled");
	if (!enabledCvar || !enabledCvar.getBoolValue()) {
		rollLockActive = false;
		return;
	}
	
	if (!gameWrapper->IsInFreeplay() && !gameWrapper->IsInCustomTraining()) {
		return;
	}
	
	if (!caller || caller.IsNull()) {
		return;
	}
	
	ServerWrapper server = gameWrapper->GetCurrentGameState();
	if (!server || server.IsNull()) {
		return;
	}
	
	PlayerControllerWrapper pc = gameWrapper->GetPlayerController();
	if (!pc || pc.IsNull()) {
		return;
	}
	
	CarWrapper localCar = pc.GetCar();
	if (!localCar || localCar.IsNull() || caller.memory_address != localCar.memory_address) {
		return;
	}
	
	struct VehicleInputParams {
		ControllerInput input;
	};
	
	VehicleInputParams* inputParams = static_cast<VehicleInputParams*>(params);
	if (!inputParams) {
		return;
	}
	
	ControllerInput& input = inputParams->input;
	
	rawYaw = input.Yaw;
	rawPitch = input.Pitch;
	
	bool buttonPressed = IsActivationButtonPressed(caller);
	
	if (buttonPressed && !rollLockActive) {
		Rotator rot = caller.GetRotation();
		baseRoll = rot.Roll;
		rollLockActive = true;
		cvarManager->log("Roll-Lock ENGAGED");
	} else if (!buttonPressed && rollLockActive) {
		rollLockActive = false;
		cvarManager->log("Roll-Lock RELEASED");
	}
	
	if (rollLockActive) {
		Rotator currentRot = caller.GetRotation();
		float currentRoll = currentRot.Roll;
		
		float deltaRoll = NormalizeAngle(currentRoll - baseRoll);
		
		float cosD = cosf(-deltaRoll);
		float sinD = sinf(-deltaRoll);
		
		correctedYaw = rawYaw * cosD + rawPitch * sinD;
		correctedPitch = -rawYaw * sinD + rawPitch * cosD;
		
		correctedYaw = std::max(-1.0f, std::min(1.0f, correctedYaw));
		correctedPitch = std::max(-1.0f, std::min(1.0f, correctedPitch));
		
		input.Yaw = correctedYaw;
		input.Pitch = correctedPitch;
	} else {
		correctedYaw = rawYaw;
		correctedPitch = rawPitch;
	}
}

void RollLockGhost::DrawGhostJoystick(CanvasWrapper canvas)
{
	CVarWrapper showOverlayCvar = cvarManager->getCvar("rolllock_show_overlay");
	if (!showOverlayCvar || !showOverlayCvar.getBoolValue()) {
		return;
	}
	
	if (!gameWrapper->IsInFreeplay() && !gameWrapper->IsInCustomTraining()) {
		return;
	}
	
	CVarWrapper xCvar = cvarManager->getCvar("rolllock_overlay_x");
	CVarWrapper yCvar = cvarManager->getCvar("rolllock_overlay_y");
	CVarWrapper radiusCvar = cvarManager->getCvar("rolllock_overlay_radius");
	
	if (!xCvar || !yCvar || !radiusCvar) {
		return;
	}
	
	int screenWidth = canvas.GetSize().X;
	int screenHeight = canvas.GetSize().Y;
	
	int offsetX = xCvar.getIntValue();
	int offsetY = yCvar.getIntValue();
	float radius = radiusCvar.getFloatValue();
	
	Vector2F center = {
		static_cast<float>(screenWidth - offsetX),
		static_cast<float>(screenHeight - offsetY)
	};
	
	canvas.SetColor(255, 255, 255, 200);
	for (int i = 0; i < 50; i++) {
		float angle1 = (i / 50.0f) * 2.0f * 3.14159f;
		float angle2 = ((i + 1) / 50.0f) * 2.0f * 3.14159f;
		Vector2F p1 = {center.X + cosf(angle1) * radius, center.Y + sinf(angle1) * radius};
		Vector2F p2 = {center.X + cosf(angle2) * radius, center.Y + sinf(angle2) * radius};
		canvas.DrawLine(p1, p2, 1.0f);
	}
	
	canvas.SetColor(128, 128, 128, 200);
	canvas.DrawLine(Vector2F{center.X - 5, center.Y}, Vector2F{center.X + 5, center.Y}, 1.0f);
	canvas.DrawLine(Vector2F{center.X, center.Y - 5}, Vector2F{center.X, center.Y + 5}, 1.0f);
	
	float rawX = rawYaw;
	float rawY = -rawPitch;
	
	Vector2F rawPos = {
		center.X + rawX * radius * 0.9f,
		center.Y + rawY * radius * 0.9f
	};
	
	canvas.SetColor(0, 255, 0, 255);
	canvas.DrawLine(center, rawPos, 2.0f);
	for (int i = 0; i < 20; i++) {
		float angle = (i / 20.0f) * 2.0f * 3.14159f;
		Vector2F p1 = {rawPos.X + cosf(angle) * 5, rawPos.Y + sinf(angle) * 5};
		Vector2F p2 = {rawPos.X + cosf(angle + 0.3f) * 5, rawPos.Y + sinf(angle + 0.3f) * 5};
		canvas.DrawLine(p1, p2, 1.0f);
	}
	
	float corrX = correctedYaw;
	float corrY = -correctedPitch;
	
	if (rollLockActive) {
		Vector2F corrPos = {
			center.X + corrX * radius * 0.9f,
			center.Y + corrY * radius * 0.9f
		};
		
		canvas.SetColor(255, 0, 0, 255);
		canvas.DrawLine(center, corrPos, 2.0f);
		for (int i = 0; i < 20; i++) {
			float angle = (i / 20.0f) * 2.0f * 3.14159f;
			Vector2F p1 = {corrPos.X + cosf(angle) * 5, corrPos.Y + sinf(angle) * 5};
			Vector2F p2 = {corrPos.X + cosf(angle + 0.3f) * 5, corrPos.Y + sinf(angle + 0.3f) * 5};
			canvas.DrawLine(p1, p2, 1.0f);
		}
	}
	
	canvas.SetPosition(Vector2{static_cast<int>(center.X - radius), static_cast<int>(center.Y - radius - 40)});
	canvas.SetColor(0, 255, 0, 255);
	canvas.DrawString("Raw Input", 1.0f, 1.0f);
	
	if (rollLockActive) {
		canvas.SetPosition(Vector2{static_cast<int>(center.X - radius), static_cast<int>(center.Y - radius - 25)});
		canvas.SetColor(255, 0, 0, 255);
		canvas.DrawString("Corrected Input", 1.0f, 1.0f);
	}
	
	CVarWrapper enabledCvar = cvarManager->getCvar("rolllock_enabled");
	if (enabledCvar && enabledCvar.getBoolValue()) {
		canvas.SetPosition(Vector2{static_cast<int>(center.X - radius), static_cast<int>(center.Y + radius + 10)});
		if (rollLockActive) {
			canvas.SetColor(0, 255, 0, 255);
			canvas.DrawString("ROLL-LOCK ACTIVE", 1.2f, 1.2f);
		} else {
			canvas.SetColor(255, 255, 0, 255);
			canvas.DrawString("Roll-Lock Ready (Hold Handbrake)", 1.0f, 1.0f);
		}
	}
}

float RollLockGhost::NormalizeAngle(float angle)
{
	constexpr float PI = 3.14159265358979323846f;
	while (angle > PI) angle -= 2.0f * PI;
	while (angle < -PI) angle += 2.0f * PI;
	return angle;
}

bool RollLockGhost::IsActivationButtonPressed(CarWrapper car)
{
	if (!car || car.IsNull()) {
		return false;
	}
	
	ControllerInput input = car.GetInput();
	
	CVarWrapper buttonModeCvar = cvarManager->getCvar("rolllock_button_mode");
	int buttonMode = buttonModeCvar ? buttonModeCvar.getIntValue() : 0;
	
	switch (buttonMode) {
		case 0:
			return input.Handbrake != 0;
		case 1:
			return input.ActivateBoost != 0;
		default:
			return input.Handbrake != 0;
	}
}

void RollLockGhost::ToggleRollLock(std::vector<std::string> params)
{
	CVarWrapper enabledCvar = cvarManager->getCvar("rolllock_enabled");
	if (!enabledCvar) {
		return;
	}
	
	bool currentValue = enabledCvar.getBoolValue();
	enabledCvar.setValue(!currentValue);
	
	cvarManager->log(!currentValue ? "Roll-Lock ENABLED" : "Roll-Lock DISABLED");
}

void RollLockGhost::ToggleOverlay(std::vector<std::string> params)
{
	CVarWrapper showCvar = cvarManager->getCvar("rolllock_show_overlay");
	if (!showCvar) {
		return;
	}
	
	bool currentValue = showCvar.getBoolValue();
	showCvar.setValue(!currentValue);
	
	cvarManager->log(!currentValue ? "Ghost Joystick Overlay SHOWN" : "Ghost Joystick Overlay HIDDEN");
}
