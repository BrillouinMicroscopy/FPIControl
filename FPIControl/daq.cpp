#include "daq.h"
#include "kcubepiezo.h"
#include "PDH.h"
#include <QtWidgets>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>

/* Definitions of PS2000 driver routines */
#include "ps2000.h"

int16_t		values_a[BUFFER_SIZE]; // block mode buffer, Channel A
int16_t		values_b[BUFFER_SIZE]; // block mode buffer, Channel B

int16_t		overflow;
int32_t		scale_to_mv = 1;

int16_t		channel_mv[PS2000_MAX_CHANNELS];

int16_t		g_overflow = 0;

// Streaming data parameters
int16_t		g_triggered = 0;
uint32_t	g_triggeredAt = 0;
uint32_t	g_nValues;
uint32_t	g_startIndex;			// Start index in application buffer where data should be written to in streaming mode collection
uint32_t	g_prevStartIndex;		// Keep track of previous index into application buffer in streaming mode collection
int16_t		g_appBufferFull = 0;	// Use this in the callback to indicate if it is going to copy past the end of the buffer

typedef enum {
	MODEL_NONE = 0,
	MODEL_PS2104 = 2104,
	MODEL_PS2105 = 2105,
	MODEL_PS2202 = 2202,
	MODEL_PS2203 = 2203,
	MODEL_PS2204 = 2204,
	MODEL_PS2205 = 2205,
	MODEL_PS2204A = 0xA204,
	MODEL_PS2205A = 0xA205
} MODEL_TYPE;

typedef struct {
	PS2000_THRESHOLD_DIRECTION	channelA;
	PS2000_THRESHOLD_DIRECTION	channelB;
	PS2000_THRESHOLD_DIRECTION	channelC;
	PS2000_THRESHOLD_DIRECTION	channelD;
	PS2000_THRESHOLD_DIRECTION	ext;
} DIRECTIONS;

typedef struct {
	PS2000_PWQ_CONDITIONS			*	conditions;
	int16_t							nConditions;
	PS2000_THRESHOLD_DIRECTION		direction;
	uint32_t						lower;
	uint32_t						upper;
	PS2000_PULSE_WIDTH_TYPE			type;
} PULSE_WIDTH_QUALIFIER;

typedef struct {
	PS2000_CHANNEL channel;
	float threshold;
	int16_t direction;
	float delay;
} SIMPLE;

typedef struct {
	int16_t hysteresis;
	DIRECTIONS directions;
	int16_t nProperties;
	PS2000_TRIGGER_CONDITIONS * conditions;
	PS2000_TRIGGER_CHANNEL_PROPERTIES * channelProperties;
	PULSE_WIDTH_QUALIFIER pwq;
	uint32_t totalSamples;
	int16_t autoStop;
	int16_t triggered;
} ADVANCED;

typedef struct {
	SIMPLE simple;
	ADVANCED advanced;
} TRIGGER_CHANNEL;

typedef struct {
	int16_t DCcoupled;
	int16_t range;
	int16_t enabled;
	int16_t values[BUFFER_SIZE];
} CHANNEL_SETTINGS;

typedef struct {
	int16_t			handle;
	MODEL_TYPE		model;
	PS2000_RANGE	firstRange;
	PS2000_RANGE	lastRange;
	TRIGGER_CHANNEL trigger;
	int16_t			maxTimebase;
	int16_t			timebases;
	int16_t			noOfChannels;
	CHANNEL_SETTINGS channelSettings[PS2000_MAX_CHANNELS];
	int16_t			hasAdvancedTriggering;
	int16_t			hasFastStreaming;
	int16_t			hasEts;
	int16_t			hasSignalGenerator;
	int16_t			awgBufferSize;
	int16_t			bufferSize;
} UNIT_MODEL;

// Struct to help with retrieving data into 
// application buffers in streaming data capture
typedef struct {
	UNIT_MODEL unit;
	int16_t *appBuffers[DUAL_SCOPE * 2];
	uint32_t bufferSizes[PS2000_MAX_CHANNELS];
} BUFFER_INFO;

UNIT_MODEL unitOpened;

BUFFER_INFO bufferInfo;

int32_t input_ranges[PS2000_MAX_RANGES] = { 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000 };

PDH pdh;
kcubepiezo piezo;

daq::daq(QObject *parent) :
	QObject(parent) {

	QWidget::connect(&timer, &QTimer::timeout,
		this, &daq::getBlockData);

	QWidget::connect(&lockingTimer, &QTimer::timeout,
		this, &daq::lock);
}

bool daq::startStopAcquisition() {
	if (timer.isActive()) {
		timer.stop();
		return false;
	} else {
		daq::setAcquisitionParameters();
		timer.start(20);
		return true;
	}
	return true;
}

bool daq::startStopAcquireLocking() {
	if (lockingTimer.isActive()) {
		daq::disableLocking();
		lockingTimer.stop();
		return false;
	} else {
		lockingTimer.start(100);
		return true;
	}
}

