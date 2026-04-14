// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "connection.h"

#include <QDebug>
#include <QElapsedTimer>
#include <QTcpSocket>

Connection::Connection(int port, QObject *parent)
	: QObject(parent)
	, m_port(port)
	, m_server(new QTcpServer(this))
	, m_socket(nullptr)
	, m_status_q()
{
	pushStatus(Connection::Status::Disconnected, "No connection");
}

Connection::~Connection()
{
	stop();
}

void Connection::pushStatus(Connection::Status::Code code, const QString &msg)
{
	m_status_q.push({code, msg.toStdString()});
}

bool Connection::start()
{
	if (m_server->isListening()) {
		pushStatus(Connection::Status::Error, "Server already started.");
		return false;
	}
	m_server->setMaxPendingConnections(1); //only single connection
	if (m_server->listen(QHostAddress::Any, m_port)) {
		connect(m_server, &QTcpServer::newConnection, this, &Connection::onNewConnection);
		return true;
	}

	pushStatus(Connection::Status::Error, "Unable to start the server: " + m_server->errorString());
	return false;
}

void Connection::stop()
{
	if (m_socket != nullptr) {
		if (m_socket->state() != QAbstractSocket::UnconnectedState) {
			disconnect(m_socket, &QTcpSocket::disconnected, this, &Connection::discardSocket);
			m_socket->disconnectFromHost();
			if (m_socket->state() != QAbstractSocket::UnconnectedState) {
				m_socket->waitForDisconnected(2500);
			}
			m_socket->deleteLater();
		}
		m_socket = nullptr;
		pushStatus(Connection::Status::Disconnected, "Socket disconnected...");
	}
	if (m_server->isListening()) {
		m_server->close();
	}
	pushStatus(Connection::Status::Disconnected, "Server stopped.");
}

bool Connection::hasStatusUpdate() const
{
	return !m_status_q.empty();
}

Connection::Status Connection::getStatusUpdate()
{
	assert(!m_status_q.empty());
	Connection::Status status = std::move(m_status_q.front());
	m_status_q.pop();
	return status;
}

bool Connection::sendBytes(const uint8_t *bytes, int len)
{
	QByteArray qData = QByteArray((const char *) bytes, len);
	// qDebug() << "sending" << qData.toHex(' ');

	if (m_socket != nullptr && m_socket->isOpen()) {
		m_socket->write(qData);
		m_socket->flush();

		return true;
	}

	return false;
}

void Connection::onNewConnection()
{
	while (m_server->hasPendingConnections()) {
		QTcpSocket *pNewSocket = m_server->nextPendingConnection();
		if (m_socket != nullptr) {
			pNewSocket->close(); //reject the connection
			pNewSocket->deleteLater();
		} else {
			m_socket = pNewSocket;
			connect(m_socket, &QTcpSocket::readyRead, this, &Connection::readSocket);
			connect(m_socket, &QTcpSocket::disconnected, this, &Connection::discardSocket);
			pushStatus(Connection::Status::Connected,
					   QString("New connection: %1").arg(pNewSocket->peerPort()));
		}
	}
}

void Connection::readSocket()
{
	while (m_socket->bytesAvailable() > 0) {
		QByteArray bytes = m_socket->readAll();
		m_parser.feed((const uint8_t *) bytes.constData(), bytes.size());
	}
}

void Connection::discardSocket()
{
	QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());
	socket->deleteLater();
	if (socket == m_socket) {
		m_socket = nullptr;
	}
	pushStatus(Connection::Status::Disconnected, "Disconnected...");
}

WireParser &Connection::parser()
{
	return m_parser;
}
