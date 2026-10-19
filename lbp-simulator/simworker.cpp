// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC


#include "simworker.h"
#include "transport.h"
#include "tcptransport.h"
#include "serialtransport.h"

#include <QThread>

#include <algorithm>
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
	Q_UNUSED(event);

	const qint64 curr_ns = m_sim_timer.nsecsElapsed();
	qint64 dt_ns = curr_ns - m_last_ns;
	m_last_ns = curr_ns;

	// After a sleep, App Nap throttling, or a debugger pause, don't try to replay
	// the gap: advance at most sim_max_tick_ns and drop the rest.
	if (dt_ns > sim_max_tick_ns) {
		gLog().debug(QString("sim tick gap %1 ms; skipping ahead").arg(dt_ns / 1'000'000));
		dt_ns = sim_max_tick_ns;
	}

	QMutexLocker lock(&m_lock);
	while (dt_ns > 0) {
		const qint64 sub_ns = std::min(dt_ns, qint64(sim_step_ns));
		m_points.push_back(m_sim.step(sub_ns));
		dt_ns -= sub_ns;
	}
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

void SimWorker::onFaultRequested(bool active)
{
	m_sim.setFault(active);
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
