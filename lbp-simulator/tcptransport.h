// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "transport.h"

#include <QTcpServer>
#include <QTcpSocket>


/** @brief TCP transport implementation: a simple wrapper around Qt's TCP server and socket implementation. */
class TcpTransport : public Transport
{
	Q_OBJECT
public:
	/** Construct, but do not start, a TCP connection on the given port. */
	TcpTransport(int port, QObject *parent = nullptr);

	/**
	 *  @brief Start listening for new connections (only permit one at a time.)
	 *  @return True if the server was successfully started, false otherwise.
	 */
	bool start() override;

	/** @brief Stop and destroy any active socket and close the server. */
	void stop() override;

	bool sendBytes(const uint8_t *data, int len) override;

private:
	void onNewConnection();
	void readSocket();
	void discardSocket();

	int m_port = 0;
	QTcpServer *m_server = nullptr;
	QTcpSocket *m_socket = nullptr;
};