bool daq::startStopLocking() {
	lockParameters.active = !lockParameters.active;
	// set voltage source to potentiometer when locking is not active
	if (lockParameters.active) {
		// set integral error to zero before starting to lock
		lockData.iError = 0;
		// store and immediately restore output voltage
		piezo.storeOutputVoltageIncrement();
		// this is necessary, because it seems, that getting the output voltage takes the external signal into account
		// whereas setting it does not
		piezo.restoreOutputVoltageIncrement();
		piezo.setVoltageSource(PZ_InputSourceFlags::PZ_ExternalSignal);
		piezoVoltage = piezo.getVoltage();
		emit(lockStateChanged(LOCKSTATE::ACTIVE));
	} else {
		daq::disableLocking();
	}
	return lockParameters.active;
}

void daq::disableLocking(LOCKSTATE lockstate) {
	piezo.setVoltageSource(PZ_InputSourceFlags::PZ_Potentiometer);
	currentVoltage = 0;
	// set output voltage of the DAQ
	ps2000_set_sig_gen_built_in(
		unitOpened.handle,				// handle of the oscilloscope
		currentVoltage * 1e6,			// offsetVoltage in microvolt
		0,								// peak to peak voltage in microvolt
		(PS2000_WAVE_TYPE)5,			// type of waveform
		(float)0,						// startFrequency in Hertz
		(float)0,						// stopFrequency in Hertz
		0,								// increment
		0,								// dwellTime, time in seconds between frequency changes in sweep mode
		PS2000_UPDOWN,					// sweepType
		0								// sweeps, number of times to sweep the frequency
	);
	lockParameters.active = FALSE;
	lockParameters.compensating = FALSE;
	emit(compensationStateChanged(false));
	emit(lockStateChanged(lockstate));
}

SCAN_PARAMETERS daq::getScanParameters() {
	return scanParameters;
}

void daq::setSampleRate(int index) {
	acquisitionParameters.timebase = index;
	daq::setAcquisitionParameters();
}

void daq::setNumberSamples(int32_t no_of_samples) {
	acquisitionParameters.no_of_samples = no_of_samples;
	daq::setAcquisitionParameters();
}

void daq::setCoupling(int index, int ch) {
	acquisitionParameters.channelSettings[ch].DCcoupled = index;
	unitOpened.channelSettings[ch].DCcoupled = (bool)index;
	daq::set_defaults();
}

void daq::setRange(int index, int ch) {
	if (index < 9) {
		acquisitionParameters.channelSettings[ch].enabled = TRUE;
		unitOpened.channelSettings[ch].enabled = TRUE;
		acquisitionParameters.channelSettings[ch].range = index + 2;
		unitOpened.channelSettings[ch].range = index + 2;
	} else if (index == 9) {
		// set auto range
	} else {
		acquisitionParameters.channelSettings[ch].enabled = FALSE;
		unitOpened.channelSettings[ch].enabled = FALSE;
	}
	daq::set_defaults();
}

void daq::setScanParameters(int type, int value) {
	switch (type) {
		case 0:
			scanParameters.amplitude = value;
			break;
		case 1:
			scanParameters.offset = value;
			break;
		case 2:
			scanParameters.waveform = value;
			break;
		case 3:
			scanParameters.frequency = value;
			break;
		case 4:
			scanParameters.nrSteps = value;
			break;
	}
}

void daq::setLockParameters(int type, double value) {
	switch (type) {
		case 0:
			lockParameters.proportional = value;
			break;
		case 1:
			lockParameters.integral = value;
			break;
		case 2:
			lockParameters.derivative = value;
			break;
		case 3:
			lockParameters.frequency = value;
			break;
		case 4:
			lockParameters.phase = value;
			break;
	}
}

void daq::toggleOffsetCompensation(bool compensate) {
	lockParameters.compensate = compensate;
}

void daq::setAcquisitionParameters() {

	int16_t maxChannels = (2 < unitOpened.noOfChannels) ? 2 : unitOpened.noOfChannels;

	for (int16_t ch(0); ch < maxChannels; ch++) {
		unitOpened.channelSettings[ch].enabled = acquisitionParameters.channelSettings[ch].enabled;
		unitOpened.channelSettings[ch].DCcoupled = acquisitionParameters.channelSettings[ch].DCcoupled;
		unitOpened.channelSettings[ch].range = acquisitionParameters.channelSettings[ch].range;
	}
	
	// initialize the ADC
	set_defaults();

	/* Trigger disabled */
	ps2000_set_trigger(unitOpened.handle, PS2000_NONE, 0, PS2000_RISING, 0, acquisitionParameters.auto_trigger_ms);

	/*  find the maximum number of samples, the time interval (in time_units),
	*		 the most suitable time units, and the maximum oversample at the current timebase
	*/
	acquisitionParameters.oversample = 1;
	while (!ps2000_get_timebase(
		unitOpened.handle,
		acquisitionParameters.timebase,
		acquisitionParameters.no_of_samples,
		&acquisitionParameters.time_interval,
		&acquisitionParameters.time_units,
		acquisitionParameters.oversample,
		&acquisitionParameters.max_samples)
		) {
		acquisitionParameters.timebase++;
	};

	emit acquisitionParametersChanged(acquisitionParameters);
}

