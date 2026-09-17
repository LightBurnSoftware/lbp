// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC


#include "simworker.h"
#include "transport.h"
#include "tcptransport.h"
#include "serialtransport.h"

#include <functional>

SimWorker::SimWorker() : QObject(nullptr)
{
	m_sim.setTxCallback(std::bind(&SimWorker::onTransportTx, this, std::placeholders::_1, std::placeholders::_2));
}

void SimWorker::init()
{
	// Empty
}

SimWorker::~SimWorker()
{
	stopTransport();
}

void SimWorker::timerEvent(QTimerEvent *event)
{
	int elapsed = m_sim_timer.restart();
	SimState state = m_sim.loop(elapsed);
}

void SimWorker::startTransport(Transport::Config config)
{
	if (m_transport) {
		m_transport->stop();
		disconnect(m_transport, &Transport::rxBytes, this, &SimWorker::onTransportRx);
		m_transport->deleteLater();
	}

	switch(config.type) {
	case Transport::Type::Tcp:
		if (config.port_num > 0) {
			m_transport = new TcpTransport(config.port_num, this);
		}
		else {
			m_transport = nullptr;
		}
		break;
	case Transport::Type::Serial:
		if (!config.port_name.isEmpty() && config.baud_rate > 0) {
			m_transport = new SerialTransport(config.port_name, config.baud_rate, this);
		}
		else {
			m_transport = nullptr;
		}
		break;
	default:
		m_transport = nullptr;
	}
	if (m_transport) {
		connect(m_transport, &Transport::rxBytes, this, &SimWorker::onTransportRx);
		m_transport->start();
	}
}

void SimWorker::stopTransport()
{
	if (m_transport) {
		m_transport->stop();
		m_transport->deleteLater();
		m_transport = nullptr;
	}
}

void SimWorker::onTransportRx(const QByteArray &bytes)
{
	m_sim.rxCallback((const uint8_t *) bytes.data(), bytes.size());
}

void SimWorker::onTransportTx(const uint8_t *bytes, size_t len)
{
	if (m_transport) {
		m_transport->sendBytes(bytes, len);
	}
}
