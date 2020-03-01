
#include "SerialAccelerometer.h"
#include "SerialClass.h"
#include "json.hpp"
#include <exception>

using json = nlohmann::json;

SerialAccelerometer::SerialAccelerometer(std::string initComPort, bool initBluetoothWithATCommand)
{
	COMPort = initComPort;
	InitWithBluetoothATCommand = initBluetoothWithATCommand;

	try
	{
		SerialPort = new Serial(COMPort.c_str());
	}
	catch (std::exception& e)
	{
	}

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
		if (SerialPort->IsConnected())
		{
			char incomingData[256] = "";
			int dataLength = 255;
			int readResult = 0;

			readResult = SerialPort->ReadData(incomingData, dataLength);

			if (readResult == 0)
			{
				if (InitWithBluetoothATCommand)
					SerialPort->WriteData(BluetoothAtCommand, 4);
			}

			incomingData[readResult] = 0;

			for each (char var in incomingData)
			{
				GetDataFromSerialPort(var);
			}
		}
		else
		{
			try
			{
				SerialPort = new Serial(COMPort.c_str());
			}
			catch (std::exception& e)
			{
				throw new std::exception("Not connected to COM");
			}
		}
	}
	catch (std::exception& e)
	{
		try
		{
			SerialPort = new Serial(COMPort.c_str());
		}
		catch (std::exception& e)
		{
			throw new std::exception("Not connected to COM");
		}
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
			auto qx = j_string["QX"].get<float>();
			auto qy = j_string["QY"].get<float>();
			auto qz = j_string["QZ"].get<float>();
			auto qw = j_string["QW"].get<float>();

			lastJson = j_string;

			return PSM_QuatfCreate(qw, qy, qz, qx);
		}
	}
}