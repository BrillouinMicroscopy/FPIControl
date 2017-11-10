#include "mainwindow.h"
#include "daq.h"
#include "ui_mainwindow.h"
#include "colors.h"
#include <QtWidgets>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>
#include <QtCharts/QChart>
#include <cstdlib>
#include <iostream>
#include <ctime>
#include <string>
#include <QtCharts/QLegendMarker>
#include <QtCharts/QXYLegendMarker>
#include "version.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow) {

    ui->setupUi(this);

	QWidget::connect(&d, SIGNAL(scanDone()), this, SLOT(updateScanView()));
	QWidget::connect(&d, SIGNAL(locked(std::array<QVector<QPointF>, static_cast<int>(lockViewPlotTypes::COUNT)> &)), this,
		SLOT(updateLockView(std::array<QVector<QPointF>, static_cast<int>(lockViewPlotTypes::COUNT)> &)));
	QWidget::connect(&d, SIGNAL(collectedBlockData(std::array<QVector<QPointF>, PS2000A_MAX_CHANNELS> &)), this,
		SLOT(updateLiveView(std::array<QVector<QPointF>, PS2000A_MAX_CHANNELS> &)));

	QWidget::connect(&d, SIGNAL(acquisitionParametersChanged(ACQUISITION_PARAMETERS)), this,
		SLOT(updateAcquisitionParameters(ACQUISITION_PARAMETERS)));

	QWidget::connect(&d, SIGNAL(lockStateChanged(LOCKSTATE)), this,
		SLOT(updateLockState(LOCKSTATE)));

	QWidget::connect(&d, SIGNAL(compensationStateChanged(bool)), this,
		SLOT(updateCompensationState(bool)));
	
	// set up live view plots
	liveViewPlots.resize(static_cast<int>(liveViewPlotTypes::COUNT));

	QtCharts::QLineSeries *channelA = new QtCharts::QLineSeries();
	channelA->setUseOpenGL(true);
	channelA->setColor(colors.blue);
	channelA->setName(QString("Detector signal"));
	liveViewPlots[static_cast<int>(liveViewPlotTypes::CHANNEL_A)] = channelA;

	QtCharts::QLineSeries *channelB = new QtCharts::QLineSeries();
	channelB->setUseOpenGL(true);
	channelB->setColor(colors.orange);
	channelB->setName(QString("Reference Signal"));
	liveViewPlots[static_cast<int>(liveViewPlotTypes::CHANNEL_B)] = channelB;
	
	// set up live view chart
	liveViewChart = new QtCharts::QChart();
	foreach(QtCharts::QLineSeries* series, liveViewPlots) {
		liveViewChart->addSeries(series);
	}
	liveViewChart->createDefaultAxes();
	liveViewChart->axisX()->setRange(0, 1024);
	liveViewChart->axisY()->setRange(-1, 4);
	liveViewChart->axisX()->setTitleText("Samples");
	liveViewChart->axisY()->setTitleText("Voltage [V]");
	liveViewChart->setTitle("Live view");
	liveViewChart->layout()->setContentsMargins(0, 0, 0, 0);

	// set up lock view plots
	lockViewPlots.resize(static_cast<int>(lockViewPlotTypes::COUNT));

	QtCharts::QLineSeries *voltage = new QtCharts::QLineSeries();
	voltage->setUseOpenGL(true);
	voltage->setColor(colors.orange);
	voltage->setName(QString("Voltage"));
	lockViewPlots[static_cast<int>(lockViewPlotTypes::VOLTAGE)] = voltage;

	QtCharts::QLineSeries *errorLock = new QtCharts::QLineSeries();
	errorLock->setUseOpenGL(true);
	errorLock->setColor(colors.yellow);
	errorLock->setName(QString("Error signal"));
	lockViewPlots[static_cast<int>(lockViewPlotTypes::ERRORSIGNAL)] = errorLock;

	QtCharts::QLineSeries *intensityLock = new QtCharts::QLineSeries();
	intensityLock->setUseOpenGL(true);
	intensityLock->setColor(colors.blue);
	intensityLock->setName(QString("Signal amplitude"));
	lockViewPlots[static_cast<int>(lockViewPlotTypes::AMPLITUDE)] = intensityLock;

	QtCharts::QLineSeries *piezoVoltage = new QtCharts::QLineSeries();
	piezoVoltage->setUseOpenGL(true);
	piezoVoltage->setColor(colors.purple);
	piezoVoltage->setName(QString("Piezo Voltage"));
	lockViewPlots[static_cast<int>(lockViewPlotTypes::PIEZOVOLTAGE)] = piezoVoltage;

	QtCharts::QLineSeries *errorMean = new QtCharts::QLineSeries();
	errorMean->setUseOpenGL(true);
	errorMean->setColor(colors.green);
	errorMean->setName(QString("Error signal mean"));
	lockViewPlots[static_cast<int>(lockViewPlotTypes::ERRORSIGNALMEAN)] = errorMean;

	QtCharts::QLineSeries *errorStd = new QtCharts::QLineSeries();
	errorStd->setUseOpenGL(true);
	errorStd->setColor(colors.red);
	errorStd->setName(QString("Error signal standard deviation"));
	lockViewPlots[static_cast<int>(lockViewPlotTypes::ERRORSIGNALSTD)] = errorStd;

	// set up lock view chart
	lockViewChart = new QtCharts::QChart();
	foreach(QtCharts::QLineSeries* series, lockViewPlots) {
		lockViewChart->addSeries(series);
	}
	lockViewChart->createDefaultAxes();
	lockViewChart->axisX()->setRange(0, 1024);
	lockViewChart->axisY()->setRange(-1, 4);
	lockViewChart->axisX()->setTitleText("Time [s]");
	lockViewChart->setTitle("Lock View");
	lockViewChart->layout()->setContentsMargins(0, 0, 0, 0);

	// set up scan view plots
	scanViewPlots.resize(static_cast<int>(scanViewPlotTypes::COUNT));

	QtCharts::QLineSeries *intensity = new QtCharts::QLineSeries();
	intensity->setUseOpenGL(true);
	intensity->setColor(colors.orange);
	intensity->setName(QString("Intensity"));
	scanViewPlots[static_cast<int>(scanViewPlotTypes::INTENSITY)] = intensity;

	QtCharts::QLineSeries *error = new QtCharts::QLineSeries();
	error->setUseOpenGL(true);
	error->setColor(colors.yellow);
	error->setName(QString("Error signal"));
	scanViewPlots[static_cast<int>(scanViewPlotTypes::ERRORSIGNAL)] = error;

	// set up live view chart
	scanViewChart = new QtCharts::QChart();
	foreach(QtCharts::QLineSeries* series, scanViewPlots) {
		scanViewChart->addSeries(series);
	}
	scanViewChart->createDefaultAxes();
	scanViewChart->axisX()->setRange(0, 1024);
	scanViewChart->axisY()->setRange(-1, 4);
	scanViewChart->axisX()->setTitleText("Piezo Voltage [V]");
	scanViewChart->setTitle("Scan View");
	scanViewChart->layout()->setContentsMargins(0, 0, 0, 0);

	// set live view chart as default chart
    ui->plotAxes->setChart(liveViewChart);
    ui->plotAxes->setRenderHint(QPainter::Antialiasing);
	ui->plotAxes->setRubberBand(QtCharts::QChartView::RectangleRubberBand);

	// set default values of GUI elements
	SCAN_PARAMETERS scanParameters = d.getScanParameters();
	ui->scanAmplitude->setValue(scanParameters.amplitude / static_cast<double>(1e6));
	ui->scanOffset->setValue(scanParameters.offset / static_cast<double>(1e6));
	ui->scanFrequency->setValue(scanParameters.frequency);
	ui->scanSteps->setValue(scanParameters.nrSteps);
	ui->scanWaveform->setCurrentIndex(scanParameters.waveform);

	// set default values of lockin parameters
	LOCK_PARAMETERS lockParameters = d.getLockParameters();
	ui->proportionalTerm->setValue(lockParameters.proportional);
	ui->integralTerm->setValue(lockParameters.integral);
	ui->derivativeTerm->setValue(lockParameters.derivative);
	ui->frequency->setValue(lockParameters.frequency);
	ui->phase->setValue(lockParameters.phase);
	ui->offsetCheckBox->setChecked(lockParameters.compensate);

	// connect legend marker to toggle visibility of plots
	MainWindow::connectMarkers();

	/* Add labels and indicator to status bar */
	// Status info
	statusInfo = new QLabel("");
	statusInfo->setAlignment(Qt::AlignLeft);
	ui->statusBar->addPermanentWidget(statusInfo, 1);

	// Compensating info
	compensationInfo = new QLabel("Compensating voltage offset");
	compensationInfo->setAlignment(Qt::AlignRight);
	compensationInfo->hide();
	ui->statusBar->addPermanentWidget(compensationInfo, 0);

	// Compensating indicator
	compensationIndicator = new QLabel;
	QMovie *movie = new QMovie(QDir::currentPath() + "/loader.gif");
	compensationIndicator->setMovie(movie);
	compensationIndicator->hide();
	movie->start();
	ui->statusBar->addPermanentWidget(compensationIndicator, 0);

	// Locking info
	lockInfo = new QLabel("Locking Inactive");
	lockInfo->setAlignment(Qt::AlignRight);
	ui->statusBar->addPermanentWidget(lockInfo, 0);

	// Locking indicator
	lockIndicator = new IndicatorWidget();
	lockIndicator->turnOn();
	lockIndicator->setMinimumSize(26, 24);
	ui->statusBar->addPermanentWidget(lockIndicator, 0);

	// style status bar to not show items borders
	ui->statusBar->setStyleSheet(QString("QStatusBar::item { border: none; }"));

	// hide floating view elements by default
	ui->floatingViewLabel->hide();
	ui->floatingViewCheckBox->hide();
}

