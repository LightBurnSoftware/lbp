// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "connection.h"
#include "firmwaresim.h"
#include "simview.h"

#include <QMainWindow>
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
	void _onBytesAvailable();

	QTextEdit *m_server_console;
	QTextEdit *m_sim_console;
	SimView *m_sim_view;
	Connection *m_connection;
	FirmwareSim m_sim;
	QElapsedTimer m_sim_timer;
};