std::array<std::vector<int32_t>, PS2000_MAX_CHANNELS> daq::collectBlockData() {

	int32_t times[BUFFER_SIZE];

	/* Start collecting data,
	*  wait for completion */
	ps2000_run_block(
		unitOpened.handle,
		acquisitionParameters.no_of_samples,
		acquisitionParameters.timebase,
		acquisitionParameters.oversample,
		&acquisitionParameters.time_indisposed_ms
	);

	while (!ps2000_ready(unitOpened.handle)) {
		Sleep(10);
	}

	ps2000_stop(unitOpened.handle);

	/* Should be done now...
	*  get the times (in nanoseconds)
	*  and the values (in ADC counts)
	*/
	ps2000_get_times_and_values(
		unitOpened.handle,
		times,
		unitOpened.channelSettings[PS2000_CHANNEL_A].values,
		unitOpened.channelSettings[PS2000_CHANNEL_B].values,
		NULL,
		NULL,
		&overflow,
		acquisitionParameters.time_units,
		acquisitionParameters.no_of_samples
	);

	// create vector of voltage values
	std::array<std::vector<int32_t>, PS2000_MAX_CHANNELS> values;
	for (int i(0); i < acquisitionParameters.no_of_samples; i++) {
		for (int ch(0); ch < unitOpened.noOfChannels; ch++) {
			if (unitOpened.channelSettings[ch].enabled) {
				values[ch].push_back(adc_to_mv(unitOpened.channelSettings[ch].values[i], unitOpened.channelSettings[ch].range));
			}
		}
	}
	return values;
}

void daq::scanManual() {
	scanData.nrSteps = scanParameters.nrSteps;
	scanData.voltages.resize(scanParameters.nrSteps);
	scanData.intensity.resize(scanParameters.nrSteps);
	scanData.error.resize(scanParameters.nrSteps);

	daq::setAcquisitionParameters();

	// generate voltage values
	for (int j(0); j < scanData.nrSteps; j++) {
		// create voltages, store in array
		scanData.voltages[j] = j*scanParameters.amplitude / (scanParameters.nrSteps - 1) + scanParameters.offset;

		// set voltages as DC value on the signal generator
		ps2000_set_sig_gen_built_in(
			unitOpened.handle,		// handle of the oscilloscope
			scanData.voltages[j],// offsetVoltage in microvolt
			0,						// peak to peak voltage in microvolt
			(PS2000_WAVE_TYPE)0,	// type of waveform
			(float)0,				// startFrequency in Hertz
			(float)0,				// stopFrequency in Hertz
			0,						// increment
			0,						// dwellTime, time in seconds between frequency changes in sweep mode
			PS2000_UPDOWN,			// sweepType
			0						// sweeps, number of times to sweep the frequency
		);

		if (j == 0) {
			Sleep(5000);
		}

		// acquire detector and reference signal, store and process it
		std::array<std::vector<int32_t>, PS2000_MAX_CHANNELS> values = daq::collectBlockData();

		std::vector<double> tau(values[0].begin(), values[0].end());
		std::vector<double> reference(values[1].begin(), values[1].end());

		//double tau_mean = generalmath::mean(tau);
		double tau_max = generalmath::max(tau);
		double tau_min = generalmath::min(tau);

		for (int kk(0); kk < tau.size(); kk++) {
			tau[kk] = (tau[kk] - tau_min) / (tau_max - tau_min);
		}

		scanData.intensity[j] = generalmath::absSum(tau);

		scanData.error[j] = pdh.getError(tau, reference);
	}

	// normalize error
	double error_max = generalmath::max(scanData.error);
	for (int kk(0); kk < scanData.error.size(); kk++) {
		scanData.error[kk] /= error_max;
	}

	// reset signal generator to start values
	daq::set_sig_gen();
	emit scanDone();
}

SCAN_DATA daq::getScanData() {
	return scanData;
}

LOCK_PARAMETERS daq::getLockParameters() {
	return lockParameters;
}

void daq::getBlockData() {
	std::array<std::vector<int32_t>, PS2000_MAX_CHANNELS> values = daq::collectBlockData();

	for (int channel(0); channel < values.size(); channel++) {
		data[channel].resize(values[channel].size());
		for (int jj(0); jj < values[channel].size(); jj++) {
			data[channel][jj] = QPointF(jj, values[channel][jj] / static_cast<double>(1000));
		}
	}

	emit collectedBlockData(data);
}

