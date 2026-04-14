// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <queue>
#include <QDateTime>
#include <QString>

#include "vec4.h"

/** A simple queue of log entries. Implemented this way to help decouple server, simulator, and ui. */
class Log
{
public:
	enum Level { DEBUG, INFO, WARNING, ERROR };
	struct Entry
	{
		Level level;
		QString msg;
		QDateTime timestamp;
	};

	/** Constructor */
	Log();

	/**
	 * @brief push a new entry onto the log
	 * @param level Rank of message.
	 * @param msg Message to push.
	 */
	void push(Level level, const QString &msg);

	/** @return true if the log has entries, false otherwise. */
	bool hasEntry();

	/** @return the next unprocessed entry off the queue. */
	const Entry pop();

private:
	std::queue<Entry> m_q;
};

/** global log instance. */
Log &gLog();

/** Helper function to assist logging a Vec4 */
QString toQString(const Vec4 &v);
