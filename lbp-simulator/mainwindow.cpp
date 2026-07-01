// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "mainwindow.h"

#include "log.h"
#include "serialtransport.h"
#include "tcptransport.h"

#include <QDebug>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QtGui/qevent.h>

#define MAX_SERVER_CONSOLE_LINES 512
#define MAX_SIM_CONSOLE_LINES 512

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, wTransport(new TransportWidget(this))
	, wSimConsole(new QTextEdit(this))
	, wSimView(new SimView(this))
	, pbStart(new QPushButton("Start", this))
	, pbStop(new QPushButton("Stop", this))
	, pbClearSim(new QPushButton("Clear Sim View", this))
	, m_sim()
	, m_sim_timer()
{
	assert(centralWidget() == nullptr);

	connect(pbClearSim, &QPushButton::clicked, this, &MainWindow::onClearClicked);
	connect(pbStop, &QPushButton::clicked, this, &MainWindow::stopTransport);
	connect(pbStart, &QPushButton::clicked, this, &MainWindow::startTransport);

	wSimConsole->setReadOnly(true);
	wSimConsole->document()->setMaximumBlockCount(MAX_SERVER_CONSOLE_LINES);
	wSimConsole->setMinimumWidth(300);
	wSimConsole->setStyleSheet("background-color: black");

	QWidget *centralWidget = new QWidget(this);

	QHBoxLayout *buttons = new QHBoxLayout();
	buttons->addWidget(pbClearSim);
	buttons->addWidget(pbStart);
	buttons->addWidget(pbStop);

	QVBoxLayout *controls = new QVBoxLayout();
	controls->addLayout(buttons);
	controls->addWidget(wTransport);
	controls->addStretch();

	QGridLayout *grid = new QGridLayout(centralWidget);
	grid->addWidget(wSimConsole, 0, 0, 2, 1);
	grid->addWidget(wSimView, 0, 1);
	grid->addLayout(controls, 1, 1);
	grid->setColumnStretch(0, 1);
	grid->setColumnStretch(1, 1);
	setCentralWidget(centralWidget);

	m_sim_timer.start();
	startTimer(10);
}

MainWindow::~MainWindow()
{
	if (m_transport) {
		m_transport->stop();
	}
}

QSize MainWindow::sizeHint() const
{
	return QSize(800, 600);
}

void MainWindow::timerEvent(QTimerEvent *event)
{
	int elapsed = m_sim_timer.restart();
	SimState state = m_sim.loop(elapsed);
	wSimView->tick(state);

	while (gLog().hasEntry()) {
		Log::Entry entry = gLog().pop();
		if (entry.level < Log::INFO) {
			continue;
		}
		switch (entry.level) {
		case Log::ERROR:
			wSimConsole->setTextColor(QColor::fromRgb(255, 0, 0));
			break;
		case Log::WARNING:
			wSimConsole->setTextColor(QColor::fromRgb(255, 255, 0));
			break;
		case Log::INFO:
			wSimConsole->setTextColor(QColor::fromRgb(255, 255, 255));
			break;
		case Log::DEBUG:
		default:
			wSimConsole->setTextColor(QColor::fromRgb(0, 255, 0));
			break;
		}
		wSimConsole->append(QString("%1 : %2").arg(entry.timestamp.time().toString(), entry.msg));
	}
}

void MainWindow::keyPressEvent(QKeyEvent *event)
{
	switch (event->key()) {
	case Qt::Key_S:
		startTransport();
		break;
	case Qt::Key_D:
		stopTransport();
		break;
	case Qt::Key_C:
		onClearClicked();
		break;
	default:
		break;
	}
}

void MainWindow::onClearClicked()
{
	wSimView->clear();
	update();
}

void MainWindow::stopTransport()
{
	qDebug() << "stop";
	m_sim.setTransport(nullptr);
	if (m_transport) {
		m_transport->stop();
		m_transport->deleteLater();
		m_transport = nullptr;
	}
}

void MainWindow::startTransport()
{
	if (m_transport) {
		m_transport->stop();
		disconnect(m_transport, &Transport::rxBytes, this, &MainWindow::onTransportRx);
		m_transport->deleteLater();
	}
	switch(wTransport->type()) {
	case Transport::Type::Tcp: {
		bool ok = false;
		int port = wTransport->port().toInt(&ok);
		if (ok) {
			m_transport = new TcpTransport(port, this);
		}
		else {
			m_transport = nullptr;
		}
	} break;
	case Transport::Type::Serial: {
		QString port = wTransport->port();
		int baud = wTransport->baudRate();
		if (!port.isEmpty() && baud > 0) {
			m_transport = new SerialTransport(port, baud, this);
		}
		else {
			m_transport = nullptr;
		}
	} break;
	default:
		m_transport = nullptr;
	}
	if (m_transport) {
		connect(m_transport, &Transport::rxBytes, this, &MainWindow::onTransportRx);
		m_transport->start();
	}
	m_sim.setTransport(m_transport);
	qDebug() << "start";
}

void MainWindow::onTransportRx(const QByteArray &bytes)
{
	m_sim.rxCallback((const uint8_t *) bytes.constData(), bytes.size());
}

void MainWindow::onAboutToQuit()
{
	if (m_transport) {
		m_transport->stop();
	}
}
