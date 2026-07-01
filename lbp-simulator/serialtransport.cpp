// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "serialtransport.h"
#include "log.h"

SerialTransport::SerialTransport(const QString &port, int baud, QObject *parent)
	: Transport(parent)
	, m_serial_port(new QSerialPort(port, this))
{
	m_serial_port->setBaudRate(baud);
	m_serial_port->setFlowControl(QSerialPort::NoFlowControl);
	m_serial_port->setDataBits(QSerialPort::Data8);
	m_serial_port->setParity(QSerialPort::NoParity);
	m_serial_port->setStopBits(QSerialPort::OneStop);

	connect(m_serial_port, &QSerialPort::readyRead, this, &SerialTransport::onBytesReady);
}

bool SerialTransport::start()
{
	if (!m_serial_port->open(QIODeviceBase::ReadWrite)) {
		gLog().push(Log::ERROR, "Failed to open Serial Port " + m_serial_port->portName());
		return false;
	}
	m_serial_port->setDataTerminalReady(false);
	m_serial_port->setRequestToSend(true);
	gLog().push(Log::INFO, "Opened Serial Port " + m_serial_port->portName() + " : " + QString::number(m_serial_port->baudRate()));
	return true;
}

void SerialTransport::stop()
{
	m_serial_port->close();
	gLog().push(Log::INFO, "Closed Serial Port " + m_serial_port->portName());
}

void SerialTransport::onBytesReady()
{
	while (m_serial_port->bytesAvailable() > 0) {
		QByteArray bytes = m_serial_port->readAll();
		emit rxBytes(bytes);
	}
}


bool SerialTransport::sendBytes(const uint8_t *data, int len)
{
	qint64 sent = m_serial_port->write((const char *) data, len);
	if (sent != len) {
		gLog().push(Log::WARNING, "Sent " + QString::number(sent) + " of " + QString::number(len) + " bytes.");
		return false;
	}
	else {
		gLog().push(Log::DEBUG, "Sent " + QString::number(sent) + " bytes.");
		return true;
	}
}

