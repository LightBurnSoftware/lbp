// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "simview.h"

#include <QDebug>
#include <QPainter>
#include <QResizeEvent>
#include <QRgb>

SimView::SimView(QWidget *parent)
	: QWidget(parent)
	, m_buffer(400, 300, QImage::Format_RGB32)
	, m_timer()
	, m_min_um()
	, m_max_um()
	, m_x_px_offset(0)
	, m_y_px_offset(0)
{
	// A gantry bed until the first SimState arrives.
	m_max_um.x = 300000;
	m_max_um.y = 180000;
	m_timer.start();
}

QSize SimView::sizeHint() const
{
	return QSize(400, 300);
}

void SimView::sizeForHeight(const QSize &size)
{
	int h_px = size.height();
	float ratio = (float) h_px / heightUm();
	int w_px = (int) (ratio * widthUm());
	m_x_px_offset = (size.width() - w_px) / 2;
	m_y_px_offset = 0;
	m_buffer = std::move(QImage(w_px, h_px, QImage::Format_RGB32));
}

void SimView::sizeForWidth(const QSize &size)
{
	int w_px = size.width();
	float ratio = (float) w_px / widthUm();
	int h_px = (int) (ratio * heightUm());
	m_x_px_offset = 0;
	m_y_px_offset = (size.height() - h_px) / 2;
	m_buffer = std::move(QImage(w_px, h_px, QImage::Format_RGB32));
}

void SimView::resizeEvent(QResizeEvent *event)
{
	layoutBuffer(event->size());
}

void SimView::layoutBuffer(const QSize &size)
{
	if (widthUm() <= 0 || heightUm() <= 0) {
		return;
	}
	// Fit whichever dimension leaves the other inside the widget.
	if ((double) widthUm() / heightUm() > (double) size.width() / qMax(1, size.height())) {
		sizeForWidth(size);
	} else {
		sizeForHeight(size);
	}
	qDebug() << m_buffer.width() << m_buffer.height() << m_x_px_offset << m_y_px_offset;
	clear();
}

int SimView::getXPx(int64_t x) const
{
	float x_percent = widthUm() > 0 ? (float) (x - m_min_um.x) / widthUm() : 0.f;
	return (int) round((x_percent * m_buffer.width()));
}

int SimView::getYPx(int64_t y) const
{
	float y_percent = heightUm() > 0 ? (float) (y - m_min_um.y) / heightUm() : 0.f;
	return (int) round((y_percent * m_buffer.height()));
}

void SimView::tick()
{
	int64_t elapsed = m_timer.restart();
	float seconds = (float) elapsed / 1000.f;
	cool(0.2 * seconds);
	update();
}

void SimView::heatPx(int x, int y, float rate, int channel)
{
	if (x >= 0 && x < m_buffer.width() && y >= 0 && y < m_buffer.height()) {
		QRgb px = m_buffer.pixel(x, y);
		int heat = qMin(255, (int) (255 * rate));
		switch (channel) {
		case 1:
			px = qRgb(qRed(px), heat, qBlue(px));
			break;
		case 2:
			px = qRgb(qRed(px), qGreen(px), heat);
			break;
		case 0:
		default:
			px = qRgb(heat, qGreen(px), qBlue(px));
		}
		m_buffer.setPixel(x, y, px);
	}
}

void SimView::heatBall(int cx, int cy, int radius, float rate, int channel)
{
	int r2 = radius * radius;
	for (int dx = -radius; dx <= radius; dx++) {
		int x2 = dx * dx;
		for (int dy = -radius; dy <= radius; dy++) {
			int y2 = dy * dy;
			if ((x2 + y2) <= r2) {
				// float scaled = rate * (1.f - (float) (x2 + y2) / r2);
				// heatPx(cx + dx, cy + dy, scaled);
				heatPx(cx + dx, cy + dy, rate, channel);
			}
		}
	}
}

void SimView::clear()
{
	m_buffer.fill(QColor(0, 0, 0));
}

void SimView::heat(SimState state)
{
	// The bed (or galvo field) comes from the simulation; re-fit the view when it changes.
	if (state.mode != m_mode || state.min_pos.x != m_min_um.x || state.min_pos.y != m_min_um.y
		|| state.max_pos.x != m_max_um.x || state.max_pos.y != m_max_um.y) {
		m_mode = state.mode;
		m_min_um = state.min_pos;
		m_max_um = state.max_pos;
		layoutBuffer(size());
	}

	// Channels: red = position (fades), green = laser cutting (stays), blue = guide light (fades).
	int x = getXPx(state.pos.x);
	int y = getYPx(state.pos.y);
	heatBall(x,
			 y,
			 3,
			 qMax(0.5, state.laser_1_power),
			 0); // heat red channel for position no matter what.
	if (state.laser_1_power > 0.f) {
		heatBall(x, y, 0, state.laser_1_power, 1); // heat green channel for cutting power.
	}
	if (state.guide_light) {
		heatBall(x, y, 1, 1.f, 2); // heat blue channel for the guide light.
	}
}

void SimView::cool(float rate)
{
	for (int y = 0; y < m_buffer.height(); y++) {
		QRgb *line = reinterpret_cast<QRgb *>(m_buffer.scanLine(y));
		for (int x = 0; x < m_buffer.width(); x++) {
			// Position (red) and guide light (blue) fade; cutting (green) stays.
			QRgb px = line[x];
			int heat = qRed(px);
			heat -= (1 + (int) (heat * rate));
			if (heat < 10) {
				heat = 0;
			}
			int guide = qBlue(px);
			guide -= (1 + (int) (guide * rate));
			if (guide < 10) {
				guide = 0;
			}
			line[x] = qRgb(heat, qGreen(px), guide);
		}
	}
}

void SimView::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.translate(0, height());
	painter.scale(1, -1);
	painter.drawImage(m_x_px_offset, m_y_px_offset, m_buffer);

	if (m_mode == MachineMode::Galvo) {
		// Crosshairs through the centre of the field: galvo (0,0).
		const int cx = m_x_px_offset + getXPx(0);
		const int cy = m_y_px_offset + getYPx(0);
		painter.setPen(QColor(70, 70, 70));
		painter.drawLine(m_x_px_offset, cy, m_x_px_offset + m_buffer.width(), cy);
		painter.drawLine(cx, m_y_px_offset, cx, m_y_px_offset + m_buffer.height());
	}
}
