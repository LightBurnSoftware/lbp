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
	, m_width_um(300000)
	, m_height_um(180000)
	, m_x_px_offset(0)
	, m_y_px_offset(0)
{
	m_timer.start();
}

QSize SimView::sizeHint() const
{
	return QSize(400, 300);
}

void SimView::sizeForHeight(const QSize &size)
{
	int h_px = size.height();
	float ratio = (float) h_px / m_height_um;
	int w_px = (int) (ratio * m_width_um);
	m_x_px_offset = (size.width() - w_px) / 2;
	m_y_px_offset = 0;
	m_buffer = std::move(QImage(w_px, h_px, QImage::Format_RGB32));
}

void SimView::sizeForWidth(const QSize &size)
{
	int w_px = size.width();
	float ratio = (float) w_px / m_width_um;
	int h_px = (int) (ratio * m_height_um);
	m_x_px_offset = 0;
	m_y_px_offset = (size.height() - h_px) / 2;
	m_buffer = std::move(QImage(w_px, h_px, QImage::Format_RGB32));
}

void SimView::resizeEvent(QResizeEvent *event)
{
	if (m_width_um > m_height_um) {
		sizeForWidth(event->size());

	} else {
		sizeForHeight(event->size());
	}
	qDebug() << m_buffer.width() << m_buffer.height() << m_x_px_offset << m_y_px_offset;
}

int SimView::getXPx(int x) const
{
	float x_percent = m_width_um > 0 ? (float) x / m_width_um : 0.f;
	return (int) round((x_percent * m_buffer.width()));
}

int SimView::getYPx(int y) const
{
	float y_percent = m_height_um > 0 ? (float) y / m_height_um : 0.f;
	return (int) round((y_percent * m_buffer.height()));
}

void SimView::tick(SimState state)
{
	int64_t elapsed = m_timer.restart();
	float seconds = (float) elapsed / 1000.f;
	cool(0.01 * seconds);
	heat(state);
	update();
}

void SimView::heatPx(int x, int y, float percent, int channel)
{
	if (x >= 0 && x < m_buffer.width() && y >= 0 && y < m_buffer.width()) {
		QRgb px = m_buffer.pixel(x, y);
		int heat = 0xff & qMin(255, (int) (255 * percent));
		switch (channel) {
		case 1:
			px = qRed(px) | (heat << 8) | qBlue(px);
			break;
		case 2:
			px = qRed(px) | qGreen(px) | heat;
			break;
		case 0:
		default:
			px = (heat << 16) | qGreen(px) | qBlue(px);
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
				// _heatPx(cx + dx, cy + dy, scaled);
				heatPx(cx + dx, cy + dy, rate, channel);
			}
		}
	}
}

void SimView::clear()
{
	m_buffer.fill(QColor(0, 0, 0));
}

void SimView::heat(const SimState &state)
{
	int x = getXPx(state.pos.x);
	int y = getYPx(state.pos.y);
	heatBall(x, y, 3, 0.5, 0); // heat red channel for position no matter what.
	if (state.power > 0.f) {
		heatBall(x, y, 1, state.power, 2); // heat blue channel for cutting power.
	}
}

void SimView::cool(float rate)
{
	for (int y = 0; y < m_buffer.height(); y++) {
		QRgb *line = reinterpret_cast<QRgb *>(m_buffer.scanLine(y));
		for (int x = 0; x < m_buffer.width(); x++) {
			QRgb px = line[x];
			int heat = qRed(px);
			heat -= (1 + (int) (heat * rate));
			if (heat < 25) {
				heat = 0;
			}
			line[x] = qRgb(heat, 0, qBlue(px));
		}
	}
}

void SimView::paintEvent(QPaintEvent *event)
{
	QPainter painter(this);
	painter.translate(0, height());
	painter.scale(1, -1);
	painter.drawImage(m_x_px_offset, m_y_px_offset, m_buffer);
}
