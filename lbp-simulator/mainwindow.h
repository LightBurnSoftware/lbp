// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "firmwaresim.h"
#include "simview.h"
#include "transport.h"
#include "transportwidget.h"

#include <QMainWindow>
#include <QPushButton>
#include <QTextEdit>
#include <QWidget>

class MainWindow : public QMainWindow
{
	Q_OBJECT

public:
	MainWindow(QWidget *parent = nullptr);
	virtual ~MainWindow();

	void onAboutToQuit();

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
	Transport *m_transport = nullptr;
	FirmwareSim m_sim;
	QElapsedTimer m_sim_timer;
};
