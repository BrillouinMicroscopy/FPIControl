#include "kcubepiezo.h"

void kcubepiezo::connect_device() {
	PCC_Open(serialNo);
	// start the device polling at 200ms intervals
	PCC_StartPolling(serialNo, 200);
	m_isConnected = true;
	// set default values
	setDefaults();
	emit(connected(m_isConnected));
}

void kcubepiezo::disconnect_device() {
	// stop polling
	PCC_StopPolling(serialNo);
	PCC_Close(serialNo);
	m_isConnected = false;
	emit(connected(m_isConnected));
}

void kcubepiezo::enable() {
	PCC_Enable(serialNo);
	defaultSettings.enabled = true;
}

void kcubepiezo::disable() {
	PCC_Disable(serialNo);
	defaultSettings.enabled = false;
}

void kcubepiezo::setDefaults() {
	//PCC_SetZero(serialNo);										// set zero reference voltage
	PCC_SetPositionControlMode(serialNo, defaultSettings.mode);		// set open loop mode
	PCC_SetMaxOutputVoltage(serialNo, defaultSettings.maxVoltage);	// set maximum output voltage
	PCC_SetVoltageSource(serialNo, defaultSettings.source);			// set voltage source
	if (defaultSettings.enabled) {
		enable();
	} else {
		disable();
	}
	settingsChanged(defaultSettings);
}

double kcubepiezo::getVoltage() {
	double maxVoltage = PCC_GetMaxOutputVoltage(serialNo);
	double relativeVoltage = PCC_GetOutputVoltage(serialNo);
	return maxVoltage * relativeVoltage / (pow(2, 15) - 1) / 10;	// [V] output voltage
}

void kcubepiezo::setVoltageSource(PZ_InputSourceFlags source) {
	PCC_SetVoltageSource(serialNo, source);
}

void kcubepiezo::setVoltage(double voltage) {
	double maxVoltage = PCC_GetMaxOutputVoltage(serialNo);
	double relativeVoltage = voltage / maxVoltage * (pow(2, 15) - 1) * 10;
	PCC_SetOutputVoltage(serialNo, (int)relativeVoltage);
}

void kcubepiezo::incrementVoltage(int direction) {
	outputVoltageIncrement += direction;
	PCC_SetOutputVoltage(serialNo, outputVoltageIncrement);
}

void kcubepiezo::storeOutputVoltageIncrement() {
	outputVoltageIncrement = getVoltageIncrement();
}

void kcubepiezo::restoreOutputVoltageIncrement() {
	PCC_SetOutputVoltage(serialNo, outputVoltageIncrement);
}

void kcubepiezo::setVoltageIncrement(int voltage) {
	PCC_SetOutputVoltage(serialNo, voltage);
}

int kcubepiezo::getVoltageIncrement() {
	return PCC_GetOutputVoltage(serialNo);
}