void daq::lock() {
	std::array<std::vector<int32_t>, PS2000_MAX_CHANNELS> values = daq::collectBlockData();			// [mV]

	std::chrono::time_point<std::chrono::system_clock> now = std::chrono::system_clock::now();

	std::vector<double> tau(values[0].begin(), values[0].end());
	std::vector<double> reference(values[1].begin(), values[1].end());

	//double tau_mean = generalmath::mean(tau);
	double tau_max = generalmath::max(tau);
	double tau_min = generalmath::min(tau);
	double amplitude = tau_max - tau_min;

	if (amplitude != 0) {
		for (int kk(0); kk < tau.size(); kk++) {
			tau[kk] = (tau[kk] - tau_min) / amplitude;
		}
	}

	// adjust for requested phase
	double samplingRate = 200e6 / pow(2, acquisitionParameters.timebase);
	int phaseStep = (int)lockParameters.phase * samplingRate / (360 * lockParameters.frequency);
	if (phaseStep > 0) {
		// simple rotation to the left
		std::rotate(reference.begin(), reference.begin() + phaseStep, reference.end());
	} else if (phaseStep < 0) {
		// simple rotation to the right
		std::rotate(reference.rbegin(), reference.rbegin() + phaseStep, reference.rend());
	}

	double error = pdh.getError(tau, reference);

	// write data to struct for storage
	lockData.amplitude.push_back(amplitude);

	if (lockParameters.active) {
		double dError = 0;
		if (lockData.error.size() > 0) {
			double dt = std::chrono::duration_cast<std::chrono::milliseconds>(now - lockData.time.back()).count() / 1e3;
			lockData.iError += lockParameters.integral * ( lockData.error.back() + error ) * (dt) / 2;
			dError = (error - lockData.error.back()) / dt;
		}
		currentVoltage = currentVoltage + (lockParameters.proportional * error + lockData.iError + lockParameters.derivative * dError) / 100;

		// check if offset compensation is necessary and set piezo voltage
		if (lockParameters.compensate) {
			compensationTimer++;
			if (abs(currentVoltage) > lockParameters.maxOffset) {
				lockParameters.compensating = TRUE;
				emit(compensationStateChanged(true));
			}
			if (abs(currentVoltage) < lockParameters.targetOffset) {
				lockParameters.compensating = FALSE;
				emit(compensationStateChanged(false));
			}
			if (lockParameters.compensating & (compensationTimer > 50)) {
				compensationTimer = 0;
				if (currentVoltage > 0) {
					piezo.incrementVoltage(1);
					piezoVoltage = piezo.getVoltage();
				}
				else {
					piezo.incrementVoltage(-1);
					piezoVoltage = piezo.getVoltage();
				}
			}
		} else {
			lockParameters.compensating = FALSE;
			emit(compensationStateChanged(false));
		}

		// abort locking if
		// - output voltage is over 2 V
		// - maximum of the signal amplitude in the last 50 measurements is below 0.05 V
		if ((abs(currentVoltage) > 2) || (generalmath::floatingMax(lockData.amplitude, 50) / static_cast<double>(1000) < 0.05)) {
			daq::disableLocking(LOCKSTATE::FAILURE);
		}

		// set output voltage of the DAQ
		ps2000_set_sig_gen_built_in(
			unitOpened.handle,				// handle of the oscilloscope
			currentVoltage * 1e6,			// offsetVoltage in microvolt
			0,								// peak to peak voltage in microvolt
			(PS2000_WAVE_TYPE)5,			// type of waveform
			(float)0,						// startFrequency in Hertz
			(float)0,						// stopFrequency in Hertz
			0,								// increment
			0,								// dwellTime, time in seconds between frequency changes in sweep mode
			PS2000_UPDOWN,					// sweepType
			0								// sweeps, number of times to sweep the frequency
		);
	}

	// write data to struct for storage
	lockData.time.push_back(now);
	lockData.error.push_back(error);
	lockData.voltage.push_back(currentVoltage);

	double passed = std::chrono::duration_cast<std::chrono::milliseconds>(now - lockData.time[0]).count() / 1e3;	// store passed time in seconds

	// write data to array for plotting
	lockDataPlot[static_cast<int>(lockViewPlotTypes::VOLTAGE)].append(QPointF(passed, currentVoltage));
	lockDataPlot[static_cast<int>(lockViewPlotTypes::ERRORSIGNAL)].append(QPointF(passed, error / 100));
	lockDataPlot[static_cast<int>(lockViewPlotTypes::AMPLITUDE)].append(QPointF(passed, amplitude / static_cast<double>(1000)));
	lockDataPlot[static_cast<int>(lockViewPlotTypes::PIEZOVOLTAGE)].append(QPointF(passed, piezoVoltage));
	lockDataPlot[static_cast<int>(lockViewPlotTypes::ERRORSIGNALMEAN)].append(QPointF(passed, generalmath::floatingMean(lockData.error, 50) / 100));
	lockDataPlot[static_cast<int>(lockViewPlotTypes::ERRORSIGNALSTD)].append(QPointF(passed, generalmath::floatingStandardDeviation(lockData.error, 50) / 100));

	emit locked(lockDataPlot);
}

bool daq::enablePiezo() {
	piezo.enable();
	return TRUE;
}

bool daq::disablePiezo() {
	piezo.disable();
	return FALSE;
}

void daq::incrementPiezoVoltage() {
	piezo.incrementVoltage(1);
}

void daq::decrementPiezoVoltage() {
	piezo.incrementVoltage(-1);
}

