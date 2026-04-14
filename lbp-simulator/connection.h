// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <lbp/parser.h>
#include <QByteArray>
#include <QMutex>
#include <QMutexLocker>
#include <QObject>
#include <QString>
#include <QTcpServer>
#include <queue>

#include "simutils.h"

/**
 * @brief A TCP connection. Handles sending and receiving LBP messages to and from LightBurn.
 *
 * This class is a simple wrapper around Qt's TCP server and socket implementation.
 * It feeds incoming bytestreams into its own lbp::Parser. The Connection class does not parse these bytes itself,
 * but rather makes this parser available so others may parse messages at their leisure.
 *
 * Status updates are stored on a queue to be read at the owner's leisure.
 */
class Connection : public QObject
{
	Q_OBJECT
public:
	/** A very basic event structure for the connection. */
	struct Status
	{
		enum Code { Disconnected, Connected, Error } code;
		std::string msg;
	};

	/** Construct, but do not start, a TCP connection on the given port. */
	Connection(int port, QObject *parent = nullptr);

	/** Destructor: Responsibly stop and clean up the connection. */
	virtual ~Connection();

	/**
	 *  @brief Start listening for new connections (only permit one at a time.)
	 *  @return True if the server was successfully started, false otherwise.
	 */
	bool start();

	/** @brief Stop and destroy any active socket and close the server. */
	void stop();

	/** @return True if there unread status updates, false otherwise. */
	bool hasStatusUpdate() const;

	/** @return the next unread status update, in FIFO order. */
	Connection::Status getStatusUpdate();

	/**
	 * @brief Send raw data through the TCP socket.
	 * @param data The data to be sent.
	 * @param len The number of bytes to send.
	 * @return True on success, false on failure.
	 */
	bool sendBytes(const uint8_t *data, int len);

	/** @return the internal lbp message parser. */
	WireParser &parser();

private slots:
	void onNewConnection();
	void readSocket();
	void discardSocket();

private:
	void pushStatus(Connection::Status::Code code, const QString &msg);

	int m_port;
	QTcpServer *m_server;
	QTcpSocket *m_socket;
	std::queue<Connection::Status> m_status_q;
	WireParser m_parser;
};
