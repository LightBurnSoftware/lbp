// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC


#include "simworker.h"

void SimWorker::init()
{

}

void SimWorker::onTimerTick()
{

}

/*
	int elapsed = m_sim_timer.restart();
	SimState state = m_sim.loop(elapsed);
	wSimView->tick(state);
	m_sim.setTransport(nullptr);
	if (m_transport) {
		m_transport->stop();
		m_transport->deleteLater();
		m_transport = nullptr;
	}

	if (m_transport) {
		m_transport->stop();
		disconnect(m_transport, &Transport::rxBytes, this, &MainWindow::onTransportRx);
		m_transport->deleteLater();
	}
	switch(wTransport->type()) {
	case Transport::Type::Tcp: {
		bool ok = false;
		int port = wTransport->port().toInt(&ok);
		if (ok) {
			m_transport = new TcpTransport(port, this);
		}
		else {
			m_transport = nullptr;
		}
	} break;
	case Transport::Type::Serial: {
		QString port = wTransport->port();
		int baud = wTransport->baudRate();
		if (!port.isEmpty() && baud > 0) {
			m_transport = new SerialTransport(port, baud, this);
		}
		else {
			m_transport = nullptr;
		}
	} break;
	default:
		m_transport = nullptr;
	}
	if (m_transport) {
		connect(m_transport, &Transport::rxBytes, this, &MainWindow::onTransportRx);
		m_transport->start();
	}
	m_sim.setTransport(m_transport);


void MainWindow::onTransportRx(const QByteArray &bytes)
{
	m_sim.rxCallback((const uint8_t *) bytes.constData(), bytes.size());
}
*/
