// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "mainwindow.h"

#include "log.h"

#include <QDebug>
#include <QGridLayout>
#include <QtGui/qevent.h>

#define PORT 6666
#define MAX_SERVER_CONSOLE_LINES 512
#define MAX_SIM_CONSOLE_LINES 512

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, m_server_console(new QTextEdit(this))
	, m_sim_console(new QTextEdit(this))
	, m_sim_view(new SimView(this))
	, m_connection(new Connection(PORT, this))
	, m_sim(*m_connection)
	, m_sim_timer()
{
	assert(centralWidget() == nullptr);

	m_server_console->setReadOnly(true);
	m_server_console->document()->setMaximumBlockCount(MAX_SERVER_CONSOLE_LINES);
	m_server_console->setMinimumWidth(400);

	m_sim_console->setReadOnly(true);
	m_sim_console->document()->setMaximumBlockCount(MAX_SERVER_CONSOLE_LINES);
	m_sim_console->setMinimumWidth(400);

	QWidget *centralWidget = new QWidget(this);
	QGridLayout *layout = new QGridLayout(centralWidget);
	layout->addWidget(m_sim_console, 0, 0, 2, 1);
	layout->addWidget(m_sim_view, 0, 1);
	layout->addWidget(m_server_console, 1, 1);
	setCentralWidget(centralWidget);

	m_connection->start();
	m_sim_timer.start();
	startTimer(10);
}

MainWindow::~MainWindow()
{
	// _connection->deleteLater();
}

QSize MainWindow::sizeHint() const
{
	return QSize(800, 600);
}

void MainWindow::timerEvent(QTimerEvent *event)
{
	while (m_connection->hasStatusUpdate()) {
		Connection::Status status = m_connection->getStatusUpdate();
		switch(status.code) {
		case Connection::Status::Connected:
			m_server_console->setTextColor(QColor::fromRgb(0, 255, 0));
			break;
		case Connection::Status::Disconnected:
			m_server_console->setTextColor(QColor::fromRgb(255, 255, 0));
			break;
		case Connection::Status::Error:
			m_server_console->setTextColor(QColor::fromRgb(255, 0, 0));
			break;
		default:
			m_server_console->setTextColor(QColor::fromRgb(255, 255, 255));
			break;
		}
		m_server_console->append(QString::fromStdString(status.msg));
	}

	int elapsed = m_sim_timer.restart();
	SimState state = m_sim.loop(elapsed);
	m_sim_view->tick(state);

	while (gLog().hasEntry()) {
		Log::Entry entry = gLog().pop();
		if (entry.level < Log::INFO) {
			continue;
		}
		switch (entry.level) {
		case Log::ERROR:
			m_sim_console->setTextColor(QColor::fromRgb(255, 0, 0));
			break;
		case Log::WARNING:
			m_sim_console->setTextColor(QColor::fromRgb(255, 255, 0));
			break;
		case Log::INFO:
			m_sim_console->setTextColor(QColor::fromRgb(255, 255, 255));
			break;
		case Log::DEBUG:
		default:
			m_sim_console->setTextColor(QColor::fromRgb(0, 255, 0));
			break;
		}
		m_sim_console->append(QString("%1 : %2").arg(entry.timestamp.time().toString(), entry.msg));
	}
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_S:
		m_connection->start();
		break;
	case Qt::Key_D:
		m_connection->stop();
		break;
	case Qt::Key_C:
		m_sim_view->clear();
		update();
		break;
	default:
		break;
	}
}

void MainWindow::onAboutToQuit()
{
	m_connection->stop();
}
