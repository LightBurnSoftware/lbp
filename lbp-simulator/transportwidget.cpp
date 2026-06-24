// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "transportwidget.h"

#include <QDebug>

#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QSerialPortInfo>
#include <QSerialPort>

#define DEFAULT_TCP_PORT 6666

TransportWidget::TransportWidget(QWidget *parent)
	: QWidget(parent)
	, rbNone(new QRadioButton("None", this))
	, rbSerial(new QRadioButton("Serial", this))
	, rbTcp(new QRadioButton("TCP", this))
	, lblSerialPort(new QLabel("Serial Port:", this))
	, lblTcpPort(new QLabel("TCP Port:", this))
	, lblBaudRate(new QLabel("Baud Rate:", this))
	, cmbSerialPort(new QComboBox(this))
	, cmbBaudRate(new QComboBox(this))
	, txtTcpPort(new QLineEdit(this))
{

	txtTcpPort->setValidator(new QIntValidator(0, 9999, this));
	txtTcpPort->setText(QString::number(DEFAULT_TCP_PORT));

	connect(rbTcp, &QRadioButton::toggled, this, &TransportWidget::onTcpToggled);
	connect(rbSerial, &QRadioButton::toggled, this, &TransportWidget::onSerialToggled);
	QGridLayout *grid = new QGridLayout(this);

	QHBoxLayout *radiolayout = new QHBoxLayout();
	radiolayout->addWidget(rbNone);
	radiolayout->addWidget(rbTcp);
	radiolayout->addWidget(rbSerial);

	cmbBaudRate->addItem(QString::number(QSerialPort::Baud9600), QSerialPort::Baud9600);
	cmbBaudRate->addItem(QString::number(QSerialPort::Baud115200), QSerialPort::Baud115200);

	grid->setColumnStretch(0, 1);
	grid->setColumnStretch(1, 1);
	grid->addWidget(new QLabel("Transport Type:"), 0, 0);
	grid->addLayout(radiolayout, 0, 1);
	grid->addWidget(lblTcpPort, 1, 0);
	grid->addWidget(txtTcpPort, 1, 1);
	grid->addWidget(lblSerialPort, 2, 0);
	grid->addWidget(cmbSerialPort, 2, 1);
	grid->addWidget(lblBaudRate, 3, 0);
	grid->addWidget(cmbBaudRate, 3, 1);

	rbNone->setChecked(true);
	updateView();
}

Transport::Type TransportWidget::type() const
{
	if (rbTcp->isChecked()) {
		return Transport::Type::Tcp;
	}
	else if (rbSerial->isChecked()) {
		return Transport::Type::Serial;
	}
	return Transport::Type::None;
}

void TransportWidget::onTcpToggled(bool checked)
{
	updateView();
}

void TransportWidget::onSerialToggled(bool checked)
{
	if (checked) {
		cmbSerialPort->clear();
		for (const QSerialPortInfo &info : QSerialPortInfo::availablePorts()) {
			cmbSerialPort->addItem(info.portName());
		}

	}
	updateView();
}

void TransportWidget::updateView()
{
	Transport::Type ttype = type();
	txtTcpPort->setVisible(ttype == Transport::Type::Tcp);
	lblTcpPort->setVisible(ttype == Transport::Type::Tcp);
	cmbSerialPort->setVisible(ttype == Transport::Type::Serial);
	lblSerialPort->setVisible(ttype == Transport::Type::Serial);
	cmbBaudRate->setVisible(ttype == Transport::Type::Serial);
	lblBaudRate->setVisible(ttype == Transport::Type::Serial);
}

QString TransportWidget::port() const
{
	switch (type()) {
	case Transport::Type::Tcp:
		return txtTcpPort->text();
	case Transport::Type::Serial:
		return cmbSerialPort->currentText();
	default:
		return "";
	}
}

int TransportWidget::baudRate() const
{
	bool ok = false;
	int baud = cmbBaudRate->currentData().toInt(&ok);
	if (ok) {
		return baud;
	}
	return 0;
}
