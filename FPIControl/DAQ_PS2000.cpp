#include "daq_PS2000.h"
#include <QtWidgets>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>

daq_PS2000::daq_PS2000(QObject *parent) :
	daq(parent,
		{ 10, 20, 50, 100, 200, 500, 1000, 2000, 5000, 10000, 20000, 50000 },
		{ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10 },
		200e6 // this is only valid for the 2205, needs to be changed, when an other model is used
	) {
	m_acquisitionParameters.timebaseIndex = m_defaultTimebaseIndex;
	m_acquisitionParameters.timebase = m_availableTimebases[m_defaultTimebaseIndex];
	// calculate sampling rates
	m_availableSamplingRates.resize(m_availableTimebases.size());
	std::transform(m_availableTimebases.begin(), m_availableTimebases.end(), m_availableSamplingRates.begin(),
		[this](int timebase) {
			return this->m_maxSamplingRate / pow(2, timebase);
		}
	);
}

daq_PS2000::~daq_PS2000() {
	disconnect_daq();
}

void daq_PS2000::setAcquisitionParameters() {

	int16_t maxChannels = (2 < m_unitOpened.noOfChannels) ? 2 : m_unitOpened.noOfChannels;

	for (gsl::index ch{ 0 }; ch < maxChannels; ch++) {
		m_unitOpened.channelSettings[ch].enabled = m_acquisitionParameters.channelSettings[ch].enabled;
		m_unitOpened.channelSettings[ch].coupling = m_acquisitionParameters.channelSettings[ch].coupling;
		m_unitOpened.channelSettings[ch].range = m_acquisitionParameters.channelSettings[ch].range;
	}
	
	// initialize the ADC
	set_defaults();

	/* Trigger disabled */
	ps2000_set_trigger(m_unitOpened.handle, PS2000_NONE, 0, PS2000_RISING, 0, m_acquisitionParameters.auto_trigger_ms);

	/*  find the maximum number of samples, the time interval (in time_units),
	*		 the most suitable time units, and the maximum oversample at the current timebase
	*/
	m_acquisitionParameters.oversample = 1;
	while (!ps2000_get_timebase(
		m_unitOpened.handle,
		m_acquisitionParameters.timebase,
		m_acquisitionParameters.no_of_samples,
		&m_acquisitionParameters.time_interval,
		&m_acquisitionParameters.time_units,
		m_acquisitionParameters.oversample,
		&m_acquisitionParameters.max_samples)
		) {
		m_acquisitionParameters.timebase++;
	};

	emit acquisitionParametersChanged(m_acquisitionParameters);
}

std::array<std::vector<int32_t>, PS2000_MAX_CHANNELS> daq_PS2000::collectBlockData() {

	int32_t times[DAQ_BUFFER_SIZE];

	/* Start collecting data,
	*  wait for completion */
	ps2000_run_block(
		m_unitOpened.handle,
		m_acquisitionParameters.no_of_samples,
		m_acquisitionParameters.timebase,
		m_acquisitionParameters.oversample,
		&m_acquisitionParameters.time_indisposed_ms
	);

	while (!ps2000_ready(m_unitOpened.handle)) {
		Sleep(10);
	}

	ps2000_stop(m_unitOpened.handle);

	/* Should be done now...
	*  get the times (in nanoseconds)
	*  and the values (in ADC counts)
	*/

	ps2000_get_times_and_values(
		m_unitOpened.handle,
		times,
		m_unitOpened.channelSettings[PS2000_CHANNEL_A].values,
		m_unitOpened.channelSettings[PS2000_CHANNEL_B].values,
		NULL,
		NULL,
		&m_overflow,
		m_acquisitionParameters.time_units,
		m_acquisitionParameters.no_of_samples
	);


	// create vector of voltage values
	std::array<std::vector<int32_t>, PS2000_MAX_CHANNELS> values;
	for (gsl::index i{ 0 }; i < m_acquisitionParameters.no_of_samples; i++) {
		for (gsl::index ch{ 0 }; ch < m_unitOpened.noOfChannels; ch++) {
			if (m_unitOpened.channelSettings[ch].enabled) {
				values[ch].push_back(adc_to_mv(m_unitOpened.channelSettings[ch].values[i], m_unitOpened.channelSettings[ch].range));
			}
		}
	}
	return values;
}

