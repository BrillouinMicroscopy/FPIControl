#include "locking.h"
#include <QtWidgets>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>

Locking::Locking(QObject *parent, daq **dataAcquisition, kcubepiezo **piezoControl) :
	QObject(parent), m_dataAcquisition(dataAcquisition), m_piezoControl(piezoControl) {

	// Calculate the maximum storage size and resize the arrays accordingly
	lockData.storageSize = (int)((1000 * lockData.storageDuration) / lockSettings.lockingTimeout);
	lockData.time.resize(lockData.storageSize);
	lockData.voltageDaq.resize(lockData.storageSize);
	lockData.voltagePiezo.resize(lockData.storageSize);
	lockData.amplitude.resize(lockData.storageSize);
	lockData.error.resize(lockData.storageSize);
	lockData.startTime = std::chrono::system_clock::now();
}

void Locking::startStopAcquireLocking() {
	if (lockingTimer->isActive()) {
		setLockState(LOCKSTATE::INACTIVE);
		m_isAcquireLockingRunning = false;
		lockingTimer->stop();
	} else {
		m_isAcquireLockingRunning = true;
		lockingTimer->start(lockSettings.lockingTimeout);
	}
	emit(s_acquireLockingRunning(m_isAcquireLockingRunning));
}

void Locking::startStopLocking() {
	if (lockSettings.state != LOCKSTATE::ACTIVE) {
		// set integral error to zero before starting to lock
		lockData.iError = 0;
		// store and immediately restore output voltage
		//m_piezoControl->storeOutputVoltageIncrement();
		// this is necessary, because it seems, that getting the output voltage takes the external signal into account
		// whereas setting it does not
		//m_piezoControl->restoreOutputVoltageIncrement();
		(*m_piezoControl)->setVoltageSource(PZ_InputSourceFlags::PZ_ExternalSignal);
		m_piezoVoltage = (*m_piezoControl)->getVoltage();
		setLockState(LOCKSTATE::ACTIVE);
	} else {
		disableLocking(LOCKSTATE::INACTIVE);
	}
}

void Locking::toggleOffsetCompensation(bool compensate) {
	lockSettings.compensate = compensate;
}

void Locking::disableLocking(LOCKSTATE lockstate) {
	(*m_piezoControl)->setVoltageSource(PZ_InputSourceFlags::PZ_ExternalSignal);
	m_daqVoltage = 0;
	// set output voltage of the DAQ
	(*m_dataAcquisition)->setOutputVoltage(m_daqVoltage);
	lockSettings.compensating = false;
	emit(compensationStateChanged(false));
	
	setLockState(lockstate);
}

void Locking::setLockState(LOCKSTATE lockstate) {
	lockSettings.state = lockstate;
	emit(lockStateChanged(lockSettings.state));
}

SCAN_SETTINGS Locking::getScanSettings() {
	return scanSettings;
}

void Locking::setScanParameters(SCANPARAMETERS type, double value) {
	switch (type) {
		case SCANPARAMETERS::LOW:
			scanSettings.low = value;
			break;
		case SCANPARAMETERS::HIGH:
			scanSettings.high = value;
			break;
		case SCANPARAMETERS::STEPS:
			scanSettings.nrSteps = value;
			break;
		case SCANPARAMETERS::INTERVAL:
			scanSettings.interval = value;
			break;
	}
}

void Locking::setLockParameters(LOCKPARAMETERS type, double value) {
	switch (type) {
		case LOCKPARAMETERS::P:
			lockSettings.proportional = value;
			break;
		case LOCKPARAMETERS::I:
			lockSettings.integral = value;
			break;
		case LOCKPARAMETERS::D:
			lockSettings.derivative = value;
			break;
		case LOCKPARAMETERS::FREQUENCY:
			lockSettings.frequency = value;
			break;
		case LOCKPARAMETERS::PHASE:
			lockSettings.phase = value;
			break;
	}
}

void Locking::startScan() {
	if (scanTimer->isActive()) {
		scanData.m_running = false;
		scanTimer->stop();
		emit s_scanRunning(scanData.m_running);
	} else {
		// prepare data arrays
		scanData.nrSteps = scanSettings.nrSteps;
		scanData.voltages = generalmath::linspace<double>(scanSettings.low, scanSettings.high, scanSettings.nrSteps);

		scanData.intensity.resize(scanSettings.nrSteps);
		scanData.error.resize(scanSettings.nrSteps);
		std::fill(scanData.intensity.begin(), scanData.intensity.end(), NAN);
		std::fill(scanData.error.begin(), scanData.error.end(), NAN);

		(*m_dataAcquisition)->setAcquisitionParameters();

		scanData.pass = 0;
		scanData.m_running = true;
		scanData.m_abort = false;
		// set piezo voltage to start value
		(*m_piezoControl)->setVoltage(scanData.voltages[scanData.pass]);
		passTimer.start();
		scanTimer->start(1000);
		emit s_scanRunning(scanData.m_running);
	}
}