MainWindow::~MainWindow() {
    delete ui;
}

void MainWindow::on_actionAbout_triggered() {
	QString str = QString("FPIControl Version %1.%2.%3 <br> Build from commit: <a href='%4'>%5</a><br>Author: <a href='mailto:%6?subject=FPIControl'>%7</a><br>Date: %8")
		.arg(Version::MAJOR)
		.arg(Version::MINOR)
		.arg(Version::PATCH)
		.arg(Version::Url.c_str())
		.arg(Version::Commit.c_str())
		.arg(Version::AuthorEmail.c_str())
		.arg(Version::Author.c_str())
		.arg(Version::Date.c_str());

	QMessageBox::about(this, tr("About FPIControl"), str);
}

void MainWindow::connectMarkers() {
	// Connect all markers to handler
	foreach(QtCharts::QLegendMarker* marker, liveViewChart->legend()->markers()) {
		// Disconnect possible existing connection to avoid multiple connections
		QWidget::disconnect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
		QWidget::connect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
	}
	foreach(QtCharts::QLegendMarker* marker, lockViewChart->legend()->markers()) {
		// Disconnect possible existing connection to avoid multiple connections
		QWidget::disconnect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
		QWidget::connect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
	}
	foreach(QtCharts::QLegendMarker* marker, scanViewChart->legend()->markers()) {
		// Disconnect possible existing connection to avoid multiple connections
		QWidget::disconnect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
		QWidget::connect(marker, SIGNAL(clicked()), this, SLOT(handleMarkerClicked()));
	}
}

