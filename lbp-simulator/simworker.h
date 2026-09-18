// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <QElapsedTimer>
#include <QObject>
#include <QMutex>

#include <vector>

#include "firmwaresim.h"
#include "transport.h"

/**
 * A Qt Worker object intended to host the FirmwareSim.
 * It will own the transport layer and invoke the FirmwareSim's rx, tx, and simulator loop functions.
 */
class SimWorker : public QObject
{
	Q_OBJECT
public:
	SimWorker();
	virtual ~SimWorker();
public slots:
	/** Connect signals and kick off operations. */
	void init();

	/** Start a transport layer with the requested configuration. */
	void startTransport(Transport::Config config);

	/** Stop the current transport. */
	void stopTransport();

	/** Invoked by firmware sim, when bytes are ready to be sent to the transport layer. */
	void onTransportTx(const uint8_t *bytes, size_t len);
	
	/** Get the list of simulated points since the last call. */
	void getSimPoints(std::vector<SimState> &out);

private slots:
	/** Invoked when the transport layer has bytes ready for reading. */
	void onTransportRx(const QByteArray &bytes);

protected:
	void timerEvent(QTimerEvent *event) override;

private:
	FirmwareSim m_sim;
	Transport *m_transport = nullptr;
	std::vector<SimState> m_points;
	QMutex m_lock;
	QElapsedTimer m_sim_timer;
	qint64 m_last_ns = 0;
	qint64 m_acc_ns = 0;
};
