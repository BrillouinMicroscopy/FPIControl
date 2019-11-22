#ifndef KCUBEPIEZO_H
#define KCUBEPIEZO_H

#include <QMainWindow>
#include <QtCore/QObject>
#include <QtWidgets>

#include <Thorlabs.MotionControl.KCube.Piezo.h>
#include <cmath>

typedef struct {
	short maxVoltage{ 750 };											// maximum output voltage
	PZ_InputSourceFlags source{ PZ_InputSourceFlags::PZ_Potentiometer };// voltage input source
	PZ_ControlModeTypes mode{ PZ_ControlModeTypes::PZ_OpenLoop };		// mode type
	bool enabled{ true };
} PIEZO_SETTINGS;

class kcubepiezo : public QObject {
	Q_OBJECT

public:
	void setDefaults();
	void setVoltage(double voltage);
	double getVoltage();
	void setVoltageIncrement(int voltage);
	int getVoltageIncrement();
	void setVoltageSource(PZ_InputSourceFlags source);
	void incrementVoltage(int direction);
	void storeOutputVoltageIncrement();
	void restoreOutputVoltageIncrement();

	char const * serialNo{ "29501039" };	// serial number of the KCube Piezo device (can be found in Kinesis) TODO: make this a changeable parameter

	PIEZO_SETTINGS defaultSettings;

public slots:
	void init() {};
	void connect();
	void disconnect();
	void enable();
	void disable();

private:
	bool m_isConnected{ false };
	bool m_isEnabled{ false };
	int m_outputVoltageIncrement{ 0 };

signals:
	void connected(bool);
	void settingsChanged(PIEZO_SETTINGS);
};

#endif // KCUBEPIEZO_H