void MainWindow::handleMarkerClicked() {
	QtCharts::QLegendMarker* marker = qobject_cast<QtCharts::QLegendMarker*> (sender());
	Q_ASSERT(marker);

	switch (marker->type()) {
		case QtCharts::QLegendMarker::LegendMarkerTypeXY: {
			// Toggle visibility of series
			marker->series()->setVisible(!marker->series()->isVisible());

			// Turn legend marker back to visible, since hiding series also hides the marker
			// and we don't want it to happen now.
			marker->setVisible(true);

			// Dim the marker, if series is not visible
			qreal alpha = 1.0;

			if (!marker->series()->isVisible()) {
				alpha = 0.5;
			}

			QColor color;
			QBrush brush = marker->labelBrush();
			color = brush.color();
			color.setAlphaF(alpha);
			brush.setColor(color);
			marker->setLabelBrush(brush);

			brush = marker->brush();
			color = brush.color();
			color.setAlphaF(alpha);
			brush.setColor(color);
			marker->setBrush(brush);

			QPen pen = marker->pen();
			color = pen.color();
			color.setAlphaF(alpha);
			pen.setColor(color);
			marker->setPen(pen);

			break;
		}
	}
}

void MainWindow::on_acquisitionButton_clicked() {
	bool running = d.startStopAcquisition();
	if (running) {
		ui->acquisitionButton->setText(QString("Stop"));
	} else {
		ui->acquisitionButton->setText(QString("Acquire"));
	}
}

