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
	SerialTransport(const QString &port, int baud, QObject *parent = nullptr);

	bool start() override;

	void stop() override;

	bool sendBytes(const uint8_t *data, int len) override;

private:
	void onBytesReady();

	QSerialPort *m_serial_port;

};
