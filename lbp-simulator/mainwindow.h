// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "simview.h"
#include "simworker.h"
#include "transport.h"
#include "transportwidget.h"

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <QThread>
#include <QWidget>

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	virtual ~MainWindow();

signals:
	void startTransportRequested(Transport::Config config);

protected:
	void timerEvent(QTimerEvent *event) override;
	QSize sizeHint() const override;
	void keyPressEvent(QKeyEvent *event) override;

private:
	void stopTransport();
	void startTransport();
	void onClearClicked();

	// widgets
	TransportWidget *wTransport = nullptr;
	QTextEdit *wSimConsole = nullptr;
	SimView *wSimView = nullptr;
	QPushButton *pbStart = nullptr;
	QPushButton *pbStop = nullptr;
	QPushButton *pbClearSim = nullptr;

	// members
	SimWorker *m_worker;
	QThread m_thread;
};
