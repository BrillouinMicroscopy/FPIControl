#include "kcubepiezo.h"

/*
 * Public definitions
 */



kcubepiezo::kcubepiezo(std::string serialNo) : m_serialNo(serialNo) {}

void kcubepiezo::setDefaults() {
	//PCC_SetZero(serialNo);										// set zero reference voltage
	PCC_SetPositionControlMode(m_serialNo.c_str(), defaultSettings.mode);		// set open loop mode
	PCC_SetMaxOutputVoltage(m_serialNo.c_str(), defaultSettings.maxVoltage);	// set maximum output voltage
	PCC_SetVoltageSource(m_serialNo.c_str(), defaultSettings.source);			// set voltage source
	PCC_SetHubAnalogInput(m_serialNo.c_str(), defaultSettings.driveInput);		// set the drive input
	if (defaultSettings.enabled) {
		enable();
	} else {
		disable();
	}
	restoreOutputVoltageIncrement();
	emit(settingsChanged(defaultSettings));
}

void kcubepiezo::setVoltage(double voltage) {
	double maxVoltage = PCC_GetMaxOutputVoltage(m_serialNo.c_str());
	double relativeVoltage = voltage / maxVoltage * (pow(2, 15) - 1) * 10;
	m_outputVoltageIncrement = (int)relativeVoltage;
	PCC_SetOutputVoltage(m_serialNo.c_str(), (int)m_outputVoltageIncrement);
}

double kcubepiezo::getVoltage() {
	double maxVoltage = PCC_GetMaxOutputVoltage(m_serialNo.c_str());
	double relativeVoltage = getVoltageIncrement();
	return maxVoltage * relativeVoltage / (pow(2, 15) - 1) / 10;	// [V] output voltage
}

void kcubepiezo::setVoltageIncrement(int voltage) {
	PCC_SetOutputVoltage(m_serialNo.c_str(), voltage);
}

int kcubepiezo::getVoltageIncrement() {
	return m_outputVoltageIncrement;
	//return PCC_GetOutputVoltage(serialNo);
}

void kcubepiezo::setVoltageSource(PZ_InputSourceFlags source) {
	PCC_SetVoltageSource(m_serialNo.c_str(), source);
}

void kcubepiezo::incrementVoltage(int direction) {
	m_outputVoltageIncrement += direction;
	PCC_SetOutputVoltage(m_serialNo.c_str(), m_outputVoltageIncrement);
}

void kcubepiezo::storeOutputVoltageIncrement() {
	//m_outputVoltageIncrement = getVoltageIncrement();
}

void kcubepiezo::restoreOutputVoltageIncrement() {
	PCC_SetOutputVoltage(m_serialNo.c_str(), m_outputVoltageIncrement);
}

/*
 * Public slots
 */

void kcubepiezo::connect() {
	// device ID for KCubePiezo
	int deviceID{ 81 };

	// Build list of connected device
	if (TLI_BuildDeviceList() == 0) {
		// get device list size 
		TLI_GetDeviceListSize();
		// get BBD serial numbers
		char serialNos[100];
		TLI_GetDeviceListByTypeExt(serialNos, 100, deviceID);

		// output list of matching devices
		{
			char* searchContext{ nullptr };
			char* p = strtok_s(serialNos, ",", &searchContext);

			while (p != nullptr) {
				TLI_DeviceInfo deviceInfo;
				// get device info from device
				TLI_GetDeviceInfo(p, &deviceInfo);
				// get strings from device info structure
				char desc[65];
				strncpy_s(desc, deviceInfo.description, 64);
				desc[64] = '\0';
				char serialNo[9];
				strncpy_s(serialNo, deviceInfo.serialNo, 8);
				serialNo[8] = '\0';
				// output
				p = strtok_s(nullptr, ",", &searchContext);
			}
		}
	}

	if (PCC_Open(m_serialNo.c_str()) == 0) {
		// start the device polling at 200ms intervals
		PCC_StartPolling(m_serialNo.c_str(), 200);
		m_isConnected = true;
		// set default values
		setDefaults();
		emit(connected(m_isConnected));
	}
}

void kcubepiezo::disconnect() {
	// stop polling
	PCC_StopPolling(m_serialNo.c_str());
	PCC_Close(m_serialNo.c_str());
	m_isConnected = false;
	emit(connected(m_isConnected));
}

void kcubepiezo::enable() {
	PCC_Enable(m_serialNo.c_str());
	defaultSettings.enabled = true;
}

void kcubepiezo::disable() {
	PCC_Disable(m_serialNo.c_str());
	defaultSettings.enabled = false;
}