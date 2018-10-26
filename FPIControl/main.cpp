#include "mainwindow.h"
#include <Windows.h>
#include <QApplication>
#include <QtWidgets/QApplication>
#include <QtWidgets/QMainWindow>
#include <QtCharts/QChartView>
#include <QtCharts/QLineSeries>

int main(int argc, char *argv[]) {
//    QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);

    #ifdef Q_OS_WIN
        SetProcessDPIAware(); // call before the main event loop
    #endif // Q_OS_WIN

    #if QT_VERSION >= QT_VERSION_CHECK(5,6,0)
		QApplication::setAttribute(Qt::AA_EnableHighDpiScaling);
		QApplication::setAttribute(Qt::AA_UseHighDpiPixmaps);
    #else
        qputenv("QT_DEVICE_PIXEL_RATIO", QByteArray("1"));
    #endif // QT_VERSION

    QApplication a(argc, argv);

    MainWindow w;
    //w.setCentralWidget(chartView);
    w.show();

    return a.exec();
}
