// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <QElapsedTimer>
#include <QObject>

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

private slots:
	/** Invoked when the transport layer has bytes ready for reading. */
	void onTransportRx(const QByteArray &bytes);

protected:
	void timerEvent(QTimerEvent *event) override;

private:

	FirmwareSim m_sim;
	QElapsedTimer m_sim_timer;
	Transport *m_transport = nullptr;
};
