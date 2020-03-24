
#include "SerialAccelerometer.h"
#include "SerialClass.h"
#include "json.hpp"
#include <exception>

using json = nlohmann::json;

SerialAccelerometer::SerialAccelerometer(std::string initComPort, bool initBluetoothWithATCommand)
{
	COMPort = initComPort;
	InitWithBluetoothATCommand = initBluetoothWithATCommand;

	SerialPort = new Serial(COMPort.c_str());

	messageFromSerialPort = "";
	lastGoodMessageFromSerialPort = "";
	lastJson = NULL;
}

void SerialAccelerometer::GetDataFromSerialPort(char res)
{
	if (res == '\0')
		return;

	if (res == '{' && isStartRecieved == false)
	{
		messageFromSerialPort = "";
		messageFromSerialPort.clear();
		isStartRecieved = true;
		messageFromSerialPort += res;
	}
	else if (res == '}' && isStartRecieved == true)
	{
		messageFromSerialPort += res;
		lastGoodMessageFromSerialPort.clear();

		for (int i = 0; i < messageFromSerialPort.size(); i++)
			lastGoodMessageFromSerialPort.push_back(messageFromSerialPort[i]);

		messageFromSerialPort.clear();
		isStartRecieved = false;
	}
	else if (res == '{' && isStartRecieved == true)
	{
		messageFromSerialPort.clear();
		messageFromSerialPort += res;
	}
	else if (isStartRecieved == true)
		messageFromSerialPort += res;
}

void SerialAccelerometer::UpdateAccelerometerData()
{
	try
	{
		if (IsConnected())
		{
			char incomingData[64] = "";
			int dataLength = 63;
			int readResult = 0;

			readResult = SerialPort->ReadData(incomingData, dataLength);

			if (readResult == 0)
			{
				if (InitWithBluetoothATCommand)
					SerialPort->WriteData(BluetoothAtCommand, 4);

				readResultZero++;

				if (readResultZero >= 50)
				{
					IsConnectionLost = true;
					SerialPort->CloseCom();
					readResultZero = 0;
				}
			}
			else
			{
				InitWithBluetoothATCommand = false;
				readResultZero = 0;
			}

			incomingData[readResult] = 0;

			for each (char var in incomingData)
			{
				GetDataFromSerialPort(var);
			}
		}
		else
		{
			SerialPort = new Serial(COMPort.c_str());
			IsConnectionLost = false;
			InitWithBluetoothATCommand = true;
		}
	}
	catch (std::exception& e)
	{
		IsConnectionLost = true;
	}
}

PSMQuatf SerialAccelerometer::GetAccelerometerQuaternion()
{
	if (lastGoodMessageFromSerialPort.length() > 1)
	{
		json j_string = NULL;

		try
		{
			j_string = json::parse(lastGoodMessageFromSerialPort.c_str());
		}
		catch (std::exception& e)
		{
			j_string = lastJson;
		}

		if (j_string == NULL)
			j_string = lastJson;

		if (j_string != NULL)
		{
			auto qx = j_string["X"].get<float>();
			auto qy = j_string["Y"].get<float>();
			auto qz = j_string["Z"].get<float>();
			auto qw = j_string["W"].get<float>();

			lastJson = j_string;

			return PSM_QuatfCreate(qw, qy, qz, qx);
		}
	}
	return PSM_QuatfCreate(1, 0, 0 ,0);
}

bool SerialAccelerometer::IsConnected()
{
	return SerialPort->IsConnected() && !IsConnectionLost;
}