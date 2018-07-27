#ifndef LOCKING_H
#define LOCKING_H

#include <QMainWindow>
#include <QtCore/QObject>
#include <QtWidgets>
#include <vector>
#include <array>
#include <chrono>
#include <ctime>

#include "daq.h"
#include "PDH.h"
#include "kcubepiezo.h"
#include "generalmath.h"

typedef struct {
	double low = 0;			// [K] offset start
	double high = 7;		// [K] offset end
	int32_t	nrSteps = 1000;	// number of steps
	double interval = 0.1;	// [s] interval between steps
} SCAN_SETTINGS;

typedef struct {
	bool m_running = false;				// is the scan currently running
	bool m_abort = false;				// should the scan be aborted
	int32_t nrSteps{ 0 };
	int pass{ 0 };
	std::vector<double> voltages;	// [µV] output voltage (<int32_t> is sufficient for this)
	std::vector<int32_t> intensity;	// [µV] measured intensity (<int32_t> is fine)
	std::vector<double> error;		// PDH error signal
} SCAN_DATA;

typedef enum enLockState {
	INACTIVE,
	ACTIVE,
	FAILURE
} LOCKSTATE;

typedef struct {
	double proportional = 0.01;		//		control parameter of the proportional part
	double integral = 0.005;		//		control parameter of the integral part
	double derivative = 0.0;		//		control parameter of the derivative part
	double frequency = 5000;		// [Hz] approx. frequency of the reference signal
	double phase = 0;				// [°]	phase shift between reference and detector signal
	bool compensate = true;			//		compensate the offset?
	bool compensating = false;		//		is it currently compensating?
	double maxOffset = 0.4;			// [V]	maximum voltage of the external input before the offset compensation kicks in
	double targetOffset = 0.1;		// [V]	target voltage of the offset compensation
	LOCKSTATE state = LOCKSTATE::INACTIVE;	//		locking enabled?
} LOCK_SETTINGS;

typedef struct {
	std::vector<std::chrono::time_point<std::chrono::system_clock>> time;		// [s]	time vector
	std::vector<int32_t> voltage;	// [µV]	output voltage (<int32_t> is sufficient for this)
	std::vector<int32_t> amplitude;	// [µV]	measured intensity (<int32_t> is fine)
	std::vector<double> error;		// [1]	PDH error signal
	double iError = 0;				// [1]	integral value of the error signal
} LOCK_DATA;

enum class liveViewPlotTypes {
	CHANNEL_A,
	CHANNEL_B,
	COUNT
};

enum class scanViewPlotTypes {
	INTENSITY,
	ERRORSIGNAL,
	COUNT
};

enum class lockViewPlotTypes {
	VOLTAGE,
	ERRORSIGNAL,
	AMPLITUDE,
	PIEZOVOLTAGE,
	ERRORSIGNALMEAN,
	ERRORSIGNALSTD,
	COUNT
};

typedef enum enScanParameters {
	LOW,
	HIGH,
	STEPS,
	INTERVAL
} SCANPARAMETERS;

typedef enum enLockParameters {
	P,
	I,
	D,
	FREQUENCY,
	PHASE
} LOCKPARAMETERS;

class Locking : public QObject {
	Q_OBJECT

	public:
		explicit Locking(QObject *parent, daq **dataAcquisition, kcubepiezo *piezoControl);
		void setLockState(LOCKSTATE lockstate = LOCKSTATE::INACTIVE);
		void setScanParameters(SCANPARAMETERS type, double value);
		void setLockParameters(LOCKPARAMETERS type, double value);
		SCAN_SETTINGS getScanSettings();
		SCAN_DATA scanData;
		LOCK_SETTINGS getLockSettings();

		std::array<QVector<QPointF>, static_cast<int>(lockViewPlotTypes::COUNT)> m_lockDataPlot;

	public slots:
		void init();
		void startScan();
		void startStopAcquireLocking();
		void startStopLocking();

		void toggleOffsetCompensation(bool);

	private slots:
		void lock();
		void scan();

	signals:
		void s_scanRunning(bool);
		void s_scanPassAcquired();
		void s_acquireLockingRunning(bool);
		void locked();
		void lockStateChanged(LOCKSTATE);
		void compensationStateChanged(bool);

	private:
		kcubepiezo *m_piezoControl;
		daq **m_dataAcquisition;
		PDH pdh;
		bool m_acquisitionRunning = false;
		bool m_isAcquireLockingRunning = false;
		QTimer *lockingTimer = nullptr;
		QTimer *scanTimer = nullptr;
		QElapsedTimer passTimer;
		SCAN_SETTINGS scanSettings;
		LOCK_SETTINGS lockSettings;
		LOCK_DATA lockData;

		double daqVoltage = 0;
		double piezoVoltage = 0;
		int compensationTimer = 0;
		
		void Locking::disableLocking(LOCKSTATE lockstate);
};

#endif // LOCKING_H