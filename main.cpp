/***************************************************************************
**                                                                        **
**  Phase Noise Analyser                                                  **
**  Copyright (C) 2025 Benjamin VERNOUX                                   **
**                                                                        **
**  This program is free software: you can redistribute it and/or modify  **
**  it under the terms of the GNU General Public License as published by  **
**  the Free Software Foundation, either version 3 of the License, or     **
**  (at your option) any later version.                                   **
**                                                                        **
**  This program is distributed in the hope that it will be useful,       **
**  but WITHOUT ANY WARRANTY; without even the implied warranty of        **
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         **
**  GNU General Public License for more details.                          **
**                                                                        **
**  You should have received a copy of the GNU General Public License     **
**  along with this program.  If not, see http://www.gnu.org/licenses/.   **
**                                                                        **
****************************************************************************
**           Author: Benjamin VERNOUX                                     **
**          Contact: https://github.com/bvernoux                          **
**             Date: 12 Apr 2025                                          **
**          Version: 1.0.0                                                **
****************************************************************************/

#include "phasenoiseanalyzerapp.h"
#include "constants.h"
#include "version.h"

#include <QApplication>
#include <QCommandLineParser>
#include <QCommandLineOption>
#include <QFileInfo>
#include <QDebug>
#include <QStyleFactory>

int main(int argc, char *argv[])
{
	QApplication app(argc, argv);
	QApplication::setApplicationName(VER_FILEDESCRIPTION_STR);
	QApplication::setApplicationVersion(VER_FILEVERSION_STR);
	QApplication::setOrganizationName(VER_LEGALCOPYRIGHT_STR);

	// Load the icon from the resource system
	QIcon appIcon(":/images/pna.svg");
	// Optionally, also set the application-wide icon
	app.setWindowIcon(appIcon);

	// Command line parsing
	QCommandLineParser parser;
	parser.setApplicationDescription("Phase Noise Analyzer Application (C++/Qt Version)");
	parser.addHelpOption();
	parser.addVersionOption();

	// Define options
	QCommandLineOption inputFileOption(QStringList() << "i" << "input", "Path to input CSV file(s). Can be specified multiple times.", "csv_filename");
	parser.addOption(inputFileOption);

	QCommandLineOption noplotRefenceOption("noplotref", "Do not plot reference.");
	parser.addOption(noplotRefenceOption);

	QCommandLineOption darkThemeOption("dark-theme", "Use dark theme.");
	parser.addOption(darkThemeOption);

	QCommandLineOption dpiOption("dpi", "DPI for output image", "dpi", QString::number(Constants::DEFAULT_DPI));
	parser.addOption(dpiOption);

	// Process arguments
	parser.process(app);

	// Get argument values
	QStringList csvFilenames = parser.values(inputFileOption); // Get multiple input files
	bool noplotRefence = !parser.isSet(noplotRefenceOption);
	bool useDarkTheme = parser.isSet(darkThemeOption);
	bool dpiOk = false;
	int dpi = parser.value(dpiOption).toInt(&dpiOk);
	if (!dpiOk || dpi <= 0) {
		dpi = Constants::DEFAULT_DPI;
		qWarning() << "Invalid DPI value provided, using default:" << dpi;
	}

	// Set Fusion style for consistent look, especially needed for dark theme palettes
	app.setStyle(QStyleFactory::create("Fusion"));

	// Create main window
	PhaseNoiseAnalyzerApp mainWindow(csvFilenames, noplotRefence, useDarkTheme, dpi);

	// Set the application window icon
	mainWindow.setWindowIcon(appIcon);

	mainWindow.show();

	// Delay maximization slightly to ensure proper rendering after show()
	mainWindow.m_startupTimer->start(10); // Use the timer created in the constructor

	return app.exec();
}