QVector<QPointF> daq::getStreamingBuffer(int ch) {
	QVector<QPointF> data;
	//data.reserve(unitOpened.trigger.advanced.totalSamples);
	data.reserve(50000);
	double voltage;

	for (int j = 0; j < 50000; j++) {
	//for (uint32_t j = 0; j < unitOpened.trigger.advanced.totalSamples; j++) {
		//voltage = adc_to_mv(bufferInfo.appBuffers[ch * 2][j], unitOpened.channelSettings[ch].range) / static_cast<double>(1000) + (qreal)rand() / (qreal)RAND_MAX;
		voltage = adc_to_mv(bufferInfo.appBuffers[ch * 2][j], unitOpened.channelSettings[ch].range) / static_cast<double>(1000);
		data.append(QPointF(j, voltage));
	}

	return data;
}

/****************************************************************************
* startStreaming
*
* Starts the streaming of data
****************************************************************************/
void daq::startStreaming(void) {
	//uint32_t	i;
	//FILE 		*fp;
	int32_t 	ok;
	//int16_t		ch;
	//uint32_t	nPreviousValues = 0;
	//double		startTime = 0.0;
	uint32_t	appBufferSize = (int)(50000);
	//uint32_t	overviewBufferSize = BUFFER_SIZE_STREAMING;
	//uint32_t	sample_count;

	set_defaults();

	// Simple trigger, 500mV, rising
	ok = ps2000_set_trigger(
		unitOpened.handle,														// handle of the oscilloscope
		PS2000_CHANNEL_A,														// source where to look for a trigger
		mv_to_adc(0, unitOpened.channelSettings[PS2000_CHANNEL_A].range),		// trigger threshold
		PS2000_RISING,															// direction, rising or falling
		0,																		// delay
		0																		// the delay in ms
	);

	unitOpened.trigger.advanced.autoStop = 0;
	unitOpened.trigger.advanced.totalSamples = 0;
	unitOpened.trigger.advanced.triggered = 0;

	//Reset global values
	g_nValues = 0;
	g_triggered = 0;
	g_triggeredAt = 0;
	g_startIndex = 0;
	g_prevStartIndex = 0;
	g_appBufferFull = 0;

	bufferInfo.unit = unitOpened;

	// Allocate memory for data arrays

	// Max A buffer at index 0, min buffer at index 1
	bufferInfo.appBuffers[PS2000_CHANNEL_A * 2] = (int16_t *)calloc(appBufferSize, sizeof(int16_t));
	bufferInfo.bufferSizes[PS2000_CHANNEL_A * 2] = appBufferSize;

	if (unitOpened.channelSettings[PS2000_CHANNEL_B].enabled) {
		// Max B buffer at index 2, min buffer at index 3
		bufferInfo.appBuffers[PS2000_CHANNEL_B * 2] = (int16_t *)calloc(appBufferSize, sizeof(int16_t));
		bufferInfo.bufferSizes[PS2000_CHANNEL_B * 2] = appBufferSize;
	}

	/* Collect data at 1us intervals
	* 1000000 points after trigger with 0 aggregation
	* Auto stop after the 1000000 samples
	* Start it collecting,
	* NOTE: The actual sampling interval used by the driver might not be that which is specified below. Use the sampling intervals
	* returned by the ps2000_get_timebase function to work out the most appropriate sampling interval to use. As these are low memory
	* devices, the fastest sampling intervals may result in lost data.
	*/
	ok = ps2000_run_streaming_ns(
		unitOpened.handle,		// handle, handle of the oscilloscope
		2,						// sample_interval, sample interval in time_units
		PS2000_US,				// time_units, units in which sample_interval is measured
		NUM_STREAMING_SAMPLES,	// max_samples, maximum number of samples
		0,						// auto_stop, boolean to indicate if streaming should stop when max_samples is reached
		1,						// noOfSamplesPerAggregate, number of samples the driver will merge
		50000		// size of the overview buffer
	);
	/* From here on, we can get data whenever we want...*/
}

void daq::collectStreamingData() {

	unitOpened.trigger.advanced.autoStop = 0;
	unitOpened.trigger.advanced.totalSamples = 0;
	unitOpened.trigger.advanced.triggered = 0;

	//Reset global values
	g_nValues = 0;
	g_triggered = 0;
	g_triggeredAt = 0;
	g_startIndex = 0;
	g_prevStartIndex = 0;
	g_appBufferFull = 0;

	while (!unitOpened.trigger.advanced.autoStop && !g_appBufferFull) {
		ps2000_get_streaming_last_values(
			unitOpened.handle,				// handle, handle of the oscilloscope
			&daq::ps2000FastStreamingReady2 // pointer to callback function to receive data
		);
	}

	emit collectedData();
}

/****************************************************************************
* stopStreaming
*
* Stops the streaming of data
****************************************************************************/
void daq::stopStreaming() {
	int16_t		ch;
	ps2000_stop(unitOpened.handle);
	
	// Free buffers
	for (ch = 0; ch < unitOpened.noOfChannels; ch++) {
		if (unitOpened.channelSettings[ch].enabled) {
			free(bufferInfo.appBuffers[ch * 2]);
		}
	}
}

