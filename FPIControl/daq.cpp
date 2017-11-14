#include "daq.h"
#include "kcubepiezo.h"
#include "PDH.h"
#include <QtWidgets>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>

/* Definitions of PS2000 driver routines */
#include "ps2000aApi.h"

int16_t		values_a[BUFFER_SIZE]; // block mode buffer, Channel A
int16_t		values_b[BUFFER_SIZE]; // block mode buffer, Channel B

int16_t		overflow;
int32_t		scale_to_mv = 1;

int16_t		channel_mv[PS2000A_MAX_CHANNELS];

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
	MODEL_PS2205A = 0xA205,
	MODEL_PS2405A = 2405
} MODEL_TYPE;

typedef struct {
	PS2000A_THRESHOLD_DIRECTION	channelA;
	PS2000A_THRESHOLD_DIRECTION	channelB;
	PS2000A_THRESHOLD_DIRECTION	channelC;
	PS2000A_THRESHOLD_DIRECTION	channelD;
	PS2000A_THRESHOLD_DIRECTION	ext;
} DIRECTIONS;

typedef struct {
	PS2000A_PWQ_CONDITIONS			*	conditions;
	int16_t							nConditions;
	PS2000A_THRESHOLD_DIRECTION		direction;
	uint32_t						lower;
	uint32_t						upper;
	PS2000A_PULSE_WIDTH_TYPE			type;
} PULSE_WIDTH_QUALIFIER;

typedef struct {
	PS2000A_CHANNEL channel;
	float threshold;
	int16_t direction;
	float delay;
} SIMPLE;

typedef struct {
	int16_t hysteresis;
	DIRECTIONS directions;
	int16_t nProperties;
	PS2000A_TRIGGER_CONDITIONS * conditions;
	PS2000A_TRIGGER_CHANNEL_PROPERTIES * channelProperties;
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
	PS2000A_COUPLING coupling;
	PS2000A_RANGE range;
	int16_t enabled;
	int16_t values[BUFFER_SIZE];
} CHANNEL_SETTINGS;

typedef struct {
	int16_t			handle;
	MODEL_TYPE		model;
	PS2000A_RANGE	firstRange;
	PS2000A_RANGE	lastRange;
	TRIGGER_CHANNEL trigger;
	int16_t			maxTimebase;
	int16_t			timebases;
	int16_t			noOfChannels;
	CHANNEL_SETTINGS channelSettings[PS2000A_MAX_CHANNELS];
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
	uint32_t bufferSizes[PS2000A_MAX_CHANNELS];
} BUFFER_INFO;

UNIT_MODEL unitOpened;

BUFFER_INFO bufferInfo;

int32_t input_ranges[PS2000A_MAX_RANGES] = { 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000 };

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
	ps2000aSetSigGenBuiltIn (
		unitOpened.handle,				// handle of the oscilloscope
		currentVoltage * 1e6,			// offsetVoltage in microvolt
		0,								// peak to peak voltage in microvolt
		PS2000A_DC_VOLTAGE,				// type of waveform
		(float)0,						// startFrequency in Hertz
		(float)0,						// stopFrequency in Hertz
		0,								// increment
		0,								// dwellTime, time in seconds between frequency changes in sweep mode
		PS2000A_UPDOWN,					// sweepType
		PS2000A_ES_OFF,
		0,
		0,								// sweeps, number of times to sweep the frequency
		PS2000A_SIGGEN_RISING,
		PS2000A_SIGGEN_NONE,
		0
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
	acquisitionParameters.timebase = sampleRates[index];
	daq::setAcquisitionParameters();
}

void daq::setNumberSamples(int32_t no_of_samples) {
	acquisitionParameters.no_of_samples = no_of_samples;
	daq::setAcquisitionParameters();
}

void daq::setCoupling(int index, int ch) {
	acquisitionParameters.channelSettings[ch].coupling = PS2000A_COUPLING(index);
	unitOpened.channelSettings[ch].coupling = PS2000A_COUPLING(index);
	daq::set_defaults();
}

