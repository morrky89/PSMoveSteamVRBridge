﻿#pragma once

//-- included -----
#include <openvr_driver.h>
#include <string>
#include <vector>
#include <chrono>
#include <atomic>
#include <thread>
#include <mutex>

#include "PSMoveClient_CAPI.h"

#include "SerialAccelerometer.h"

//-- pre-declarations -----
class CPSMoveTrackedDeviceLatest;

//-- definitions -----
class CWatchdogDriver_PSMoveService : public vr::IVRWatchdogProvider
{
public:
    CWatchdogDriver_PSMoveService();

    // Inherited via IClientTrackedDeviceProvider
    virtual vr::EVRInitError Init( vr::IVRDriverContext *pDriverContext );
    virtual void Cleanup() override;

protected:
	void WorkerThreadFunction();
	void WatchdogLogVarArgs( const char *pMsgFormat, va_list args );
	void WatchdogLog( const char *pMsgFormat, ... );

private:
	class vr::IVRDriverLog * m_pLogger;
    std::mutex m_loggerMutex;

	bool m_bWasConnected;
	std::atomic_bool m_bExitSignaled;
    std::thread *m_pWatchdogThread;

	std::string m_strPSMoveServiceAddress;
	std::string m_strServerPort;

	PSMControllerList controllerList;
};

class CServerDriver_PSMoveService : public vr::IServerTrackedDeviceProvider
{
public:
    CServerDriver_PSMoveService();
    virtual ~CServerDriver_PSMoveService();

    // Inherited via IServerTrackedDeviceProvider
    virtual vr::EVRInitError Init( vr::IVRDriverContext *pDriverContext ) override;
    virtual void Cleanup() override;
    virtual const char * const *GetInterfaceVersions() override;
    virtual void RunFrame() override;
    virtual bool ShouldBlockStandbyMode() override;
    virtual void EnterStandby() override;
    virtual void LeaveStandby() override;

    void LaunchPSMoveMonitor();

	void SetHMDTrackingSpace(const PSMPosef &origin_pose);
    inline PSMPosef GetWorldFromDriverPose() const { return m_worldFromDriverPose; }

private:
    vr::ITrackedDeviceServerDriver * FindTrackedDeviceDriver(const char * pchId);
    void AllocateUniquePSMoveController(PSMControllerID ControllerID, PSMControllerHand psmControllerHand, const std::string &ControllerSerial);
    void AllocateUniqueVirtualController(PSMControllerID psmControllerID, PSMControllerHand psmControllerHand, const std::string &psmControllerSerial);
    void AttachPSNaviToParentController(PSMControllerID ControllerID, const std::string &ControllerSerial, const std::string &ParentControllerSerial);
    void AllocateUniqueDualShock4Controller(PSMControllerID ControllerID, PSMControllerHand psmControllerHand, const std::string &ControllerSerial);
    void AllocateUniquePSMoveTracker(const PSMClientTrackerInfo *trackerInfo);
    bool ReconnectToPSMoveService();

    // Event Handling
    void HandleClientPSMoveEvent(const PSMMessage *event);
    void HandleConnectedToPSMoveService();
    void HandleFailedToConnectToPSMoveService();
    void HandleDisconnectedFromPSMoveService();
	static void HandleServiceVersionResponse(const PSMResponseMessage *response, void *userdata);
    void HandleControllerListChanged();
    void HandleTrackerListChanged();

    // Response Handling
    void HandleClientPSMoveResponse(const PSMMessage *message);
    void HandleControllerListReponse(const PSMControllerList *controller_list, const PSMResponseHandle response_handle);
    void HandleTrackerListReponse(const PSMTrackerList *tracker_list);
    
    void LaunchPSMoveMonitor_Internal( const char * pchDriverInstallDir );

	std::string m_strPSMoveHMDSerialNo;
	std::string m_strPSMoveServiceAddress;
	std::string m_strServerPort;

    bool m_bLaunchedPSMoveMonitor;
	bool m_bInitialized;

    std::vector< CPSMoveTrackedDeviceLatest * > m_vecTrackedDevices;

    // HMD Tracking Space
    PSMPosef m_worldFromDriverPose;
};

class CPSMoveTrackedDeviceLatest : public vr::ITrackedDeviceServerDriver
{
public:
    CPSMoveTrackedDeviceLatest();
    virtual ~CPSMoveTrackedDeviceLatest();