void MainWindow::on_acquireLockButton_clicked() {
	bool running = d.startStopAcquireLocking();
	if (running) {
		ui->acquireLockButton->setText(QString("Stop"));
	}
	else {
		ui->lockButton->setText(QString("Lock"));
		ui->acquireLockButton->setText(QString("Acquire"));
	}
}

void MainWindow::on_lockButton_clicked() {
	d.startStopLocking();
	//if (running) {
	//	ui->lockButton->setText(QString("Unlock"));
	//}
	//else {
	//	ui->lockButton->setText(QString("Lock"));
	//}
}

void MainWindow::on_sampleRate_activated(const int index) {
	d.setSampleRate(index);
}

void MainWindow::on_chACoupling_activated(const int index) {
	d.setCoupling(index, 0);
}

void MainWindow::on_chBCoupling_activated(const int index) {
	d.setCoupling(index, 1);
}

void MainWindow::on_chARange_activated(const int index) {
	d.setRange(index, 0);
}

void MainWindow::on_chBRange_activated(const int index) {
	d.setRange(index, 1);
}

void MainWindow::on_sampleNumber_valueChanged(const int no_of_samples) {
	d.setNumberSamples(no_of_samples);
}

void MainWindow::on_scanAmplitude_valueChanged(const double value) {
	// value is in [V], has to me set in [mV]
	d.setScanParameters(0, static_cast<int>(1e6*value));
}

void MainWindow::on_scanOffset_valueChanged(const double value) {
	// value is in [V], has to me set in [mV]
	d.setScanParameters(1, static_cast<int>(1e6*value));
}

void MainWindow::on_scanWaveform_activated(const int index) {
	d.setScanParameters(2, index);
}

void MainWindow::on_scanFrequency_valueChanged(const double value) {
	d.setScanFrequency(value);
}

void MainWindow::on_scanSteps_valueChanged(const int value) {
	d.setScanParameters(4, value);
}

/***********************************
 * Set parameters for cavity locking 
 ***********************************/
void MainWindow::on_proportionalTerm_valueChanged(const double value) {
	d.setLockParameters(0, value);
}
void MainWindow::on_integralTerm_valueChanged(const double value) {
	d.setLockParameters(1, value);
}
void MainWindow::on_derivativeTerm_valueChanged(const double value) {
	d.setLockParameters(2, value);
}
void MainWindow::on_frequency_valueChanged(const double value) {
	d.setLockParameters(3, value);
}
void MainWindow::on_phase_valueChanged(const double value) {
	d.setLockParameters(4, value);
}
void MainWindow::on_offsetCheckBox_clicked(const bool checked) {
	if (checked) {
		d.toggleOffsetCompensation(checked);
	} else {
		d.toggleOffsetCompensation(checked);
	}
}

void MainWindow::on_incrementVoltage_clicked() {
	d.incrementPiezoVoltage();
}

void MainWindow::on_decrementVoltage_clicked() {
	d.decrementPiezoVoltage();
}