void Locking::scan() {
	//abort scan if wanted
	if (scanData.m_abort) {
		scanData.m_running = false;
		scanTimer->stop();
		emit s_scanRunning(scanData.m_running);
	}

	// acquire new datapoint when interval has passed
	if (passTimer.elapsed() < (scanSettings.interval*1e3)) {
		return;
	}

	// reset timer when enough time has passed
	passTimer.start();

	// acquire detector and reference signal, store and process it
	std::array<std::vector<int32_t>, DAQ_MAX_CHANNELS> values = (*m_dataAcquisition)->collectBlockData();

	std::vector<double> tau(values[0].begin(), values[0].end());
	std::vector<double> reference(values[1].begin(), values[1].end());

	double tau_max = generalmath::maximum(tau);
	double tau_min = generalmath::minimum(tau);

	// normalize transmission signal
	std::transform(tau.begin(), tau.end(), tau.begin(),
		[tau_max, tau_min](double &el) {
			return (el - tau_min) / (tau_max - tau_min);
		}
	);

	scanData.intensity[scanData.pass] = generalmath::absSum(tau);
	scanData.error[scanData.pass] = pdh.getError(tau, reference);

	++scanData.pass;
	emit s_scanPassAcquired();
	// if scan is not done, set temperature to new value, else annouce finished scan
	if (scanData.pass < scanData.nrSteps) {
		(*m_piezoControl)->setVoltage(scanData.voltages[scanData.pass]);
	} else {
		scanData.m_running = false;
		scanTimer->stop();
		emit s_scanRunning(scanData.m_running);
	}
}

LOCK_SETTINGS Locking::getLockSettings() {
	return lockSettings;
}

void Locking::lock() {
	std::array<std::vector<int32_t>, DAQ_MAX_CHANNELS> values = (*m_dataAcquisition)->collectBlockData();

	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

	std::vector<double> tau(values[0].begin(), values[0].end());
	std::vector<double> reference(values[1].begin(), values[1].end());

	//double tau_mean = generalmath::mean(tau);
	double tau_max = generalmath::maximum(tau);
	double tau_min = generalmath::minimum(tau);
	double amplitude = tau_max - tau_min;

	if (amplitude != 0) {
		for (int kk(0); kk < tau.size(); kk++) {
			tau[kk] = (tau[kk] - tau_min) / amplitude;
		}
	}

	// adjust for requested phase
	ACQUISITION_PARAMETERS acquisitionParameters = (*m_dataAcquisition)->getAcquisitionParameters();
	double samplingRate = (*m_dataAcquisition)->getCurrentSamplingRate();
	int phaseStep = (int)lockSettings.phase * samplingRate / (360 * lockSettings.frequency);
	if (phaseStep > 0) {
		// simple rotation to the left
		std::rotate(reference.begin(), reference.begin() + phaseStep, reference.end());
	} else if (phaseStep < 0) {
		// simple rotation to the right
		std::rotate(reference.rbegin(), reference.rbegin() + phaseStep, reference.rend());
	}

	double error = pdh.getError(tau, reference);


	if (lockSettings.state == LOCKSTATE::ACTIVE) {
		double dError = 0;
		if (lockData.error.size() > 0) {
			auto prevIndex = generalmath::indexWrapped((int)lockData.nextIndex - 1, lockData.storageSize);
			double dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - lockData.time[prevIndex]).count() / 1e3;
			lockData.iError += 1e-3 * lockSettings.integral * (lockData.error[prevIndex] + error) * (dt) / 2;
			dError = (error - lockData.error[prevIndex]) / dt;
		}
		m_daqVoltage += (1e-3 * lockSettings.proportional * error + lockData.iError + 1e-3 * lockSettings.derivative * dError) / 100;

		// check if offset compensation is necessary and set piezo voltage
		if (lockSettings.compensate) {
			m_compensationTimer++;
			if (abs(m_daqVoltage) > lockSettings.maxOffset) {
				lockSettings.compensating = true;
				emit(compensationStateChanged(true));
			}
			if (abs(m_daqVoltage) < lockSettings.targetOffset) {
				lockSettings.compensating = false;
				emit(compensationStateChanged(false));
			}
			if (lockSettings.compensating & (m_compensationTimer > lockSettings.compensationTimeout)) {
				m_compensationTimer = 0;
				if (m_daqVoltage > 0) {
					(*m_piezoControl)->incrementVoltage(1);
					m_piezoVoltage = (*m_piezoControl)->getVoltage();
				} else {
					(*m_piezoControl)->incrementVoltage(-1);
					m_piezoVoltage = (*m_piezoControl)->getVoltage();
				}
			}
		} else {
			lockSettings.compensating = false;
			emit(compensationStateChanged(false));
		}

		// abort locking if
		// - output voltage is over 2 V
		// - maximum of the signal amplitude in the last 50 measurements is below 0.05 V
		if ((abs(m_daqVoltage) > 2)) {
		//if ((abs(m_daqVoltage) > 2) || (generalmath::floatingMax(lockData.amplitude, 50) / static_cast<double>(1000) < 0.05)) {
			Locking::disableLocking(LOCKSTATE::FAILURE);
		}

		// set output voltage of the DAQ
		(*m_dataAcquisition)->setOutputVoltage(m_daqVoltage);
	}

	// write data to struct for storage
	lockData.amplitude[lockData.nextIndex] = amplitude;
	lockData.time[lockData.nextIndex] = now;
	lockData.error[lockData.nextIndex] = error;
	lockData.voltageDaq[lockData.nextIndex] = m_daqVoltage;
	lockData.voltagePiezo[lockData.nextIndex] = m_piezoVoltage;
	lockData.nextIndex++;

	// If the next index to write to is outside of the array, we wrap around to the start
	if (lockData.nextIndex >= lockData.storageSize) {
		lockData.nextIndex = 0;
	}

	emit(locked());
}

void Locking::init() {
	// create timers and connect their signals
	// after moving locking to another thread
	lockingTimer = new QTimer();
	scanTimer = new QTimer();
	QMetaObject::Connection connection;
	connection = QWidget::connect(
		lockingTimer,
		&QTimer::timeout,
		this,
		&Locking::lock
	);
	connection = QWidget::connect(
		scanTimer,
		&QTimer::timeout,
		this,
		&Locking::scan
	);
}