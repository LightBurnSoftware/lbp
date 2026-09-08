// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "tcptransport.h"

#include "log.h"

#include <QElapsedTimer>

TcpTransport::TcpTransport(int port, QObject *parent)
	: Transport(parent)
	, m_port(port)
	, m_server(new QTcpServer(this))
{
	// Empty
}

bool TcpTransport::start()
{
	if (m_server->isListening()) {
		gLog().push(Log::WARNING, "Server already started");
		return false;
	}
	m_server->setMaxPendingConnections(1); //only single connection
	if (m_server->listen(QHostAddress::Any, m_port)) {
		connect(m_server, &QTcpServer::newConnection, this, &TcpTransport::onNewConnection);
		gLog().push(Log::INFO, "Started TCP Server on port " + QString::number(m_port));
		return true;
	}

	gLog().push(Log::ERROR, "Unable to start the server: " + m_server->errorString());
	return false;
}

void TcpTransport::stop()
{
	if (m_socket != nullptr) {
		if (m_socket->state() != QAbstractSocket::UnconnectedState) {
			disconnect(m_socket, &QTcpSocket::disconnected, this, &TcpTransport::discardSocket);
			m_socket->disconnectFromHost();
			if (m_socket->state() != QAbstractSocket::UnconnectedState) {
				m_socket->waitForDisconnected(2500);
			}
			m_socket->deleteLater();
		}
		m_socket = nullptr;
		gLog().push(Log::INFO, "Socket disconnected...");
	}
	if (m_server->isListening()) {
		m_server->close();
	}
	gLog().push(Log::INFO, "Server stopped.");
}

bool TcpTransport::sendBytes(const uint8_t *bytes, int len)
{
	QByteArray qData = QByteArray((const char *) bytes, len);

	if (m_socket != nullptr && m_socket->isOpen()) {
		m_socket->write(qData);
		m_socket->flush();

		return true;
	}

	return false;
}

void TcpTransport::onNewConnection()
{
	while (m_server->hasPendingConnections()) {
		QTcpSocket *pNewSocket = m_server->nextPendingConnection();
		if (m_socket != nullptr) {
			pNewSocket->close(); //reject the connection
			pNewSocket->deleteLater();
		} else {
			m_socket = pNewSocket;
			// m_socket->setSocketOption(QAbstractSocket::LowDelayOption, 1);
			connect(m_socket, &QTcpSocket::readyRead, this, &TcpTransport::readSocket);
			connect(m_socket, &QTcpSocket::disconnected, this, &TcpTransport::discardSocket);
			gLog().push(Log::INFO, QString("New connection: %1").arg(pNewSocket->peerPort()));
		}
	}
}

void TcpTransport::readSocket()
{
	while (m_socket->bytesAvailable() > 0) {
		QByteArray bytes = m_socket->readAll();
		emit rxBytes(bytes);
	}
}

void TcpTransport::discardSocket()
{
	QTcpSocket* socket = reinterpret_cast<QTcpSocket*>(sender());
	socket->deleteLater();
	if (socket == m_socket) {
		m_socket = nullptr;
	}
	gLog().push(Log::INFO, "TCP Socket disconnected...");
}