/****************************************************************************
* adc_to_mv
*
* If the user selects scaling to millivolts,
* Convert an 12-bit ADC count into millivolts
****************************************************************************/
int32_t daq::adc_to_mv(int32_t raw, int32_t ch) {
	return (scale_to_mv) ? (raw * input_ranges[ch]) / 32767 : raw;
}

/****************************************************************************
* mv_to_adc
*
* Convert a millivolt value into a 12-bit ADC count
*
*  (useful for setting trigger thresholds)
****************************************************************************/
int16_t daq::mv_to_adc(int16_t mv, int16_t ch) {
	return ((mv * 32767) / input_ranges[ch]);
}

/****************************************************************************
* set_defaults - restore default settings
****************************************************************************/
void daq::set_defaults(void) {
	int16_t ch = 0;
	ps2000_set_ets(unitOpened.handle, PS2000_ETS_OFF, 0, 0);

	for (ch = 0; ch < unitOpened.noOfChannels; ch++) {
		ps2000_set_channel(unitOpened.handle,
			ch,
			unitOpened.channelSettings[ch].enabled,
			unitOpened.channelSettings[ch].DCcoupled,
			unitOpened.channelSettings[ch].range
		);
	}
}

/****************************************************************************
*
* Streaming callback
*
* This demonstrates how to copy data to application buffers
*
****************************************************************************/
void  __stdcall daq::ps2000FastStreamingReady2(
	int16_t	**overviewBuffers,
	int16_t		overflow,
	uint32_t	triggeredAt,
	int16_t		triggered,
	int16_t		auto_stop,
	uint32_t	nValues
) {

	int16_t channel = 0;

	unitOpened.trigger.advanced.totalSamples += nValues;
	unitOpened.trigger.advanced.autoStop = auto_stop;

	g_triggered = triggered;
	g_triggeredAt = triggeredAt;

	g_overflow = overflow;

	if (nValues > 0 && g_appBufferFull == 0) {
		for (channel = (int16_t)PS2000_CHANNEL_A; channel < DUAL_SCOPE; channel++) {
			if (bufferInfo.unit.channelSettings[channel].enabled) {
				if (unitOpened.trigger.advanced.totalSamples <= bufferInfo.bufferSizes[channel * 2] && !g_appBufferFull) {
					g_nValues = nValues;
				} else if (g_startIndex < bufferInfo.bufferSizes[channel * 2]) {
					g_nValues = bufferInfo.bufferSizes[channel * 2] - (g_startIndex + 1);			// Only copy data into application buffer up to end
					unitOpened.trigger.advanced.totalSamples = bufferInfo.bufferSizes[channel * 2];	// Total samples limited to application buffer
					g_appBufferFull = 1;
				} else {
					// g_startIndex might be >= buffer length
					g_nValues = 0;
					unitOpened.trigger.advanced.totalSamples = bufferInfo.bufferSizes[channel * 2];
					g_appBufferFull = 1;
				}

				// Copy data...

				// Max buffers
				if (overviewBuffers[channel * 2] && bufferInfo.appBuffers[channel * 2]) {
					memcpy_s(
						(void *)(bufferInfo.appBuffers[channel * 2] + g_startIndex),
						g_nValues * sizeof(int16_t),
						(void *)(overviewBuffers[channel * 2]),
						g_nValues * sizeof(int16_t)
					);
				}

				// Min buffers
				if (overviewBuffers[channel * 2 + 1] && bufferInfo.appBuffers[channel * 2 + 1]) {
					memcpy_s(
						(void *)(bufferInfo.appBuffers[channel * 2 + 1] + g_startIndex),
						g_nValues * sizeof(int16_t),
						(void *)(overviewBuffers[channel * 2 + 1]), g_nValues * sizeof(int16_t)
					);
				}
			}
		}

		g_prevStartIndex = g_startIndex;
		g_startIndex = unitOpened.trigger.advanced.totalSamples;
	}
}

bool daq::connect() {
	if (!unitOpened.handle) {
		unitOpened.handle = ps2000_open_unit();
		
		get_info();

		if (!unitOpened.handle) {
			return false;
		} else {
			daq::setAcquisitionParameters();
			return true;
		}
	} else {
		return true;
	}
}

bool daq::disconnect() {
	if (unitOpened.handle) {
		ps2000_close_unit(unitOpened.handle);
		unitOpened.handle = NULL;
	}
	return false;
}

bool daq::connectPiezo() {
	piezo.connect();
	return TRUE;
}

bool daq::disconnectPiezo() {
	piezo.disconnect();
	return FALSE;
}

