#ifndef DAQ_H
#define DAQ_H

#include <QMainWindow>
#include <QtCore/QObject>
#include <QtWidgets>
#include <vector>

#include "ps2000.h"
#include "DFT.h"

#define BUFFER_SIZE 	8000
#define BUFFER_SIZE_STREAMING 50000		// Overview buffer size
#define NUM_STREAMING_SAMPLES 100000	// Number of streaming samples to collect
#define MAX_CHANNELS 4
#define SINGLE_CH_SCOPE 1				// Single channel scope
#define DUAL_SCOPE 2					// Dual channel scope

typedef struct {
	int16_t 	auto_trigger_ms = 0;
	int32_t 	time_interval;
	int16_t 	time_units;
	int16_t 	oversample;
	int32_t 	no_of_samples = BUFFER_SIZE;
	int32_t 	max_samples;
	int32_t 	time_indisposed_ms;
} ACQUISITION_PARAMETERS;

typedef struct {
	int32_t amplitude = 2e6;		// [�V] peak to peak voltage
	int32_t offset = 0;				// 
	int16_t waveform = 3;			// type of waveform
	int32_t frequency = 100;		// frequency of the scan
	int32_t	nrSteps = 100;			// number of steps for the manual scan
} SCAN_PARAMETERS;

typedef struct {
	int32_t nrSteps = 0;
	std::vector<int32_t> voltages;	// [�V] output voltage (<int32_t> is sufficient for this)
	std::vector<int32_t> intensity;	// [�V] measured intensity (<int32_t> is fine)
	AMPLITUDES amplitudes;			// amplitudes of the first and second harmonic
	std::vector<double> quotients;	// quotients of the amplitudes of the first and second harmonic
} SCAN_RESULTS;

class daq : public QObject {
	Q_OBJECT

	public:
		explicit daq(QObject *parent = 0);
		bool startStopAcquisition();
		void acquire();
		QVector<QPointF> getData();
		QVector<QPointF> getBuffer(int ch);
		bool connect();
		bool disconnect();
		void startStreaming();
		void collectStreamingData();
		void stopStreaming();
		void set_sig_gen();
		void set_trigger_advanced();
		SCAN_PARAMETERS getScanParameters();
		void setScanParameters(int type, int value);
		void scanManual();
		SCAN_RESULTS getScanResults();

	private slots:

	signals:
		void scanDone();
		void collectedData();

	private:
		static void __stdcall ps2000FastStreamingReady2(
			int16_t **overviewBuffers,
			int16_t   overflow,
			uint32_t  triggeredAt,
			int16_t   triggered,
			int16_t   auto_stop,
			uint32_t  nValues
		);
		ACQUISITION_PARAMETERS setAcquisitionParameters();
		std::array<std::vector<int32_t>, PS2000_MAX_CHANNELS> collectData(ACQUISITION_PARAMETERS acquisitionParameters);
		int32_t adc_to_mv(
			int32_t raw,
			int32_t ch
		);
		int16_t mv_to_adc(
			int16_t mv,
			int16_t ch
		);
		void set_defaults(void);
		void get_info(void);
		QTimer timer;
		int colCount = 1024;
		QVector<QPointF> points;
		SCAN_PARAMETERS scanParameters;
		SCAN_RESULTS scanResults;
		double mean(std::vector<int>);
		double mean(std::vector<double>);
		std::complex<double> mean(std::vector<std::complex<double>>);
};

#endif // DAQ_H