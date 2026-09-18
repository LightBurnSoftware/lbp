// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "log.h"

#include <QMutexLocker>

Log &gLog()
{
	static Log instance;
	return instance;
}

void Log::push(Level level, const QString &msg)
{
	QMutexLocker lock(&m_lock);
	m_q.push({level, msg, QDateTime::currentDateTimeUtc()});
}

const Log::Entry Log::pop()
{
	QMutexLocker lock(&m_lock);	
	Entry entry = std::move(m_q.front());
	m_q.pop();
	return entry;
}

bool Log::hasEntry()
{
	QMutexLocker lock(&m_lock);	
	return !m_q.empty();
}

void Log::info(const QString &msg)
{
	push(Log::INFO, msg);
}

void Log::debug(const QString &msg)
{
	push(Log::DEBUG, msg);
}

void Log::error(const QString &msg)
{
	push(Log::ERROR, msg);
}

void Log::warning(const QString &msg)
{
	push(Log::WARNING, msg);
}

QString toQString(const Vec4 &v)
{
	return QString("(%1, %2, %3, %4)").arg(v.x).arg(v.y).arg(v.z).arg(v.u);
}
