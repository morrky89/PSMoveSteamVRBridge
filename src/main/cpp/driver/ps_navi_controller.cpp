#define _USE_MATH_DEFINES

#include "constants.h"
#include "server_driver.h"
#include "utils.h"
#include "settings_util.h"
#include "driver.h"
#include "facing_handsolver.h"
#include "ps_navi_controller.h"
#include "trackable_device.h"
#include <assert.h>

#if _MSC_VER
#define strcasecmp(a, b) stricmp(a,b)
#pragma warning (disable: 4996) // 'This function or variable may be unsafe': snprintf
#define snprintf _snprintf
#endif

namespace steamvrbridge {

	// -- PSNaviControllerConfig -----
	configuru::Config PSNaviControllerConfig::WriteToJSON() {
		configuru::Config &pt= ControllerConfig::WriteToJSON();

		// General Settings
		pt["thumbstick_deadzone"] = thumbstick_deadzone;

		//PSMove controller button -> fake touchpad mappings
		WriteEmulatedTouchpadAction(pt, k_PSMButtonID_PS);
		WriteEmulatedTouchpadAction(pt, k_PSMButtonID_DPad_Left);
		WriteEmulatedTouchpadAction(pt, k_PSMButtonID_DPad_Up);
		WriteEmulatedTouchpadAction(pt, k_PSMButtonID_DPad_Right);
		WriteEmulatedTouchpadAction(pt, k_PSMButtonID_DPad_Down);
		WriteEmulatedTouchpadAction(pt, k_PSMButtonID_Circle);
		WriteEmulatedTouchpadAction(pt, k_PSMButtonID_Cross);
		WriteEmulatedTouchpadAction(pt, k_PSMButtonID_Joystick);
		WriteEmulatedTouchpadAction(pt, k_PSMButtonID_Shoulder);

		return pt;
	}

	bool PSNaviControllerConfig::ReadFromJSON(const configuru::Config &pt) {

		if (!ControllerConfig::ReadFromJSON(pt))
			return false;

		// General Settings
		thumbstick_deadzone = pt.get_or<float>("thumbstick_deadzone", thumbstick_deadzone);

		// DS4 controller button -> fake touchpad mappings
		ReadEmulatedTouchpadAction(pt, k_PSMButtonID_PS);
		ReadEmulatedTouchpadAction(pt, k_PSMButtonID_DPad_Left);
		ReadEmulatedTouchpadAction(pt, k_PSMButtonID_DPad_Up);
		ReadEmulatedTouchpadAction(pt, k_PSMButtonID_DPad_Right);
		ReadEmulatedTouchpadAction(pt, k_PSMButtonID_DPad_Down);
		ReadEmulatedTouchpadAction(pt, k_PSMButtonID_Circle);
		ReadEmulatedTouchpadAction(pt, k_PSMButtonID_Cross);
		ReadEmulatedTouchpadAction(pt, k_PSMButtonID_Joystick);
		ReadEmulatedTouchpadAction(pt, k_PSMButtonID_Shoulder);

		return true;
	}

	// -- PSNaviController -----
	PSNaviController::PSNaviController(
		PSMControllerID psmControllerId,
		vr::ETrackedControllerRole trackedControllerRole,
		const char *psmSerialNo)
		: Controller()
		, m_parentController(nullptr)
		, m_nPSMControllerId(psmControllerId)
		, m_PSMServiceController(nullptr)
		, m_nPoseSequenceNumber(0)
		, m_resetPoseButtonPressTime()
		, m_bResetPoseRequestSent(false)
		, m_resetAlignButtonPressTime()
		, m_bResetAlignRequestSent(false)
		, m_touchpadDirectionsUsed(false)
		, m_lastSanitizedThumbstick_X(0.f)
		, m_lastSanitizedThumbstick_Y(0.f) {
		char svrIdentifier[256];
		Utils::GenerateControllerSteamVRIdentifier(svrIdentifier, sizeof(svrIdentifier), psmControllerId);
		m_strSteamVRSerialNo = svrIdentifier;

		m_lastTouchpadPressTime = std::chrono::high_resolution_clock::now();

		if (psmSerialNo != NULL) {
			m_strPSMControllerSerialNo = psmSerialNo;
		}

		// Tell PSM Client API that we are listening to this controller id
		PSM_AllocateControllerListener(psmControllerId);
		m_PSMServiceController = PSM_GetController(psmControllerId);

		m_TrackedControllerRole = trackedControllerRole;

		m_trackingStatus = vr::TrackingResult_Running_OK;
	}