void daq_PS2000::setOutputVoltage(double voltage) {
	ps2000_set_sig_gen_built_in(
		m_unitOpened.handle,			// handle of the oscilloscope
		voltage * 1e6,					// offsetVoltage in microvolt
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

/****************************************************************************
* set_defaults - restore default settings
****************************************************************************/
void daq_PS2000::set_defaults(void) {
	ps2000_set_ets(m_unitOpened.handle, PS2000_ETS_OFF, 0, 0);

	for (gsl::index ch{ 0 }; ch < m_unitOpened.noOfChannels; ch++) {
		ps2000_set_channel(
			m_unitOpened.handle,
			ch,
			(int16_t)m_unitOpened.channelSettings[ch].enabled,
			(int16_t)m_unitOpened.channelSettings[ch].coupling,
			m_unitOpened.channelSettings[ch].range
		);
	}
}

void daq_PS2000::connect_daq() {
	if (!m_isConnected) {
		m_unitOpened.handle = ps2000_open_unit();
		get_info();

		if (!m_unitOpened.handle) {
			m_isConnected = false;
		} else {
			setAcquisitionParameters();
			m_isConnected = true;
		}
	}
	emit(connected(m_isConnected));
}

void daq_PS2000::disconnect_daq() {
	if (m_isConnected) {
		if (timer->isActive()) {
			timer->stop();
			m_acquisitionRunning = false;
		}
		ps2000_close_unit(m_unitOpened.handle);
		m_unitOpened.handle = NULL;
		m_isConnected = false;
	}
	emit(connected(m_isConnected));
}

void daq_PS2000::get_info(void) {
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

	int8_t	line[80];
	int32_t	variant;

	if (m_unitOpened.handle) {
		for (gsl::index i{ 0 }; i < 6; i++) {
			ps2000_get_unit_info(m_unitOpened.handle, line, sizeof(line), i);

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
			case (int)PS2000_TYPE::MODEL_PS2104:
				m_unitOpened.model = (int)PS2000_TYPE::MODEL_PS2104;
				m_unitOpened.firstRange = PS2000_100MV;
				m_unitOpened.lastRange = PS2000_20V;
				m_unitOpened.maxTimebase = PS2104_MAX_TIMEBASE;
				m_unitOpened.timebases = m_unitOpened.maxTimebase;
				m_unitOpened.noOfChannels = 1;
				m_unitOpened.hasAdvancedTriggering = false;
				m_unitOpened.hasSignalGenerator = false;
				m_unitOpened.hasEts = true;
				m_unitOpened.hasFastStreaming = false;
				break;

			case (int)PS2000_TYPE::MODEL_PS2105:
				m_unitOpened.model = (int)PS2000_TYPE::MODEL_PS2105;
				m_unitOpened.firstRange = PS2000_100MV;
				m_unitOpened.lastRange = PS2000_20V;
				m_unitOpened.maxTimebase = PS2105_MAX_TIMEBASE;
				m_unitOpened.timebases = m_unitOpened.maxTimebase;
				m_unitOpened.noOfChannels = 1;
				m_unitOpened.hasAdvancedTriggering = false;
				m_unitOpened.hasSignalGenerator = false;
				m_unitOpened.hasEts = true;
				m_unitOpened.hasFastStreaming = false;
				break;

			case (int)PS2000_TYPE::MODEL_PS2202:
				m_unitOpened.model = (int)PS2000_TYPE::MODEL_PS2202;
				m_unitOpened.firstRange = PS2000_100MV;
				m_unitOpened.lastRange = PS2000_20V;
				m_unitOpened.maxTimebase = PS2200_MAX_TIMEBASE;
				m_unitOpened.timebases = m_unitOpened.maxTimebase;
				m_unitOpened.noOfChannels = 2;
				m_unitOpened.hasAdvancedTriggering = false;
				m_unitOpened.hasSignalGenerator = false;
				m_unitOpened.hasEts = false;
				m_unitOpened.hasFastStreaming = false;
				break;

			case (int)PS2000_TYPE::MODEL_PS2203:
				m_unitOpened.model = (int)PS2000_TYPE::MODEL_PS2203;
				m_unitOpened.firstRange = PS2000_50MV;
				m_unitOpened.lastRange = PS2000_20V;
				m_unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				m_unitOpened.timebases = m_unitOpened.maxTimebase;
				m_unitOpened.noOfChannels = 2;
				m_unitOpened.hasAdvancedTriggering = true;
				m_unitOpened.hasSignalGenerator = true;
				m_unitOpened.hasEts = true;
				m_unitOpened.hasFastStreaming = true;
				break;

			case (int)PS2000_TYPE::MODEL_PS2204:
				m_unitOpened.model = (int)PS2000_TYPE::MODEL_PS2204;
				m_unitOpened.firstRange = PS2000_50MV;
				m_unitOpened.lastRange = PS2000_20V;
				m_unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				m_unitOpened.timebases = m_unitOpened.maxTimebase;
				m_unitOpened.noOfChannels = 2;
				m_unitOpened.hasAdvancedTriggering = true;
				m_unitOpened.hasSignalGenerator = true;
				m_unitOpened.hasEts = true;
				m_unitOpened.hasFastStreaming = true;
				m_unitOpened.bufferSize = 8000;
				break;

			case (int)PS2000_TYPE::MODEL_PS2204A:
				m_unitOpened.model = (int)PS2000_TYPE::MODEL_PS2204A;
				m_unitOpened.firstRange = PS2000_50MV;
				m_unitOpened.lastRange = PS2000_20V;
				m_unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				m_unitOpened.timebases = m_unitOpened.maxTimebase;
				m_unitOpened.noOfChannels = DUAL_SCOPE;
				m_unitOpened.hasAdvancedTriggering = true;
				m_unitOpened.hasSignalGenerator = true;
				m_unitOpened.hasEts = true;
				m_unitOpened.hasFastStreaming = true;
				m_unitOpened.awgBufferSize = 4096;
				m_unitOpened.bufferSize = 8000;
				break;

			case (int)PS2000_TYPE::MODEL_PS2205:
				m_unitOpened.model = (int)PS2000_TYPE::MODEL_PS2205;
				m_unitOpened.firstRange = PS2000_50MV;
				m_unitOpened.lastRange = PS2000_20V;
				m_unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				m_unitOpened.timebases = m_unitOpened.maxTimebase;
				m_unitOpened.noOfChannels = 2;
				m_unitOpened.hasAdvancedTriggering = true;
				m_unitOpened.hasSignalGenerator = true;
				m_unitOpened.hasEts = true;
				m_unitOpened.hasFastStreaming = true;
				m_unitOpened.bufferSize = 16000;
				break;

			case (int)PS2000_TYPE::MODEL_PS2205A:
				m_unitOpened.model = (int)PS2000_TYPE::MODEL_PS2205A;
				m_unitOpened.firstRange = PS2000_50MV;
				m_unitOpened.lastRange = PS2000_20V;
				m_unitOpened.maxTimebase = PS2000_MAX_TIMEBASE;
				m_unitOpened.timebases = m_unitOpened.maxTimebase;
				m_unitOpened.noOfChannels = DUAL_SCOPE;
				m_unitOpened.hasAdvancedTriggering = true;
				m_unitOpened.hasSignalGenerator = true;
				m_unitOpened.hasEts = true;
				m_unitOpened.hasFastStreaming = true;
				m_unitOpened.awgBufferSize = 4096;
				m_unitOpened.bufferSize = 16000;
				break;

			default:
				printf("Unit not supported");
		}

		m_unitOpened.channelSettings[PS2000_CHANNEL_A].enabled = true;
		m_unitOpened.channelSettings[PS2000_CHANNEL_A].coupling = PS_DC;
		m_unitOpened.channelSettings[PS2000_CHANNEL_A].range = PS2000_5V;

		if (m_unitOpened.noOfChannels == DUAL_SCOPE) {
			m_unitOpened.channelSettings[PS2000_CHANNEL_B].enabled = true;
		} else {
			m_unitOpened.channelSettings[PS2000_CHANNEL_B].enabled = false;
		}

		m_unitOpened.channelSettings[PS2000_CHANNEL_B].coupling = PS_DC;
		m_unitOpened.channelSettings[PS2000_CHANNEL_B].range = PS2000_5V;

		set_defaults();

	} else {
		printf("Unit Not Opened\n");

		ps2000_get_unit_info(m_unitOpened.handle, line, sizeof(line), 5);

		printf("%s: %s\n", description[5], line);
		m_unitOpened.model = (int)PS2000_TYPE::MODEL_NONE;
		m_unitOpened.firstRange = PS2000_100MV;
		m_unitOpened.lastRange = PS2000_20V;
		m_unitOpened.timebases = PS2105_MAX_TIMEBASE;
		m_unitOpened.noOfChannels = SINGLE_CH_SCOPE;
	}
}