    // Shared Implementation of vr::ITrackedDeviceServerDriver
    virtual vr::EVRInitError Activate(vr::TrackedDeviceIndex_t unObjectId) override;
    virtual void Deactivate() override;
    virtual void EnterStandby() override;
    virtual void *GetComponent(const char *pchComponentNameAndVersion) override;
    virtual void DebugRequest(const char * pchRequest, char * pchResponseBuffer, uint32_t unResponseBufferSize) override;
    virtual vr::DriverPose_t GetPose() override;

    // CPSMoveTrackedDeviceLatest Interface
    virtual vr::ETrackedDeviceClass GetTrackedDeviceClass() const;
    virtual bool IsActivated() const;
    virtual void Update();
    virtual void RefreshWorldFromDriverPose();
    PSMPosef GetWorldFromDriverPose();
    virtual const char *GetSteamVRIdentifier() const;
	PSMQuatf AlignOrientationForMPU;
protected:
	vr::PropertyContainerHandle_t m_ulPropertyContainer;

    // Tracked device identification
    std::string m_strSteamVRSerialNo;

    // Assigned by vrserver upon Activate().  The same ID visible to clients
    vr::TrackedDeviceIndex_t m_unSteamVRTrackedDeviceId;

    // Cached for answering version queries from vrserver
    vr::DriverPose_t m_Pose;
    unsigned short m_firmware_revision;
    unsigned short m_hardware_revision;
};

class CPSMoveControllerLatest : public CPSMoveTrackedDeviceLatest, public vr::IVRControllerComponent
{
public:
	// Mirrors definition in PSMControllerType
    enum ePSControllerType
    {
        k_EPSControllerType_Move,
        k_EPSControllerType_Navi,
        k_EPSControllerType_DS4,
        k_EPSControllerType_Virtual,

		k_EPSControllerType_Count
    };

    enum ePSButtonID
    {
        k_EPSButtonID_0,
        k_EPSButtonID_1,
        k_EPSButtonID_2,
        k_EPSButtonID_3,
        k_EPSButtonID_4,
        k_EPSButtonID_5,
        k_EPSButtonID_6,
        k_EPSButtonID_7,
        k_EPSButtonID_8,
        k_EPSButtonID_9,
        k_EPSButtonID_10,
        k_EPSButtonID_11,
        k_EPSButtonID_12,
        k_EPSButtonID_13,
        k_EPSButtonID_14,
        k_EPSButtonID_15,
        k_EPSButtonID_16,
        k_EPSButtonID_17,
        k_EPSButtonID_18,
        k_EPSButtonID_19,
        k_EPSButtonID_20,
        k_EPSButtonID_21,
        k_EPSButtonID_22,
        k_EPSButtonID_23,
        k_EPSButtonID_24,
        k_EPSButtonID_25,
        k_EPSButtonID_26,
        k_EPSButtonID_27,
        k_EPSButtonID_28,
        k_EPSButtonID_29,
        k_EPSButtonID_30,
        k_EPSButtonID_31,

        k_EPSButtonID_Count,

        k_EPSButtonID_PS= k_EPSButtonID_0,
        k_EPSButtonID_Left= k_EPSButtonID_1,
        k_EPSButtonID_Up= k_EPSButtonID_2,
        k_EPSButtonID_Right= k_EPSButtonID_3,
        k_EPSButtonID_Down= k_EPSButtonID_4,
        k_EPSButtonID_Move= k_EPSButtonID_5,
        k_EPSButtonID_Trackpad= k_EPSButtonID_6,
        k_EPSButtonID_Trigger= k_EPSButtonID_7,
        k_EPSButtonID_Triangle= k_EPSButtonID_8,
        k_EPSButtonID_Square= k_EPSButtonID_9,
        k_EPSButtonID_Circle= k_EPSButtonID_10,
        k_EPSButtonID_Cross= k_EPSButtonID_11,
        k_EPSButtonID_Select= k_EPSButtonID_12,
        k_EPSButtonID_Share= k_EPSButtonID_13,
        k_EPSButtonID_Start= k_EPSButtonID_14,
        k_EPSButtonID_Options= k_EPSButtonID_15,
        k_EPSButtonID_L1= k_EPSButtonID_16,
        k_EPSButtonID_L2= k_EPSButtonID_17,
        k_EPSButtonID_L3= k_EPSButtonID_18,
        k_EPSButtonID_R1= k_EPSButtonID_19,
        k_EPSButtonID_R2= k_EPSButtonID_20,
        k_EPSButtonID_R3= k_EPSButtonID_21,
    };

	enum eVRTouchpadDirection
	{
		k_EVRTouchpadDirection_None,

		k_EVRTouchpadDirection_Left,
		k_EVRTouchpadDirection_Up,
		k_EVRTouchpadDirection_Right,
		k_EVRTouchpadDirection_Down,

