#pragma once

#ifndef SerialAccelerometer_h
#define SerialAccelerometer_h

#include "SerialClass.h"
#include "json.hpp"
#include "ClientGeometry_CAPI.h"

class SerialAccelerometer
{
public:
	SerialAccelerometer(std::string initComPort, bool initWithBluetoothATCommand);
	void UpdateAccelerometerData();
	PSMQuatf GetAccelerometerQuaternion();
	bool IsConnected();
private:
	void GetDataFromSerialPort(char res);
	Serial* SerialPort;
	std::string COMPort;
	std::string messageFromSerialPort;
	std::string lastGoodMessageFromSerialPort;
	bool isStartRecieved = false;
	nlohmann::json lastJson;
	const char BluetoothAtCommand[4] = "AT";
	bool InitWithBluetoothATCommand;
	bool IsConnectionLost = false;
	int readResultZero = 0;
};

#endif