void MainWindow::updateLiveView(std::array<QVector<QPointF>, PS2000A_MAX_CHANNELS> &data) {
	if (view == 0) {
		int channel = 0;
		foreach(QtCharts::QLineSeries* series, liveViewPlots) {
			if (series->isVisible()) {
				series->replace(data[channel]);
			}
			++channel;
		}
		liveViewChart->axisX()->setRange(0, data[0].length());
	}
}

void MainWindow::updateLockView(std::array<QVector<QPointF>, static_cast<int>(lockViewPlotTypes::COUNT)> &data) {
	if (view == 1) {
		int channel = 0;
		foreach(QtCharts::QLineSeries* series, lockViewPlots) {
			if (series->isVisible()) {
				series->replace(data[channel]);
			}
			++channel;
		}
		if (viewSettings.floatingView) {
			// show last 60 seconds
			double minX = data[0].back().x() - 60;
			minX = (minX < 0) ? 0 : minX;
			lockViewChart->axisX()->setRange(minX, data[0].back().x());
		} else {
			lockViewChart->axisX()->setRange(data[0][0].x(), data[0].back().x());
		}
		
	}
}

void MainWindow::updateScanView() {
	if (view == 2) {
		SCAN_DATA scanData = d.getScanData();
		QVector<QPointF> intensity;
		intensity.reserve(scanData.nrSteps);
		QVector<QPointF> error;
		error.reserve(scanData.nrSteps);
		for (int j(0); j < scanData.nrSteps; j++) {
			intensity.append(QPointF(scanData.voltages[j] / static_cast<double>(1e6), scanData.intensity[j] / static_cast<double>(1000)));
			error.append(QPointF(scanData.voltages[j] / static_cast<double>(1e6), std::real(scanData.error[j])));
		}
		scanViewPlots[static_cast<int>(scanViewPlotTypes::INTENSITY)]->replace(intensity);
		scanViewPlots[static_cast<int>(scanViewPlotTypes::ERRORSIGNAL)]->replace(error);

		scanViewChart->axisX()->setRange(0, 2);
		scanViewChart->axisY()->setRange(-0.4, 1.2);
	}
}

void MainWindow::updateAcquisitionParameters(ACQUISITION_PARAMETERS acquisitionParameters) {
	// set sample rate
	ui->sampleRate->setCurrentIndex(acquisitionParameters.timebase);
	// set number of samples
	ui->sampleNumber->setValue(acquisitionParameters.no_of_samples);
	// set range
	ui->chARange->setCurrentIndex(acquisitionParameters.channelSettings[0].range);
	ui->chBRange->setCurrentIndex(acquisitionParameters.channelSettings[1].range);
	// set coupling
	ui->chACoupling->setCurrentIndex(acquisitionParameters.channelSettings[0].coupling);
	ui->chBCoupling->setCurrentIndex(acquisitionParameters.channelSettings[1].coupling);
}

void MainWindow::updateLockState(LOCKSTATE lockState) {
	switch (lockState) {
		case LOCKSTATE::ACTIVE:
			lockInfo->setText("Locking active");
			ui->lockButton->setText(QString("Unlock"));
			lockIndicator->active();
			break;
		case LOCKSTATE::INACTIVE:
			lockInfo->setText("Locking inactive");
			ui->lockButton->setText(QString("Lock"));
			lockIndicator->inactive();
			break;
		case LOCKSTATE::FAILURE:
			lockInfo->setText("Locking failure");
			ui->lockButton->setText(QString("Lock"));
			lockIndicator->failure();
			break;
	}
}

void MainWindow::updateCompensationState(bool compensating) {
	if (compensating) {
		compensationIndicator->show();
		compensationInfo->show();
	} else {
		compensationIndicator->hide();
		compensationInfo->hide();
	}
}

void MainWindow::on_scanButtonManual_clicked() {
	d.scanManual();
}

