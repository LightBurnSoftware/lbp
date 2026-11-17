// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "simview.h"
#include "simworker.h"
#include "transport.h"
#include "transportwidget.h"

#include <QCheckBox>
#include <QLabel>
#include <QMainWindow>
#include <QPushButton>
#include <QRadioButton>
#include <QTextEdit>
#include <QThread>
#include <QWidget>

#include <vector>

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	virtual ~MainWindow();

signals:
	void startTransportRequested(Transport::Config config);
	void stopTransportRequested();
	void faultRequested(bool active);
	void modeRequested(MachineMode mode);

protected:
	void timerEvent(QTimerEvent *event) override;
	QSize sizeHint() const override;
	void keyPressEvent(QKeyEvent *event) override;

private:
	void onStopTransportClicked();
	void onStartTransportClicked();
	void onClearClicked();
	void onGalvoModeToggled(bool galvo);
	void updateStatus(const SimState &state);

	// widgets
	TransportWidget *wTransport = nullptr;
	QTextEdit *wSimConsole = nullptr;
	SimView *wSimView = nullptr;
	QPushButton *pbStart = nullptr;
	QPushButton *pbStop = nullptr;
	QPushButton *pbClearSim = nullptr;
	QCheckBox *cbFault = nullptr;
	QRadioButton *rbGantry = nullptr;
	QRadioButton *rbGalvo = nullptr;
	QLabel *lblStatus = nullptr;

	// members
	SimWorker *m_worker; // harness to operate the firmware simulation
	QThread m_thread; // background worker thread for the sim harness
	std::vector<SimState> m_points; // storage for simulated laser positions
};
