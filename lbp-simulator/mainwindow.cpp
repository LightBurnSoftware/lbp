// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "mainwindow.h"

#include "log.h"

#include <QDebug>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QtGui/qevent.h>

#define MAX_SERVER_CONSOLE_LINES 512
#define MAX_SIM_CONSOLE_LINES 512

static constexpr Log::Level kLogFilter = Log::INFO;

MainWindow::MainWindow(QWidget *parent)
	: QMainWindow(parent)
	, wTransport(new TransportWidget(this))
	, wSimConsole(new QTextEdit(this))
	, wSimView(new SimView(this))
	, pbStart(new QPushButton("Start", this))
	, pbStop(new QPushButton("Stop", this))
	, pbClearSim(new QPushButton("Clear Sim View", this))
	, cbFault(new QCheckBox("Simulate Fault", this))
	, rbGantry(new QRadioButton("Gantry", this))
	, rbGalvo(new QRadioButton("Galvo", this))
	, lblStatus(new QLabel(this))
{
	// Widget setup
	assert(centralWidget() == nullptr);

	rbGantry->setChecked(true);

	connect(pbClearSim, &QPushButton::clicked, this, &MainWindow::onClearClicked);
	connect(pbStop, &QPushButton::clicked, this, &MainWindow::onStopTransportClicked);
	connect(pbStart, &QPushButton::clicked, this, &MainWindow::onStartTransportClicked);
	connect(cbFault, &QCheckBox::toggled, this, &MainWindow::faultRequested);
	connect(rbGalvo, &QRadioButton::toggled, this, &MainWindow::onGalvoModeToggled);

	wSimConsole->setReadOnly(true);
	wSimConsole->document()->setMaximumBlockCount(MAX_SERVER_CONSOLE_LINES);
	wSimConsole->setMinimumWidth(300);
	wSimConsole->setStyleSheet("background-color: black");

	QWidget *centralWidget = new QWidget(this);

	QGroupBox *gbTransport = new QGroupBox("Transport", this);
	QGridLayout *lTransport = new QGridLayout();
	lTransport->addWidget(wTransport, 0, 0, 1, 2);
	lTransport->addWidget(pbStart, 1, 0);
	lTransport->addWidget(pbStop, 1, 1);
	gbTransport->setLayout(lTransport);

	QGroupBox *gbDevice = new QGroupBox("Device", this);
	QGridLayout *lDevice = new QGridLayout();
	lDevice->addWidget(rbGalvo, 0, 1);
	lDevice->addWidget(rbGantry, 0, 0);
	lDevice->addWidget(pbClearSim, 1, 1);
	lDevice->addWidget(cbFault, 1, 0);
	gbDevice->setLayout(lDevice);

	QVBoxLayout *controls = new QVBoxLayout();
	controls->addWidget(lblStatus);
	controls->addStretch();
	controls->addWidget(gbDevice);
	controls->addWidget(gbTransport);

	QGridLayout *grid = new QGridLayout(centralWidget);
	grid->addWidget(wSimConsole, 0, 0, 2, 1);
	grid->addWidget(wSimView, 0, 1);
	grid->addLayout(controls, 1, 1);
	grid->setColumnStretch(0, 1);
	grid->setColumnStretch(1, 1);
	setCentralWidget(centralWidget);

	// Logic setup
	qRegisterMetaType<Transport::Config>();
	qRegisterMetaType<MachineMode>();

	m_worker = new SimWorker;
	m_worker->moveToThread(&m_thread);

	connect(this, &MainWindow::startTransportRequested, m_worker, &SimWorker::onStartTransportRequested);
	connect(this, &MainWindow::stopTransportRequested, m_worker, &SimWorker::onStopTransportRequested);
	connect(this, &MainWindow::faultRequested, m_worker, &SimWorker::onFaultRequested);
	connect(this, &MainWindow::modeRequested, m_worker, &SimWorker::onModeRequested);
	connect(&m_thread, &QThread::started, m_worker, &SimWorker::init);
	connect(&m_thread, &QThread::finished, m_worker, &SimWorker::deleteLater);
	m_thread.start();

	startTimer(20);
}

MainWindow::~MainWindow()
{
	m_thread.quit();
	m_thread.wait();
}

QSize MainWindow::sizeHint() const
{
	return QSize(800, 600);
}

void MainWindow::timerEvent(QTimerEvent *event)
{
	m_points.clear();
	m_worker->getSimPoints(m_points);
	for (const SimState &point : m_points) {
		wSimView->heat(point);
	}
	if (!m_points.empty()) {
		updateStatus(m_points.back());
	}

	wSimView->tick();
	while (gLog().hasEntry()) {
		Log::Entry entry = gLog().pop();
		if (entry.level < kLogFilter) {
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
		onStartTransportClicked();
		break;
	case Qt::Key_D:
		onStopTransportClicked();
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

void MainWindow::onGalvoModeToggled(bool galvo)
{
	emit modeRequested(galvo ? MachineMode::Galvo : MachineMode::Gantry);
	wSimView->clear();
	update();
}

void MainWindow::updateStatus(const SimState &state)
{
	if (state.mode == MachineMode::Galvo) {
		const double field_x_mm = double(state.max_pos.x - state.min_pos.x) / 1000.0;
		const double field_y_mm = double(state.max_pos.y - state.min_pos.y) / 1000.0;
		lblStatus->setText(QString("Galvo: %1 x %2 mm field, correction %3, guide light %4")
							   .arg(field_x_mm)
							   .arg(field_y_mm)
							   .arg(state.correction_on ? "on" : "off")
							   .arg(state.guide_light ? "on" : "off"));
	} else {
		lblStatus->setText("Gantry");
	}
}

void MainWindow::onStopTransportClicked()
{
	emit stopTransportRequested();
}

void MainWindow::onStartTransportClicked()
{
	emit startTransportRequested(wTransport->config());
}
