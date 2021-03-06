#ifndef DAQ_PS2000_H
#define DAQ_PS2000_H

#include <QMainWindow>
#include <QtCore/QObject>
#include <QtWidgets>
#include <vector>
#include <array>
#include <chrono>
#include <ctime>

#include <gsl/gsl>
#include "ps2000.h"
#include "daq.h"
#include "..\circularBuffer.h"
#include "..\generalmath.h"

#define DAQ_BUFFER_SIZE 	8000
#define SINGLE_CH_SCOPE 1				// Single channel scope
#define DUAL_SCOPE 2					// Dual channel scope

typedef enum class PS2000Type{
	MODEL_NONE = 0,
	MODEL_PS2104 = 2104,
	MODEL_PS2105 = 2105,
	MODEL_PS2202 = 2202,
	MODEL_PS2203 = 2203,
	MODEL_PS2204 = 2204,
	MODEL_PS2205 = 2205,
	MODEL_PS2204A = 0xA204,
	MODEL_PS2205A = 0xA205
} PS2000_TYPE;

class daq_PS2000 : public daq {
	Q_OBJECT

	public:
		explicit daq_PS2000(QObject *parent);
		~daq_PS2000();
		void setAcquisitionParameters() override;
		std::array<std::vector<int32_t>, DAQ_MAX_CHANNELS> collectBlockData() override;
		void setOutputVoltage(double voltage) override;

		double getCurrentSamplingRate() override;

	public slots:
		void connect() override;
		void disconnect() override;

	private:
		void set_defaults(void) override;
		void get_info(void) override;

		int m_defaultTimebaseIndex{ 10 };
};

#endif // DAQ_PS2000_H