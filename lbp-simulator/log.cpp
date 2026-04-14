// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "log.h"

Log &gLog()
{
	static Log instance;
	return instance;
}

Log::Log()
	: m_q()
{
	// Empty
}

void Log::push(Level level, const QString &msg)
{
	m_q.push({level, msg, QDateTime::currentDateTimeUtc()});
}

const Log::Entry Log::pop()
{
	Entry entry = std::move(m_q.front());
	m_q.pop();
	return entry;
}

bool Log::hasEntry()
{
	return !m_q.empty();
}

QString toQString(const Vec4 &v)
{
	return QString("(%1, %2, %3, %4)").arg(v.x).arg(v.y).arg(v.z).arg(v.u);
}