void daq::setRange(int index, int ch) {
	if (index < 9) {
		acquisitionParameters.channelSettings[ch].enabled = TRUE;
		unitOpened.channelSettings[ch].enabled = TRUE;
		acquisitionParameters.channelSettings[ch].range = PS2000A_RANGE(index);
		unitOpened.channelSettings[ch].range = PS2000A_RANGE(index);
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

void daq::setScanFrequency(float frequency) {
	scanParameters.frequency = frequency;
};

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
		unitOpened.channelSettings[ch].coupling = acquisitionParameters.channelSettings[ch].coupling;
		unitOpened.channelSettings[ch].range = acquisitionParameters.channelSettings[ch].range;
	}
	
	// initialize the ADC
	set_defaults();

	/* Trigger disabled */
	ps2000aSetSimpleTrigger(
		unitOpened.handle,		// handle
		NULL,					// enable
		PS2000A_CHANNEL_A,		// source
		NULL,					// threshold
		PS2000A_NONE,			// direction
		NULL,					// delay
		NULL
	);

	/*  find the maximum number of samples, the time interval (in time_units),
	*		 the most suitable time units, and the maximum oversample at the current timebase
	*/
	acquisitionParameters.oversample = 1;

	while (ps2000aGetTimebase (
		unitOpened.handle,
		acquisitionParameters.timebase,
		acquisitionParameters.no_of_samples,
		&acquisitionParameters.time_interval,
		acquisitionParameters.oversample,
		&acquisitionParameters.max_samples,
		0)
		) {
		acquisitionParameters.timebase++;
	};

	emit acquisitionParametersChanged(acquisitionParameters);
}

