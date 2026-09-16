// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <QObject>

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
	/** Connect signals and kick off operations. */
	void init();

	void startTransport();

	void stopTransport();

private:
	void onTimerTick();

	FirmwareSim m_sim;
	Transport *m_transport = nullptr;
};