void daq::get_info(void) {
	int8_t description[8][25] = {
		"Driver Version   ",
		"USB Version      ",
		"Hardware Version ",
		"Variant Info     ",
		"Serial           ",
		"Cal Date         ",
		"Error Code       ",
		"Kernel Driver    " 
	};
	int16_t	i;
	int8_t	line[80];
	int32_t	variant;

	if (unitOpened.handle) {
		for (i = 0; i < 6; i++) {
			ps2000_get_unit_info(unitOpened.handle, line, sizeof(line), i);

			if (i == 3) {
				variant = atoi((const char*)line);

				// Identify if 2204A or 2205A
				if (strlen((const char*)line) == 5) {
					line[4] = toupper(line[4]);

					// i.e 2204A -> 0xA204
					if (line[1] == '2' && line[4] == 'A') {
						variant += 0x9968;
					}
				}
			}
			printf("%s: %s\n", description[i], line);
		}

		switch (variant) {
			case MODEL_PS2104:
				unitOpened.model = MODEL_PS2104;
				unitOpened.firstRange = PS2000_100MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2104_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 1;
				unitOpened.hasAdvancedTriggering = FALSE;
				unitOpened.hasSignalGenerator = FALSE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = FALSE;
				break;

			case MODEL_PS2105:
				unitOpened.model = MODEL_PS2105;
				unitOpened.firstRange = PS2000_100MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2105_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 1;
				unitOpened.hasAdvancedTriggering = FALSE;
				unitOpened.hasSignalGenerator = FALSE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = FALSE;
				break;

			case MODEL_PS2202:
				unitOpened.model = MODEL_PS2202;
				unitOpened.firstRange = PS2000_100MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 2;
				unitOpened.hasAdvancedTriggering = FALSE;
				unitOpened.hasSignalGenerator = FALSE;
				unitOpened.hasEts = FALSE;
				unitOpened.hasFastStreaming = FALSE;
				break;

			case MODEL_PS2203:
				unitOpened.model = MODEL_PS2203;
				unitOpened.firstRange = PS2000_50MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 2;
				unitOpened.hasAdvancedTriggering = TRUE;
				unitOpened.hasSignalGenerator = TRUE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = TRUE;
				break;

			case MODEL_PS2204:
				unitOpened.model = MODEL_PS2204;
				unitOpened.firstRange = PS2000_50MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 2;
				unitOpened.hasAdvancedTriggering = TRUE;
				unitOpened.hasSignalGenerator = TRUE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = TRUE;
				unitOpened.bufferSize = 8000;
				break;

			case MODEL_PS2204A:
				unitOpened.model = MODEL_PS2204A;
				unitOpened.firstRange = PS2000_50MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = DUAL_SCOPE;
				unitOpened.hasAdvancedTriggering = TRUE;
				unitOpened.hasSignalGenerator = TRUE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = TRUE;
				unitOpened.awgBufferSize = 4096;
				unitOpened.bufferSize = 8000;
				break;

			case MODEL_PS2205:
				unitOpened.model = MODEL_PS2205;
				unitOpened.firstRange = PS2000_50MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = 2;
				unitOpened.hasAdvancedTriggering = TRUE;
				unitOpened.hasSignalGenerator = TRUE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = TRUE;
				unitOpened.bufferSize = 16000;
				break;

			case MODEL_PS2205A:
				unitOpened.model = MODEL_PS2205A;
				unitOpened.firstRange = PS2000_50MV;
				unitOpened.lastRange = PS2000_20V;
				unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				unitOpened.timebases = unitOpened.maxTimebase;
				unitOpened.noOfChannels = DUAL_SCOPE;
				unitOpened.hasAdvancedTriggering = TRUE;
				unitOpened.hasSignalGenerator = TRUE;
				unitOpened.hasEts = TRUE;
				unitOpened.hasFastStreaming = TRUE;
				unitOpened.awgBufferSize = 4096;
				unitOpened.bufferSize = 16000;
				break;

			default:
				printf("Unit not supported");
		}

		unitOpened.channelSettings[PS2000_CHANNEL_A].enabled = 1;
		unitOpened.channelSettings[PS2000_CHANNEL_A].DCcoupled = 1;
		unitOpened.channelSettings[PS2000_CHANNEL_A].range = PS2000_5V;

		if (unitOpened.noOfChannels == DUAL_SCOPE) {
			unitOpened.channelSettings[PS2000_CHANNEL_B].enabled = 1;
		} else {
			unitOpened.channelSettings[PS2000_CHANNEL_B].enabled = 0;
		}

		unitOpened.channelSettings[PS2000_CHANNEL_B].DCcoupled = 1;
		unitOpened.channelSettings[PS2000_CHANNEL_B].range = PS2000_5V;

		set_defaults();

	} else {
		printf("Unit Not Opened\n");

		ps2000_get_unit_info(unitOpened.handle, line, sizeof(line), 5);

		printf("%s: %s\n", description[5], line);
		unitOpened.model = MODEL_NONE;
		unitOpened.firstRange = PS2000_100MV;
		unitOpened.lastRange = PS2000_20V;
		unitOpened.timebases = PS2105_MAX_TIMEBASE;
		unitOpened.noOfChannels = SINGLE_CH_SCOPE;
	}
}

void daq::set_sig_gen() {

	currentVoltage = scanParameters.offset / 1e6;

	ps2000_set_sig_gen_built_in(
		unitOpened.handle,				// handle of the oscilloscope
		scanParameters.offset,			// offsetVoltage in microvolt
		scanParameters.amplitude,		// peak to peak voltage in microvolt
		(PS2000_WAVE_TYPE) scanParameters.waveform,	// type of waveform
		(float) scanParameters.frequency,	// startFrequency in Hertz
		(float) scanParameters.frequency,	// stopFrequency in Hertz
		0,								// increment
		0,								// dwellTime, time in seconds between frequency changes in sweep mode
		PS2000_UPDOWN,					// sweepType
		0								// sweeps, number of times to sweep the frequency
	);

}