std::array<std::vector<int32_t>, PS2000A_MAX_CHANNELS> daq::collectBlockData() {

	/* Start collecting data,
	*  wait for completion */
	ps2000aRunBlock(
		unitOpened.handle,						// handle
		0,										// noOfPreTriggerSamples
		acquisitionParameters.no_of_samples,	// noOfPostTriggerSamples
		acquisitionParameters.timebase,			// timebase
		acquisitionParameters.oversample,		// oversample
		&acquisitionParameters.time_indisposed_ms,	//timeIndisposedMs
		0,										// segmentIndex
		NULL,									// lpReady
		NULL									// * pParameter
	);

	int16_t ready = 0;
	ps2000aIsReady(unitOpened.handle, &ready);
	while (!ready) {
		Sleep(10);
		ps2000aIsReady(unitOpened.handle, &ready);
	}

	for (int16_t ch(0); ch < 4; ch++) {
		ps2000aSetDataBuffers(
			unitOpened.handle,
			(int16_t) ch,
			buffers[ch * 2],
			buffers[ch * 2 + 1],
			BUFFER_SIZE,
			0,
			PS2000A_RATIO_MODE_NONE
		);
	}

	ps2000aGetValues(
		unitOpened.handle,
		0,
		&acquisitionParameters.no_of_samples,
		NULL,
		PS2000A_RATIO_MODE_NONE,
		0,
		NULL
	);

	ps2000aStop(unitOpened.handle);

	// create vector of voltage values
	std::array<std::vector<int32_t>, PS2000A_MAX_CHANNELS> values;
	for (int32_t i(0); i < static_cast<int32_t>(acquisitionParameters.no_of_samples); i++) {
		for (int ch(0); ch < unitOpened.noOfChannels; ch++) {
			if (unitOpened.channelSettings[ch].enabled) {
				values[ch].push_back(adc_to_mv(buffers[ch * 2][i], unitOpened.channelSettings[ch].range));
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
		ps2000aSetSigGenBuiltIn(
			unitOpened.handle,		// handle of the oscilloscope
			scanData.voltages[j],	// offsetVoltage in microvolt
			0,						// peak to peak voltage in microvolt
			PS2000A_DC_VOLTAGE,		// type of waveform
			(float)0,				// startFrequency in Hertz
			(float)0,				// stopFrequency in Hertz
			0,						// increment
			0,						// dwellTime, time in seconds between frequency changes in sweep mode
			PS2000A_UPDOWN,			// sweepType
			PS2000A_ES_OFF,
			0,
			0,						// sweeps, number of times to sweep the frequency
			PS2000A_SIGGEN_RISING,
			PS2000A_SIGGEN_NONE,
			0
		);

		if (j == 0) {
			Sleep(5000);
		}

		// acquire detector and reference signal, store and process it
		std::array<std::vector<int32_t>, PS2000A_MAX_CHANNELS> values = daq::collectBlockData();

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
	std::array<std::vector<int32_t>, PS2000A_MAX_CHANNELS> values = daq::collectBlockData();

	for (int channel(0); channel < values.size(); channel++) {
		data[channel].resize(values[channel].size());
		for (int jj(0); jj < values[channel].size(); jj++) {
			data[channel][jj] = QPointF(jj, values[channel][jj] / static_cast<double>(1000));
		}
	}

	emit collectedBlockData(data);
}

void daq::lock() {
	std::array<std::vector<int32_t>, PS2000A_MAX_CHANNELS> values = daq::collectBlockData();			// [mV]

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
		ps2000aSetSigGenBuiltIn(
			unitOpened.handle,		// handle of the oscilloscope
			currentVoltage * 1e6,	// offsetVoltage in microvolt
			0,						// peak to peak voltage in microvolt
			PS2000A_DC_VOLTAGE,		// type of waveform
			(float)0,				// startFrequency in Hertz
			(float)0,				// stopFrequency in Hertz
			0,						// increment
			0,						// dwellTime, time in seconds between frequency changes in sweep mode
			PS2000A_UPDOWN,			// sweepType
			PS2000A_ES_OFF,
			0,
			0,						// sweeps, number of times to sweep the frequency
			PS2000A_SIGGEN_RISING,
			PS2000A_SIGGEN_NONE,
			0
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

	ps2000aSetEts(
		unitOpened.handle,
		PS2000A_ETS_OFF,
		0,
		0,
		NULL
	);

	for (ch = 0; ch < unitOpened.noOfChannels; ch++) {
		ps2000aSetChannel(
			unitOpened.handle,
			PS2000A_CHANNEL(ch),
			unitOpened.channelSettings[ch].enabled,
			unitOpened.channelSettings[ch].coupling,
			unitOpened.channelSettings[ch].range,
			0											// analog offset
		);
	}
}

bool daq::connect() {
	if (!unitOpened.handle) {
		ps2000aOpenUnit(&unitOpened.handle, NULL);
		
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
		ps2000aCloseUnit(unitOpened.handle);
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
	
	char line[80];
	int32_t	variant;
	int16_t requiredSize = 0;

	if (unitOpened.handle) {
		ps2000aGetUnitInfo(
			unitOpened.handle,
			(int8_t *)line,
			sizeof(line),
			&requiredSize,
			PICO_VARIANT_INFO
		);

		variant = atoi((const char*)line);

		// Identify if 2204A or 2205A
		if (strlen((const char*)line) == 5) {
			line[4] = toupper(line[4]);

			// i.e 2204A -> 0xA204
			if (line[1] == '2' && line[4] == 'A') {
				variant += 0x9968;
			}
		}


		switch (variant) {
			case MODEL_PS2405A:
				unitOpened.model = MODEL_PS2405A;
				unitOpened.firstRange = PS2000A_50MV;
				unitOpened.lastRange = PS2000A_20V;
				unitOpened.noOfChannels = QUAD_SCOPE;
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

		unitOpened.channelSettings[PS2000A_CHANNEL_A].enabled = 1;
		unitOpened.channelSettings[PS2000A_CHANNEL_A].coupling = PS2000A_DC;
		unitOpened.channelSettings[PS2000A_CHANNEL_A].range = PS2000A_5V;

		if (unitOpened.noOfChannels == DUAL_SCOPE) {
			unitOpened.channelSettings[PS2000A_CHANNEL_B].enabled = 1;
		} else {
			unitOpened.channelSettings[PS2000A_CHANNEL_B].enabled = 0;
		}

		unitOpened.channelSettings[PS2000A_CHANNEL_B].coupling = PS2000A_DC;
		unitOpened.channelSettings[PS2000A_CHANNEL_B].range = PS2000A_5V;

		set_defaults();

	} else {
		//printf("Unit Not Opened\n");

		//ps2000_get_unit_info(unitOpened.handle, line, sizeof(line), 5);

		//printf("%s: %s\n", description[5], line);
		//unitOpened.model = MODEL_NONE;
		//unitOpened.firstRange = PS2000A_100MV;
		//unitOpened.lastRange = PS2000A_20V;
		//unitOpened.timebases = PS2105A_MAX_TIMEBASE;
		//unitOpened.noOfChannels = SINGLE_CH_SCOPE;
	}
}

void daq::set_sig_gen() {

	currentVoltage = scanParameters.offset / 1e6;

	ps2000aSetSigGenBuiltIn(
		unitOpened.handle,				// handle of the oscilloscope
		scanParameters.offset,			// offsetVoltage in microvolt
		scanParameters.amplitude,		// peak to peak voltage in microvolt
		(PS2000A_WAVE_TYPE)scanParameters.waveform,	// type of waveform
		(float)scanParameters.frequency,// startFrequency in Hertz
		(float)scanParameters.frequency,// stopFrequency in Hertz
		0,								// increment
		0,								// dwellTime, time in seconds between frequency changes in sweep mode
		PS2000A_UPDOWN,					// sweepType
		PS2000A_ES_OFF,
		0,
		0,								// sweeps, number of times to sweep the frequency
		PS2000A_SIGGEN_RISING,
		PS2000A_SIGGEN_NONE,
		0
	);

}