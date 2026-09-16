// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "mainwindow.h"

#include "log.h"

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
{
	qRegisterMetaType<Transport::Config>();

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

	startTimer(5);
}

MainWindow::~MainWindow()
{

}

QSize MainWindow::sizeHint() const
{
	return QSize(800, 600);
}

void MainWindow::timerEvent(QTimerEvent *event)
{
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
}

void MainWindow::startTransport()
{
	Transport::Config config;

	qDebug() << "start";
}
