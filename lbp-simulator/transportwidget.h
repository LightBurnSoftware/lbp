// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include <QComboBox>
#include <QRadioButton>
#include <QLabel>
#include <QLineEdit>
#include <QWidget>

#include "transport.h"

class TransportWidget : public QWidget
{
	Q_OBJECT
public:
	explicit TransportWidget(QWidget *parent = nullptr);

	/** @return the selected type of transport. */
	Transport::Type type() const;

	/** @return the port used for the transport. */
	QString port() const;

	int baudRate() const;

private:
	void onTcpToggled(bool checked);
	void onSerialToggled(bool checked);

	/** Update which controls are enabled based on user selections. */
	void updateView();

	QRadioButton *rbNone;
	QRadioButton *rbSerial;
	QRadioButton *rbTcp;
	QLabel *lblSerialPort;
	QLabel *lblTcpPort;
	QLabel *lblBaudRate;
	QComboBox *cmbSerialPort;
	QComboBox *cmbBaudRate;
	QLineEdit *txtTcpPort;
};
