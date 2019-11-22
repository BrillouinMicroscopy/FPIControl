#include "daq_PS2000.h"
#include <QtWidgets>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>

#include "daq_PS2000.h"

/*
 * Public definitions
 */

daq::daq(QObject *parent) :
	QObject(parent) {
}

daq::daq(QObject *parent, std::vector<int32_t> ranges, std::vector<int> timebases, double maxSamplingRate) :
	QObject(parent), m_input_ranges(ranges), m_availableTimebases(timebases), m_maxSamplingRate(maxSamplingRate) {
}

std::vector<double> daq::getSamplingRates() {
	return m_availableSamplingRates;
}

ACQUISITION_PARAMETERS daq::getAcquisitionParameters() {
	return m_acquisitionParameters;
}

void daq::setSampleRate(int index) {
	m_acquisitionParameters.timebaseIndex = index;
	m_acquisitionParameters.timebase = m_availableTimebases[index];
	setAcquisitionParameters();
}

void daq::setCoupling(int coupling, int ch) {
	m_acquisitionParameters.channelSettings[ch].coupling = coupling;
	m_unitOpened.channelSettings[ch].coupling = coupling;
	set_defaults();
}

void daq::setRange(int index, int ch) {
	if (index < 9) {
		m_acquisitionParameters.channelSettings[ch].enabled = true;
		m_unitOpened.channelSettings[ch].enabled = true;
		m_acquisitionParameters.channelSettings[ch].range = index + 2;
		m_unitOpened.channelSettings[ch].range = index + 2;
	} else if (index == 9) {
		// set auto range
	} else {
		m_acquisitionParameters.channelSettings[ch].enabled = false;
		m_unitOpened.channelSettings[ch].enabled = false;
	}
	set_defaults();
}

void daq::setNumberSamples(int32_t no_of_samples) {
	m_acquisitionParameters.no_of_samples = no_of_samples;
	setAcquisitionParameters();
}

/*
 * Public slots
 */

void daq::init() {
	// create timers and connect their signals
	// after moving daq_PS2000 to another thread
	timer = new QTimer();
	QMetaObject::Connection connection = QWidget::connect(
		timer,
		&QTimer::timeout,
		this,
		&daq::getBlockData
	);
}

void daq::startStopAcquisition() {
	if (timer->isActive()) {
		timer->stop();
		m_acquisitionRunning = false;
	} else {
		setAcquisitionParameters();
		timer->start(20);
		m_acquisitionRunning = true;
	}
	emit(s_acquisitionRunning(m_acquisitionRunning));
}

/*
 * Protected definitions
 */

int32_t daq::adc_to_mv(int32_t raw, int32_t ch) {
	return (m_scale_to_mv) ? (raw * m_input_ranges[ch]) / 32767 : raw;
}

int16_t daq::mv_to_adc(int16_t mv, int16_t ch) {
	return ((mv * 32767) / m_input_ranges[ch]);
}

/*
 * Protected slots
 */

void daq::getBlockData() {
	std::array<std::vector<int32_t>, DAQ_MAX_CHANNELS> values = collectBlockData();

	//m_liveBuffer->m_freeBuffers->acquire();
	int16_t** buffer = m_liveBuffer->getWriteBuffer();
	for (gsl::index channel{ 0 }; channel < (gsl::index)values.size(); channel++) {
		for (gsl::index jj{ 0 }; jj < (gsl::index)values[channel].size(); jj++) {
			buffer[channel][jj] = values[channel][jj];
		}
	}
	m_liveBuffer->m_usedBuffers->release();

	emit collectedBlockData();
}