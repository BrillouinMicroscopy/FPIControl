#ifndef DAQ_PS2000A_H
#define DAQ_PS2000A_H

#include <QMainWindow>
#include <QtCore/QObject>
#include <QtWidgets>
#include <vector>
#include <array>
#include <chrono>
#include <ctime>

#include <gsl/gsl>
#include "ps2000aApi.h"
#include "daq.h"
#include "..\circularBuffer.h"
#include "..\generalmath.h"

#define DAQ_BUFFER_SIZE 	8000
#define SINGLE_CH_SCOPE 1				// Single channel scope
#define DUAL_SCOPE 2					// Dual channel scope
#define QUAD_SCOPE 4

typedef enum class PS2000AType{
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
} PS2000A_TYPE;

class daq_PS2000A : public daq {
	Q_OBJECT

	public:
		explicit daq_PS2000A(QObject *parent);
		~daq_PS2000A();
		void setAcquisitionParameters();
		std::array<std::vector<int32_t>, DAQ_MAX_CHANNELS> collectBlockData();
		void setOutputVoltage(double voltage);

		double getCurrentSamplingRate() override;

	public slots:
		void connect_daq();
		void disconnect_daq();

	private:
		void set_defaults(void);
		void get_info(void);

		int m_defaultTimebaseIndex = 12;

		int16_t buffers[PS2000A_MAX_CHANNEL_BUFFERS][DAQ_BUFFER_SIZE * sizeof(int16_t)]{ 0 };
};

#endif // DAQ_PS2000A_H