void MainWindow::on_selectDisplay_activated(const int index) {
	view = index;
	switch (index) {
		case 0:
			// it is necessary to hide the series, because they do not get removed
			// after setChart in case useOpenGL == true (bug in QT?)
			foreach(QtCharts::QLineSeries* series, scanViewPlots) {
				series->setVisible(false);
			}
			foreach(QtCharts::QLineSeries* series, lockViewPlots) {
				series->setVisible(false);
			}
			foreach(QtCharts::QLineSeries* series, liveViewPlots) {
				series->setVisible(true);
			}
			ui->plotAxes->setChart(liveViewChart);
			ui->floatingViewLabel->hide();
			ui->floatingViewCheckBox->hide();
			//MainWindow::updateLiveView();
			break;
		case 1:
			// it is necessary to hide the series, because they do not get removed
			// after setChart in case useOpenGL == true (bug in QT?)
			foreach(QtCharts::QLineSeries* series, liveViewPlots) {
				series->setVisible(false);
			}
			foreach(QtCharts::QLineSeries* series, scanViewPlots) {
				series->setVisible(false);
			}
			foreach(QtCharts::QLineSeries* series, lockViewPlots) {
				series->setVisible(true);
			}
			ui->plotAxes->setChart(lockViewChart);
			ui->floatingViewLabel->show();
			ui->floatingViewCheckBox->show();
			//MainWindow::updateScanView();
			break;
		case 2:
			// it is necessary to hide the series, because they do not get removed
			// after setChart in case useOpenGL == true (bug in QT?)
			foreach(QtCharts::QLineSeries* series, liveViewPlots) {
				series->setVisible(false);
			}
			foreach(QtCharts::QLineSeries* series, lockViewPlots) {
				series->setVisible(false);
			}
			foreach(QtCharts::QLineSeries* series, scanViewPlots) {
				series->setVisible(true);
			}
			ui->plotAxes->setChart(scanViewChart);
			ui->floatingViewLabel->hide();
			ui->floatingViewCheckBox->hide();
			MainWindow::updateScanView();
			break;
	}
}

void MainWindow::on_floatingViewCheckBox_clicked(const bool checked) {
	viewSettings.floatingView = checked;
}

void MainWindow::on_actionConnect_triggered() {
	bool connected = d.connect();
	if (connected) {
		ui->actionConnect->setEnabled(false);
		ui->actionDisconnect->setEnabled(true);
		statusInfo->setText("Successfully connected");
	} else {
		ui->actionConnect->setEnabled(true);
		ui->actionDisconnect->setEnabled(false);
	}
}

void MainWindow::on_actionDisconnect_triggered() {
	bool connected = d.disconnect();
	if (connected) {
		ui->actionConnect->setEnabled(false);
		ui->actionDisconnect->setEnabled(true);
	}  else {
		ui->actionConnect->setEnabled(true);
		ui->actionDisconnect->setEnabled(false);
		statusInfo->setText("Successfully disconnected");
	}
}

void MainWindow::on_actionConnect_Piezo_triggered() {
	bool connected = d.connectPiezo();
	if (connected) {
		ui->actionConnect_Piezo->setEnabled(false);
		ui->actionDisconnect_Piezo->setEnabled(true);
	}
	else {
		ui->actionConnect_Piezo->setEnabled(true);
		ui->actionDisconnect_Piezo->setEnabled(false);
	}
}

void MainWindow::on_actionDisconnect_Piezo_triggered() {
	bool connected = d.disconnectPiezo();
	if (connected) {
		ui->actionConnect_Piezo->setEnabled(false);
		ui->actionDisconnect_Piezo->setEnabled(true);
	}
	else {
		ui->actionConnect_Piezo->setEnabled(true);
		ui->actionDisconnect_Piezo->setEnabled(false);
	}
}

void MainWindow::on_enablePiezoCheckBox_clicked(const bool checked) {
	if (checked) {
		d.enablePiezo();
	}
	else {
		d.disablePiezo();
	}
}

void MainWindow::on_scanButton_clicked() {
	d.set_sig_gen();
}