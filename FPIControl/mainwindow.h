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

class MainWindow : public QMainWindow {
	Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

private slots:
	void on_selectDisplay_activated(const int index);
	void on_acquisitionButton_clicked();
	void on_actionConnect_triggered();
	void on_actionDisconnect_triggered();
	void on_scanButton_clicked();
	void on_scanButtonManual_clicked();
	// SLOTS for setting the acquisitionParameters
	void on_sampleRate_activated(const int index);
	void on_chACoupling_activated(const int index);
	void on_chBCoupling_activated(const int index);
	void on_sampleNumber_valueChanged(const int no_of_samples);
	// SLOTS for setting the scanParameters
	void on_scanAmplitude_valueChanged(const double value);
	void on_scanOffset_valueChanged(const double value);
	void on_scanWaveform_activated(const int index);
	void on_scanFrequency_valueChanged(const double value);
	void on_scanSteps_valueChanged(const int value);

	// SLOTS for updating the plots
    void updateLiveView(std::array<QVector<QPointF>, PS2000_MAX_CHANNELS> data);
	void updateScanView();

	// SLOTS for updating the acquisition parameters
	void updateAcquisitionParameters(ACQUISITION_PARAMETERS acquisitionParameters);

public slots:
	void connectMarkers();
	void handleMarkerClicked();

private:
    Ui::MainWindow *ui;
	enum class liveViewPlotTypes {
		CHANNEL_A,
		CHANNEL_B
	};
	enum class scanViewPlotTypes {
		INTENSITY,
		A1,
		A2,
		QUOTIENTS
	};
	QtCharts::QChart *liveViewChart;
	QtCharts::QChart *lockViewChart;
	QtCharts::QChart *scanViewChart;
	QList<QtCharts::QLineSeries *> liveViewPlots;
	QList<QtCharts::QLineSeries *> scanViewPlots;
	daq d;
	int view = 0;	// selection of the view
};

#endif // MAINWINDOW_H