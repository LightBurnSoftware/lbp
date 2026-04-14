// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "simstate.h"
#include <QImage>
#include <QWidget>
#include <QtCore/qelapsedtimer.h>

class SimView : public QWidget
{
	Q_OBJECT
public:
	explicit SimView(QWidget *parent = nullptr);
	QSize sizeHint() const override;

	void tick(SimState state);

	void clear();

protected:
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	void sizeForHeight(const QSize &size);
	void sizeForWidth(const QSize &size);

	int getXPx(int x) const;
	int getYPx(int y) const;
	void cool(float rate);
	void heatPx(int x, int y, float rate, int channel = 0);
	void heatBall(int cx, int cy, int radius, float rate, int channel = 0);
	void heat(const SimState &state);

	QImage m_buffer;
	QElapsedTimer m_timer;

	// dimensions of the simulated workbed in micrometers
	int m_width_um;
	int m_height_um;

	// offset into the drawn widget where the workbed is located
	int m_x_px_offset;
	int m_y_px_offset;
};
