#ifndef COLORS_H
#define COLORS_H

#include <QtWidgets>

typedef struct {
	QColor blue		= QColor(  0, 114, 189, 255);
	QColor orange	= QColor(217,  83,  25, 255);
	QColor yellow	= QColor(237, 177,  32, 255);
	QColor purple	= QColor(126,  47, 142, 255);
	QColor green	= QColor(119, 172,  48, 255);
	QColor skyblue	= QColor( 77, 190, 238, 255);
	QColor red		= QColor(162,  20,  47, 255);
} COLORS;

COLORS colors;

#endif // COLORS_H