		k_EVRTouchpadDirection_UpLeft,
		k_EVRTouchpadDirection_UpRight,
		k_EVRTouchpadDirection_DownLeft,
		k_EVRTouchpadDirection_DownRight,

		k_EVRTouchpadDirection_Count
	};


    CPSMoveControllerLatest(PSMControllerID psmControllerID, PSMControllerType psmControllerType, PSMControllerHand psmControllerHand, const char *psmSerialNo );
    virtual ~CPSMoveControllerLatest();

    // Overridden Implementation of vr::ITrackedDeviceServerDriver
    virtual vr::EVRInitError Activate(vr::TrackedDeviceIndex_t unObjectId) override;
    virtual void Deactivate() override;
    virtual void *GetComponent(const char *pchComponentNameAndVersion) override;

    // Implementation of vr::IVRControllerComponent
    virtual vr::VRControllerState_t GetControllerState() override;
    virtual bool TriggerHapticPulse( uint32_t unAxisId, uint16_t usPulseDurationMicroseconds ) override;

    // Overridden Implementation of CPSMoveTrackedDeviceLatest
    virtual vr::ETrackedDeviceClass GetTrackedDeviceClass() const override { return vr::TrackedDeviceClass_Controller; }
    virtual void Update() override;
	virtual void RefreshWorldFromDriverPose() override;

	// CPSMoveControllerLatest Interface 
    bool HasControllerId(int ControllerID);
	bool AttachChildPSMController(int ChildControllerId, PSMControllerType controllerType, const std::string &ChildControllerSerialNo);
    inline bool HasPSMControllerId(int ControllerID) const { return ControllerID == m_nPSMControllerId; }
	inline const PSMController * getPSMControllerView() const { return m_PSMControllerView; }
	inline std::string getPSMControllerSerialNo() const { return m_strPSMControllerSerialNo; }
	inline PSMControllerType getPSMControllerType() const { return m_PSMControllerType; }
private:
    typedef void ( vr::IVRServerDriverHost::*ButtonUpdate )( uint32_t unWhichDevice, vr::EVRButtonId eButtonId, double eventTimeOffset );

    void SendButtonUpdates( ButtonUpdate ButtonEvent, uint64_t ulMask );
	void RealignHMDTrackingSpace();
    void UpdateControllerState();
	void UpdateControllerStateFromPsMoveButtonState(ePSControllerType controllerType, ePSButtonID buttonId, PSMButtonState buttonState, vr::VRControllerState_t* pControllerStateToUpdate);
	void GetMetersPosInRotSpace(const PSMQuatf *rotation, PSMVector3f* outPosition);
    void UpdateTrackingState();
    void UpdateRumbleState();
	void UpdateBatteryChargeState(PSMBatteryState newBatteryEnum);

    // Controller State
    int m_nPSMControllerId;
	PSMControllerType m_PSMControllerType;
	PSMControllerHand m_psmControllerHand;
    PSMController *m_PSMControllerView;
	std::string m_strPSMControllerSerialNo;

    // Child Controller State
    int m_nPSMChildControllerId;
	PSMControllerType m_PSMChildControllerType;
    PSMController *m_PSMChildControllerView;
	std::string m_strPSMChildControllerSerialNo;

	// Used to report the controllers calibration status
	vr::ETrackingResult m_trackingStatus;

    // Used to ignore old state from PSM Service
    int m_nPoseSequenceNumber;

    // To main structures for passing state to vrserver
    vr::VRControllerState_t m_ControllerState;

    // Cached for answering version queries from vrserver
    bool m_bIsBatteryCharging;
    float m_fBatteryChargeFraction;

    // Rumble state
	bool m_bRumbleSuppressed;
    uint16_t m_pendingHapticPulseDuration;
    std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTimeRumbleSent;
    bool m_lastTimeRumbleSentValid;

	//virtual extend controller in meters
	float m_fVirtuallExtendControllersYMeters;
	float m_fVirtuallExtendControllersZMeters;

	// virtually rotate controller
	bool m_fVirtuallyRotateController;

	// delay in resetting touchpad position after touchpad press
	bool m_bDelayAfterTouchpadPress;

	// true while the touchpad is considered active (touched or pressed) 
	// after the initial touchpad delay, if any
	bool m_bTouchpadWasActive;

	std::chrono::time_point<std::chrono::high_resolution_clock> m_lastTouchpadPressTime;
	bool m_touchpadDirectionsUsed;

	std::chrono::time_point<std::chrono::high_resolution_clock> m_resetPoseButtonPressTime;
	bool m_bResetPoseRequestSent;
	std::chrono::time_point<std::chrono::high_resolution_clock> m_resetAlignButtonPressTime;
	bool m_bResetAlignRequestSent;

