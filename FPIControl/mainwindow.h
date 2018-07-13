#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>

#include "daq.h"

namespace Ui {
	class MainWindow;
}


class IndicatorWidget : public QWidget {
	Q_OBJECT
		Q_PROPERTY(bool on READ isOn WRITE setOn)
public:
	IndicatorWidget(const QColor &color = QColor(246, 180, 0), QWidget *parent = 0)
		: QWidget(parent), m_color(color), m_on(false) {}

	bool isOn() const {
		return m_on;
	}

public slots:
	void turnOff() { setOn(false); }
	void turnOn() { setOn(true); }
	/* states */
	void failure() { setColor(QColor(246, 12, 0)); }	// failure
	void inactive() { setColor(QColor(246, 180, 0)); }	// inactive
	void active() { setColor(QColor(177, 243, 0)); }	// active, no error

protected:
	void paintEvent(QPaintEvent *) override {
		if (!m_on)
			return;
		QPainter painter(this);
		painter.setRenderHint(QPainter::Antialiasing);
		painter.setBrush(m_color);
		painter.drawEllipse(2, 2, 20, 20);
	}

private:
	QColor m_color;
	bool m_on;
	void setOn(bool on) {
		if (on == m_on)
			return;
		m_on = on;
		update();
	}
	void setColor(QColor color) {
		m_color = color;
		update();
	}
};

typedef struct {
	bool automatic = TRUE;
	double xmin;
	double xmax;
	double ymin;
	double ymax;
} AXIS_RANGE;

typedef struct {
	bool floatingView = FALSE;
	AXIS_RANGE liveView;
	AXIS_RANGE lockView;
	AXIS_RANGE scanView;
} VIEW_SETTINGS;

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
	void on_selectDisplay_activated(const int index);
	void on_floatingViewCheckBox_clicked(const bool checked);

	void on_acquisitionButton_clicked();
	void on_lockButton_clicked();
	void on_acquireLockButton_clicked();
	void on_actionConnect_triggered();
	void on_actionDisconnect_triggered();
	void on_actionConnect_Piezo_triggered();
	void on_actionDisconnect_Piezo_triggered();
	void on_scanButton_clicked();
	void on_scanButtonManual_clicked();
	// SLOTS for setting the acquisitionParameters
	void on_sampleRate_activated(const int index);
	void on_chACoupling_activated(const int index);
	void on_chBCoupling_activated(const int index);
	void on_chARange_activated(const int index);
	void on_chBRange_activated(const int index);
	void on_sampleNumber_valueChanged(const int no_of_samples);
	// SLOTS for setting the scanParameters
	void on_scanAmplitude_valueChanged(const double value);
	void on_scanOffset_valueChanged(const double value);
	void on_scanWaveform_activated(const int index);
	void on_scanFrequency_valueChanged(const double value);
	void on_scanSteps_valueChanged(const int value);

	void on_proportionalTerm_valueChanged(const double value);
	void on_integralTerm_valueChanged(const double value);
	void on_derivativeTerm_valueChanged(const double value);
	void on_frequency_valueChanged(const double value);
	void on_phase_valueChanged(const double value);

	void on_enablePiezoCheckBox_clicked(const bool checked);
	void on_incrementVoltage_clicked();
	void on_decrementVoltage_clicked();
	void on_offsetCheckBox_clicked(const bool checked);

	// SLOTS for updating the plots
	void updateLiveView(std::array<QVector<QPointF>, PS2000A_MAX_CHANNELS> &data);
	void updateScanView();
	void updateLockView(std::array<QVector<QPointF>, static_cast<int>(lockViewPlotTypes::COUNT)> &data);

	// SLOTS for updating the acquisition parameters
	void updateAcquisitionParameters(ACQUISITION_PARAMETERS acquisitionParameters);

	// SLOTS for updating the locking state
	void updateLockState(LOCKSTATE lockState);

	// SLOT for updating the compensation state
	void updateCompensationState(bool compensating);

	void on_actionAbout_triggered();

public slots:
	void connectMarkers();
	void handleMarkerClicked();

private:
    Ui::MainWindow *ui;
	QtCharts::QChart *liveViewChart;
	QtCharts::QChart *lockViewChart;
	QtCharts::QChart *scanViewChart;
	QVector<QtCharts::QLineSeries *> liveViewPlots;
	QVector<QtCharts::QLineSeries *> lockViewPlots;
	QVector<QtCharts::QLineSeries *> scanViewPlots;
	daq d;
	int view = 0;	// selection of the view
	IndicatorWidget *lockIndicator;
	QLabel *compensationIndicator;
	QLabel *lockInfo;
	QLabel *compensationInfo;
	QLabel *statusInfo;
	VIEW_SETTINGS viewSettings;
};

#endif // MAINWINDOW_H
