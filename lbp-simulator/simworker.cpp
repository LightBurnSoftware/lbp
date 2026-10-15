// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC


#include "simworker.h"
#include "transport.h"
#include "tcptransport.h"
#include "serialtransport.h"

#include <QThread>

#include <functional>

#include "log.h"

SimWorker::SimWorker() : QObject(nullptr)
{
	// Empty
}

void SimWorker::init()
{
	m_sim.setTxCallback(std::bind(&SimWorker::onTransportTx, this, std::placeholders::_1, std::placeholders::_2));
	m_sim_timer.start();
	startTimer(5);
}

SimWorker::~SimWorker()
{
	onStopTransportRequested();
}

void SimWorker::timerEvent(QTimerEvent *event)
{
	static constexpr qint64 kMaxStepsPerTick = 64;

	const qint64 curr_ns = m_sim_timer.nsecsElapsed();

	m_acc_ns += (curr_ns - m_last_ns);

	qint64 steps = m_acc_ns / sim_step_ns;

	if (steps > kMaxStepsPerTick) {
		gLog().warning(QString("sim behind: %1 steps").arg(steps - kMaxStepsPerTick));
		steps = kMaxStepsPerTick;
	}

	m_acc_ns -= steps * sim_step_ns;

	QMutexLocker lock(&m_lock);
	for (int i = 0; i < steps; i++) {
		SimState point = m_sim.step();
		m_points.push_back(point);
	}

	m_last_ns = curr_ns;
}

void SimWorker::onStartTransportRequested(Transport::Config config)
{
	onStopTransportRequested();

	switch(config.type) {
	case Transport::Type::Tcp:
		if (config.port_num > 0) {
			m_transport = new TcpTransport(config.port_num, this);
		}
		break;
	case Transport::Type::Serial:
		if (!config.port_name.isEmpty() && config.baud_rate > 0) {
			m_transport = new SerialTransport(config.port_name, config.baud_rate, this);
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

void SimWorker::onStopTransportRequested()
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

void SimWorker::getSimPoints(std::vector<SimState> &out)
{
	QMutexLocker lock(&m_lock);
	m_points.swap(out);
}