	bool m_bUsePSNaviDPadRecenter;
	bool m_bUsePSNaviDPadRealign;

    // Button Remapping
    vr::EVRButtonId psButtonIDToVRButtonID[k_EPSControllerType_Count][k_EPSButtonID_Count];
	eVRTouchpadDirection psButtonIDToVrTouchpadDirection[k_EPSControllerType_Count][k_EPSButtonID_Count];
    void LoadButtonMapping(
        vr::IVRSettings *pSettings,
		const CPSMoveControllerLatest::ePSControllerType controllerType,
        const CPSMoveControllerLatest::ePSButtonID psButtonID,
        const vr::EVRButtonId defaultVRButtonID,
		const eVRTouchpadDirection defaultTouchpadDirection,
		int controllerId = -1);
	bool LoadBool(vr::IVRSettings *pSettings, const char *pchSection, const char *pchSettingsKey, const bool bDefaultValue);
	int LoadInt(vr::IVRSettings *pSettings, const char *pchSection, const char *pchSettingsKey, const int iDefaultValue);
	float LoadFloat(vr::IVRSettings *pSettings, const char *pchSection, const char *pchSettingsKey, const float fDefaultValue);

	// Settings values. Used to determine whether we'll map controller movement after touchpad
	// presses to touchpad axis values.
	bool m_bUseSpatialOffsetAfterTouchpadPressAsTouchpadAxis;
	float m_fMetersPerTouchpadAxisUnits;

	// Settings value: used to determine how many meters in front of the HMD the controller
	// is held when it's being calibrated.
	float m_fControllerMetersInFrontOfHmdAtCalibration;

	// The position of the controller in meters in driver space relative to its own rotation
	// at the time when the touchpad was most recently pressed (after being up).
	PSMVector3f m_posMetersAtTouchpadPressTime;

	// The orientation of the controller in driver space at the time when
	// the touchpad was most recently pressed (after being up).
	PSMQuatf m_driverSpaceRotationAtTouchpadPressTime;

	// Flag used to completely disable the alignment gesture
	bool m_bDisableHMDAlignmentGesture;

	// Flag to tell if we should use the controller orientation as part of the controller alignment
	bool m_bUseControllerOrientationInHMDAlignment;

	// The axis to use for trigger input
	int m_steamVRTriggerAxisIndex;
	int m_steamVRNaviTriggerAxisIndex;

	// The axes to use for touchpad input (virtual controller only)
    int m_virtualTriggerAxisIndex;
	int m_virtualTouchpadXAxisIndex;
    int m_virtualTouchpadYAxisIndex;

	// The size of the deadzone for the controller's thumbstick
	float m_thumbstickDeadzone;

	// Treat a thumbstick touch also as a press
	bool m_bThumbstickTouchAsPress;

	// Settings values. Used to adjust throwing power using linear velocity and acceleration.
	float m_fLinearVelocityMultiplier;
	float m_fLinearVelocityExponent;

    // The button to use for controller hmd alignment
    ePSButtonID m_hmdAlignPSButtonID;

    // Override model to use for the controller
    std::string m_overrideModel;

    // Optional solver used to determine hand orientation
    class IHandOrientationSolver *m_orientationSolver;

    // Callbacks
    static void start_controller_response_callback(const PSMResponseMessage *response, void *userdata);

	SerialAccelerometer *serialAccelerometer;
	bool useOnlyYawForVirtualTrackerWithHMDOrientation;
	bool useSerialAccelerometer;
	bool disableExtendForAccelerometerDevices;
};

class CPSMoveTrackerLatest : public CPSMoveTrackedDeviceLatest
{
public:
    CPSMoveTrackerLatest(const PSMClientTrackerInfo *trackerInfo);
    virtual ~CPSMoveTrackerLatest();

    // Overridden Implementation of vr::ITrackedDeviceServerDriver
    virtual vr::EVRInitError Activate(vr::TrackedDeviceIndex_t unObjectId) override;
    virtual void Deactivate() override;

    // Overridden Implementation of CPSMoveTrackedDeviceLatest
    virtual vr::ETrackedDeviceClass GetTrackedDeviceClass() const override { return vr::TrackedDeviceClass_TrackingReference; }
    virtual void Update() override;

    bool HasTrackerId(int ControllerID);
    void SetClientTrackerInfo(const PSMClientTrackerInfo *trackerInfo);

private:
    // Which tracker
    int m_nTrackerId;

    // The static information about this tracker
    PSMClientTrackerInfo m_tracker_info;
};