void daq::set_trigger_advanced(void) {
	int16_t ok = 0;
	int16_t auto_trigger_ms = 0;

	// to trigger of more than one channel set this parameter to 2 or more
	// each condition can only have on parameter set to PS2000_CONDITION_TRUE or PS2000_CONDITION_FALSE
	// if more than on condition is set then it will trigger off condition one, or condition two etc.
	unitOpened.trigger.advanced.nProperties = 1;
	// set the trigger channel to channel A by using PS2000_CONDITION_TRUE
	unitOpened.trigger.advanced.conditions = (PS2000_TRIGGER_CONDITIONS*)malloc(sizeof(PS2000_TRIGGER_CONDITIONS) * unitOpened.trigger.advanced.nProperties);
	unitOpened.trigger.advanced.conditions->channelA = PS2000_CONDITION_TRUE;
	unitOpened.trigger.advanced.conditions->channelB = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->channelC = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->channelD = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->external = PS2000_CONDITION_DONT_CARE;
	unitOpened.trigger.advanced.conditions->pulseWidthQualifier = PS2000_CONDITION_DONT_CARE;

	// set channel A to rising
	// the remainder will be ignored as only a condition is set for channel A
	unitOpened.trigger.advanced.directions.channelA = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.channelB = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.channelC = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.channelD = PS2000_ADV_RISING;
	unitOpened.trigger.advanced.directions.ext = PS2000_ADV_RISING;


	unitOpened.trigger.advanced.channelProperties = (PS2000_TRIGGER_CHANNEL_PROPERTIES*)malloc(sizeof(PS2000_TRIGGER_CHANNEL_PROPERTIES) * unitOpened.trigger.advanced.nProperties);
	// there is one property for each condition
	// set channel A
	// trigger level 1500 adc counts the trigger point will vary depending on the voltage range
	// hysterisis 4096 adc counts  
	unitOpened.trigger.advanced.channelProperties->channel = (int16_t)PS2000_CHANNEL_A;
	unitOpened.trigger.advanced.channelProperties->thresholdMajor = 1500;
	// not used in level triggering, should be set when in window mode
	unitOpened.trigger.advanced.channelProperties->thresholdMinor = 0;
	// used in level triggering, not used when in window mode
	unitOpened.trigger.advanced.channelProperties->hysteresis = (int16_t)4096;
	unitOpened.trigger.advanced.channelProperties->thresholdMode = PS2000_LEVEL;

	ok = ps2000SetAdvTriggerChannelConditions(unitOpened.handle, unitOpened.trigger.advanced.conditions, unitOpened.trigger.advanced.nProperties);

	ok = ps2000SetAdvTriggerChannelDirections(unitOpened.handle,
		unitOpened.trigger.advanced.directions.channelA,
		unitOpened.trigger.advanced.directions.channelB,
		unitOpened.trigger.advanced.directions.channelC,
		unitOpened.trigger.advanced.directions.channelD,
		unitOpened.trigger.advanced.directions.ext);

	ok = ps2000SetAdvTriggerChannelProperties(unitOpened.handle,
		unitOpened.trigger.advanced.channelProperties,
		unitOpened.trigger.advanced.nProperties,
		auto_trigger_ms);

	// remove comments to try triggering with a pulse width qualifier
	// add a condition for the pulse width eg. in addition to the channel A or as a replacement
	//unitOpened.trigger.advanced.pwq.conditions = malloc (sizeof (PS2000_PWQ_CONDITIONS));
	//unitOpened.trigger.advanced.pwq.conditions->channelA = PS2000_CONDITION_TRUE;
	//unitOpened.trigger.advanced.pwq.conditions->channelB = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.conditions->channelC = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.conditions->channelD = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.conditions->external = PS2000_CONDITION_DONT_CARE;
	//unitOpened.trigger.advanced.pwq.nConditions = 1;

	//unitOpened.trigger.advanced.pwq.direction = PS2000_RISING;
	//unitOpened.trigger.advanced.pwq.type = PS2000_PW_TYPE_LESS_THAN;
	//// used when type	PS2000_PW_TYPE_IN_RANGE,	PS2000_PW_TYPE_OUT_OF_RANGE
	//unitOpened.trigger.advanced.pwq.lower = 0;
	//unitOpened.trigger.advanced.pwq.upper = 10000;
	//ps2000SetPulseWidthQualifier (unitOpened.handle,
	//															unitOpened.trigger.advanced.pwq.conditions,
	//															unitOpened.trigger.advanced.pwq.nConditions, 
	//															unitOpened.trigger.advanced.pwq.direction,
	//															unitOpened.trigger.advanced.pwq.lower,
	//															unitOpened.trigger.advanced.pwq.upper,
	//															unitOpened.trigger.advanced.pwq.type);

	ok = ps2000SetAdvTriggerDelay(unitOpened.handle, 0, -10);
}