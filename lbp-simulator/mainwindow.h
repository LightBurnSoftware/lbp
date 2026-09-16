// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "simworker.h"
#include "simview.h"
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

protected:
	void timerEvent(QTimerEvent *event) override;
	QSize sizeHint() const override;
	void keyPressEvent(QKeyEvent *event) override;

private:
	void stopTransport();
	void startTransport();
	void onClearClicked();
	void onTransportRx(const QByteArray &bytes);

	// widgets
	TransportWidget *wTransport = nullptr;
	QTextEdit *wSimConsole = nullptr;
	SimView *wSimView = nullptr;
	QPushButton *pbStart = nullptr;
	QPushButton *pbStop = nullptr;
	QPushButton *pbClearSim = nullptr;

	// members
	SimWorker *m_worker;
	QThread m_bg_thread;
};
