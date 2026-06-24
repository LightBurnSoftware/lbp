// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "transport.h"

#include <QSerialPort>
#include <QSerialPortInfo>

class SerialTransport : public Transport
{
	Q_OBJECT
public:
	/** Construct, but do not open, a Serial connection on the given port. */
	SerialTransport(const QString &port, int baud, QObject *parent = nullptr);

	/**
	 *  @brief Attempt to open the serial port.
	 *  @return True if the port was successfully opened, false otherwise.
	 */
	bool start() override;

	/** Close the serial port. */
	void stop() override;

	bool sendBytes(const uint8_t *data, int len) override;

private:
	void onBytesReady();

	QSerialPort *m_serial_port;
};
