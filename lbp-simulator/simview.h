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

	void tick();
	void heat(SimState state);
	void cool(float rate);

	void clear();

protected:
	void resizeEvent(QResizeEvent *event) override;
	void paintEvent(QPaintEvent *event) override;

private:
	void sizeForHeight(const QSize &size);
	void sizeForWidth(const QSize &size);
	/** Size the buffer to the widget, keeping the bed's aspect ratio, and clear it. */
	void layoutBuffer(const QSize &size);

	int64_t widthUm() const { return m_max_um.x - m_min_um.x; }
	int64_t heightUm() const { return m_max_um.y - m_min_um.y; }
	int getXPx(int64_t x) const;
	int getYPx(int64_t y) const;

	void heatPx(int x, int y, float rate, int channel = 0);
	void heatBall(int cx, int cy, int radius, float rate, int channel = 0);

	QImage m_buffer;
	QElapsedTimer m_timer;

	// extent of the simulated workbed (gantry) or marking field (galvo) in micrometers, from SimState
	Vec8 m_min_um;
	Vec8 m_max_um;
	MachineMode m_mode = MachineMode::Gantry;

	// offset into the drawn widget where the workbed is located
	int m_x_px_offset;
	int m_y_px_offset;
};
