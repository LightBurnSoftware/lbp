// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	MainWindow w;

	QObject::connect(&a, &QCoreApplication::aboutToQuit, &w, &MainWindow::onAboutToQuit);
	w.show();
	return a.exec();
}