	PSNaviController::~PSNaviController() {
		PSM_FreeControllerListener(m_PSMServiceController->ControllerID);
		m_PSMServiceController = nullptr;
	}

	void PSNaviController::AttachToController(Controller *parent_controller)
	{
		if (m_unSteamVRTrackedDeviceId == vr::k_unTrackedDeviceIndexInvalid)
		{
			// Use our parent's property container for registering buttons and axes
			m_parentController= parent_controller;
			m_ulPropertyContainer = parent_controller->getPropertyContainerHandle();

			PSMRequestID requestId;
			if (PSM_StartControllerDataStreamAsync(
				m_PSMServiceController->ControllerID,
				PSMStreamFlags_includePositionData | PSMStreamFlags_includePhysicsData,
				&requestId) != PSMResult_Error) {
				PSM_RegisterCallback(requestId, PSNaviController::start_controller_response_callback, this);
			}
		}
		else
		{
			Logger::Error("PSNaviController::AttachToController() - Can't attach PSNavi(%s) since it's already been Activated!",
				m_strPSMControllerSerialNo.c_str());
		}
	}

	vr::EVRInitError PSNaviController::Activate(vr::TrackedDeviceIndex_t unObjectId) {
		vr::EVRInitError result = Controller::Activate(unObjectId);

		if (m_parentController != nullptr) {
			Logger::Error("PSNaviController::AttachToController() - Can't Activate PSNavi(%s) since it's already been attached to %s!",
				m_strPSMControllerSerialNo.c_str(), m_parentController->GetPSMControllerSerialNo().c_str());
			return vr::VRInitError_Driver_Failed;
		}

		if (result == vr::VRInitError_None) {
			Logger::Info("PSNaviController::Activate - Controller %d Activated\n", unObjectId);

			PSMRequestID requestId;
			if (PSM_StartControllerDataStreamAsync(
				m_PSMServiceController->ControllerID,
				PSMStreamFlags_includePositionData | PSMStreamFlags_includePhysicsData,
				&requestId) != PSMResult_Error) {
				PSM_RegisterCallback(requestId, PSNaviController::start_controller_response_callback, this);
			}

			// Setup controller properties
			{
				vr::CVRPropertyHelpers *properties = vr::VRProperties();

				properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceOff_String, "{psmove}psnavi_status_off.png");
				properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearching_String, "{psmove}psnavi_status_ready.png");
				properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceSearchingAlert_String, "{psmove}psnavi_status_ready_alert.png");
				properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReady_String, "{psmove}psnavi_status_ready.png");
				properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceReadyAlert_String, "{psmove}psnavi_status_ready_alert.png");
				properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceNotReady_String, "{psmove}psnavi_status_error.png");
				properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceStandby_String, "{psmove}psnavi_status_ready.png");
				properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_NamedIconPathDeviceAlertLow_String, "{psmove}psnavi_status_ready_low.png");

				properties->SetBoolProperty(m_ulPropertyContainer, vr::Prop_WillDriftInYaw_Bool, false);
				properties->SetBoolProperty(m_ulPropertyContainer, vr::Prop_DeviceIsWireless_Bool, true);
				properties->SetBoolProperty(m_ulPropertyContainer, vr::Prop_DeviceProvidesBatteryStatus_Bool, false);

				properties->SetInt32Property(m_ulPropertyContainer, vr::Prop_DeviceClass_Int32, vr::TrackedDeviceClass_Controller);

				// The {psmove} syntax lets us refer to rendermodels that are installed
				// in the driver's own resources/rendermodels directory.  The driver can
				// still refer to SteamVR models like "generic_hmd".
				if (getConfig()->override_model.length() > 0)
				{
					properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_RenderModelName_String, getConfig()->override_model.c_str());
				}
				else
				{
					properties->SetStringProperty(m_ulPropertyContainer, vr::Prop_RenderModelName_String, "{psmove}navi_controller");
				}

				// Set device properties
				vr::VRProperties()->SetInt32Property(m_ulPropertyContainer, vr::Prop_ControllerRoleHint_Int32, m_TrackedControllerRole);
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ManufacturerName_String, "Sony");
				vr::VRProperties()->SetUint64Property(m_ulPropertyContainer, vr::Prop_HardwareRevision_Uint64, 1313);
				vr::VRProperties()->SetUint64Property(m_ulPropertyContainer, vr::Prop_FirmwareVersion_Uint64, 1315);
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_ModelNumber_String, "PS Navi");
				vr::VRProperties()->SetStringProperty(m_ulPropertyContainer, vr::Prop_SerialNumber_String, m_strPSMControllerSerialNo.c_str());
			}
		}

		return result;
	}

	void PSNaviController::start_controller_response_callback(
		const PSMResponseMessage *response, void *userdata) {
		PSNaviController *controller = reinterpret_cast<PSNaviController *>(userdata);

		if (response->result_code == PSMResult::PSMResult_Success) {

			Logger::Info("PSNaviController::start_controller_response_callback - Controller stream started\n");

			// Create the special case system button (bound to PS button)
			controller->CreateButtonComponent(k_PSMButtonID_System);

			// Create buttons components
			controller->CreateButtonComponent(k_PSMButtonID_PS);
			controller->CreateButtonComponent(k_PSMButtonID_Circle);
			controller->CreateButtonComponent(k_PSMButtonID_Cross);
			controller->CreateButtonComponent(k_PSMButtonID_DPad_Up);
			controller->CreateButtonComponent(k_PSMButtonID_DPad_Down);
			controller->CreateButtonComponent(k_PSMButtonID_DPad_Left);
			controller->CreateButtonComponent(k_PSMButtonID_DPad_Right);
			controller->CreateButtonComponent(k_PSMButtonID_Shoulder);
			controller->CreateButtonComponent(k_PSMButtonID_Joystick);

			// Create axis components
			controller->CreateAxisComponent(k_PSMAxisID_Trigger);
			controller->CreateAxisComponent(k_PSMAxisID_Joystick_X);
			controller->CreateAxisComponent(k_PSMAxisID_Joystick_Y);

			// [optional] Create components for emulated trackpad
			controller->CreateButtonComponent(k_PSMButtonID_EmulatedTrackpadTouched);
			controller->CreateButtonComponent(k_PSMButtonID_EmulatedTrackpadPressed);
			controller->CreateAxisComponent(k_PSMAxisID_EmulatedTrackpad_X);
			controller->CreateAxisComponent(k_PSMAxisID_EmulatedTrackpad_Y);
		}
	}

	void PSNaviController::Deactivate() {
		Logger::Info("PSNaviController::Deactivate - Controller stream stopped\n");
		PSM_StopControllerDataStreamAsync(m_PSMServiceController->ControllerID, nullptr);
		Controller::Deactivate();
	}

	void PSNaviController::UpdateControllerState() {
		static const uint64_t s_kTouchpadButtonMask = vr::ButtonMaskFromId(vr::k_EButton_SteamVR_Touchpad);

		assert(m_PSMServiceController != nullptr);
		assert(m_PSMServiceController->IsConnected);

		const PSMPSNavi &clientView = m_PSMServiceController->ControllerState.PSNaviState;

		// System Button hard-coded to PS button
		Controller::UpdateButton(k_PSMButtonID_System, clientView.PSButton);

		// Process all the native buttons 
		Controller::UpdateButton(k_PSMButtonID_PS, clientView.PSButton);
		Controller::UpdateButton(k_PSMButtonID_Circle, clientView.CircleButton);
		Controller::UpdateButton(k_PSMButtonID_Cross, clientView.CrossButton);
		Controller::UpdateButton(k_PSMButtonID_DPad_Up, clientView.DPadUpButton);
		Controller::UpdateButton(k_PSMButtonID_DPad_Down, clientView.DPadDownButton);
		Controller::UpdateButton(k_PSMButtonID_DPad_Left, clientView.DPadLeftButton);
		Controller::UpdateButton(k_PSMButtonID_DPad_Right, clientView.DPadRightButton);
		Controller::UpdateButton(k_PSMButtonID_Shoulder, clientView.L2Button);
		Controller::UpdateButton(k_PSMButtonID_Joystick, clientView.L3Button);

		// Thumbstick handling
		UpdateThumbstick();

		// Touchpad handling
		UpdateEmulatedTrackpad();

		// Trigger handling
		Controller::UpdateAxis(k_PSMAxisID_Trigger, clientView.TriggerValue / 255.f);
	}

	void PSNaviController::UpdateThumbstick()
	{
		const PSMPSNavi &clientView = m_PSMServiceController->ControllerState.PSNaviState;

		const unsigned char rawThumbStickX = clientView.Stick_XAxis;
		const unsigned char rawThumbStickY = clientView.Stick_YAxis;
		const float thumb_stick_x = ((float)rawThumbStickX - 127.f) / 127.f;
		const float thumb_stick_y = ((float)rawThumbStickY - 127.f) / 127.f;
		const float thumb_stick_radius = sqrtf(thumb_stick_x*thumb_stick_x + thumb_stick_y * thumb_stick_y);

		// Moving a thumb-stick outside of the deadzone is consider a touchpad touch
		if (thumb_stick_radius >= getConfig()->thumbstick_deadzone)
		{
			// Rescale the thumb-stick position to hide the dead zone
			const float rescaledRadius = (thumb_stick_radius - getConfig()->thumbstick_deadzone) / (1.f - getConfig()->thumbstick_deadzone);

			// Set the thumb-stick axis
			m_lastSanitizedThumbstick_X = (rescaledRadius / thumb_stick_radius) * thumb_stick_x;
			m_lastSanitizedThumbstick_Y = (rescaledRadius / thumb_stick_radius) * thumb_stick_y;
		}
		else
		{
			m_lastSanitizedThumbstick_X= 0.f;
			m_lastSanitizedThumbstick_Y= 0.f;
		}

		Controller::UpdateAxis(k_PSMAxisID_Joystick_X, m_lastSanitizedThumbstick_X);
		Controller::UpdateAxis(k_PSMAxisID_Joystick_Y, m_lastSanitizedThumbstick_Y);
	}

	// Updates the state of the controllers touchpad axis relative to its position over time and active state.
	void PSNaviController::UpdateEmulatedTrackpad() {
		// Bail if the config hasn't enabled the emulated trackpad
		if (!HasButton(k_PSMButtonID_EmulatedTrackpadPressed) && !HasButton(k_PSMButtonID_EmulatedTrackpadPressed))
			return;

		// Find the highest priority emulated touch pad action (if any)
		eEmulatedTrackpadAction highestPriorityAction= k_EmulatedTrackpadAction_None;
		for (int buttonIndex = 0; buttonIndex < static_cast<int>(k_PSMButtonID_Count); ++buttonIndex) {
			eEmulatedTrackpadAction action= getConfig()->ps_button_id_to_emulated_touchpad_action[buttonIndex];
			if (action != k_EmulatedTrackpadAction_None) {
				PSMButtonState button_state= PSMButtonState_UP;
				if (Controller::GetButtonState((ePSMButtonID)buttonIndex, button_state)) {
					if (button_state == PSMButtonState_DOWN || button_state == PSMButtonState_PRESSED) {
						if (action >= highestPriorityAction) {
							highestPriorityAction= action;
						}

						if (action >= k_EmulatedTrackpadAction_Press) {
							break;
						}
					}
				}
			}
		}

		float touchpad_x = 0.f;
		float touchpad_y = 0.f;
		PSMButtonState emulatedTouchPadTouchedState= PSMButtonState_UP;
		PSMButtonState emulatedTouchPadPressedState= PSMButtonState_UP;

		if (highestPriorityAction == k_EmulatedTrackpadAction_Touch)
		{
			emulatedTouchPadTouchedState= PSMButtonState_DOWN;
		}
		else if (highestPriorityAction == k_EmulatedTrackpadAction_Press)
		{
			emulatedTouchPadTouchedState= PSMButtonState_DOWN;
			emulatedTouchPadPressedState= PSMButtonState_DOWN;
		}

		if (highestPriorityAction != k_EmulatedTrackpadAction_None)
		{
			emulatedTouchPadTouchedState= PSMButtonState_DOWN;
			emulatedTouchPadPressedState= PSMButtonState_DOWN;

			// If the action specifies a specific trackpad direction,
			// then use the given trackpad axis
			switch (highestPriorityAction)
			{
			case k_EmulatedTrackpadAction_Touch:
			case k_EmulatedTrackpadAction_Press:
				touchpad_x= 0.f;
				touchpad_y= 0.f;
				break;
			case k_EmulatedTrackpadAction_Left:
				touchpad_x= -1.f;
				touchpad_y= 0.f;
				break;
			case k_EmulatedTrackpadAction_Up:
				touchpad_x= 0.f;
				touchpad_y= 1.f;
				break;
			case k_EmulatedTrackpadAction_Right:
				touchpad_x= 1.f;
				touchpad_y= 0.f;
				break;
			case k_EmulatedTrackpadAction_Down:
				touchpad_x= 0.f;
				touchpad_y= -1.f;
				break;
			case k_EmulatedTrackpadAction_UpLeft:
				touchpad_x = -0.707f;
				touchpad_y = 0.707f;
				break;
			case k_EmulatedTrackpadAction_UpRight:
				touchpad_x = 0.707f;
				touchpad_y = 0.707f;
				break;
			case k_EmulatedTrackpadAction_DownLeft:
				touchpad_x = -0.707f;
				touchpad_y = -0.707f;
				break;
			case k_EmulatedTrackpadAction_DownRight:
				touchpad_x = 0.707f;
				touchpad_y = -0.707f;
				break;
			}		
		} else {
			// Fallback to using the thumbstick as the touchpad.
			// Consider the touchpad pressed if the thumbstick is deflected at all.
			const bool bIsTouched= fabsf(m_lastSanitizedThumbstick_X) + fabsf(m_lastSanitizedThumbstick_Y) > 0.f;

			if (bIsTouched)
			{
				emulatedTouchPadTouchedState= PSMButtonState_DOWN;
				emulatedTouchPadPressedState= PSMButtonState_DOWN;
				touchpad_x= m_lastSanitizedThumbstick_X;
				touchpad_y= m_lastSanitizedThumbstick_Y;
			}
		}

		Controller::UpdateButton(k_PSMButtonID_EmulatedTrackpadTouched, emulatedTouchPadTouchedState);
		Controller::UpdateButton(k_PSMButtonID_EmulatedTrackpadPressed, emulatedTouchPadPressedState);

		Controller::UpdateAxis(k_PSMAxisID_EmulatedTrackpad_X, touchpad_x);
		Controller::UpdateAxis(k_PSMAxisID_EmulatedTrackpad_Y, touchpad_y);
	}

	void PSNaviController::UpdateTrackingState() {
		assert(m_PSMServiceController != nullptr);
		assert(m_PSMServiceController->IsConnected);

		// The tracking status will be one of the following states:
		m_Pose.result = m_trackingStatus;

		m_Pose.deviceIsConnected = m_PSMServiceController->IsConnected;

		// These should always be false from any modern driver.  These are for Oculus DK1-like
		// rotation-only tracking.  Support for that has likely rotted in vrserver.
		m_Pose.willDriftInYaw = false;
		m_Pose.shouldApplyHeadModel = false;

		// No prediction since that's already handled in the psmove service
		m_Pose.poseTimeOffset = 0.f;

		// No transform due to the current HMD orientation
		m_Pose.qDriverFromHeadRotation.w = 1.f;
		m_Pose.qDriverFromHeadRotation.x = 0.0f;
		m_Pose.qDriverFromHeadRotation.y = 0.0f;
		m_Pose.qDriverFromHeadRotation.z = 0.0f;
		m_Pose.vecDriverFromHeadTranslation[0] = 0.f;
		m_Pose.vecDriverFromHeadTranslation[1] = 0.f;
		m_Pose.vecDriverFromHeadTranslation[2] = 0.f;

		// Set position
		m_Pose.vecPosition[0] = 0.f;
		m_Pose.vecPosition[1] = 0.f;
		m_Pose.vecPosition[2] = 0.f;

		// Set rotational coordinates

		m_Pose.qRotation.w = 1.f;
		m_Pose.qRotation.x = 0.f;
		m_Pose.qRotation.y = 0.f;
		m_Pose.qRotation.z = 0.f;

		m_Pose.poseIsValid = false;

		// This call posts this pose to shared memory, where all clients will have access to it the next
		// moment they want to predict a pose.
		vr::VRServerDriverHost()->TrackedDevicePoseUpdated(m_unSteamVRTrackedDeviceId, m_Pose, sizeof(vr::DriverPose_t));
	}

	void PSNaviController::Update() {
		Controller::Update();

		if (IsActivated() && m_PSMServiceController->IsConnected) {
			int seq_num = m_PSMServiceController->OutputSequenceNum;

			// Only other updating incoming state if it actually changed and is due for one
			if (m_nPoseSequenceNumber < seq_num) {
				m_nPoseSequenceNumber = seq_num;

				UpdateTrackingState();
				UpdateControllerState();
			}
		}
	}

	void PSNaviController::RefreshWorldFromDriverPose() {
		TrackableDevice::RefreshWorldFromDriverPose();
		// Mark the calibration process as done once we have setup the world from driver pose
		m_trackingStatus = vr::TrackingResult_Running_OK;
	}
}