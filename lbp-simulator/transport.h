// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once
#include <QObject>

/** @brief Base class and interface for sending and receiving LBP messages from LightBurn. */
class Transport : public QObject
{
	Q_OBJECT
public:
    enum class Type { None, Tcp, Serial };

	/** Constructor */
	Transport(QObject *parent = nullptr) : QObject(parent) {}

	/** Destructor: Responsibly stop and clean up the connection. */
	virtual ~Transport() = default;

	/**
	 * @brief Send raw data through the transport medium.
	 * @param data The data to be sent.
	 * @param len The number of bytes to send.
	 * @return True on success, false on failure.
	 */
	virtual bool sendBytes(const uint8_t *data, int len) = 0;

	/**
	 *  @brief Activate the connection. Start sending and receiving bytes.
	 *  @return True if the server was successfully started, false otherwise.
	 */
	virtual bool start() = 0;

	/** @brief Deactivate the connection. Stop sending and receiving bytes. */
	virtual void stop() = 0;
signals:
	/** @brief triggered when the transport has bytes ready to be read. */
	void rxBytes(const QByteArray &bytes);
};
