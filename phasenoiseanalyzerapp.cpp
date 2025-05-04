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
**             Date: 04 May 2025                                          **
**          Version: 1.0.0.1                                              **
****************************************************************************/
#include "phasenoiseanalyzerapp.h"
#include "constants.h"
#include "utils.h" // Include utility functions header
#include "version.h"

#include <QApplication>
#include <QMenuBar>
#include <QMenu>
#include <QAction>
#include <QToolBar>
#include <QStatusBar>
#include <QFileDialog>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QWidget>
#include <QLabel>
#include <QDockWidget>
#include <QCheckBox>
#include <QComboBox>
#include <QPushButton>
#include <QGroupBox>
#include <QSlider>
#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QColorDialog>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QHeaderView>
#include <QSplitter>
#include <QSizePolicy>
#include <QPalette>
#include <QStyleFactory>
#include <QTimer>
#include <QDateTime>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug> // For logging
#include <QtMath>
#include <limits> // For numeric limits
#include <QTextDocument>
#include <QPainter>
#include <algorithm>    // For std::sort
#include <QFontMetrics> // For text size calculation (optional, as QTextDocument calculates size)
#include <QMenu> // Added for context menu
#include <QContextMenuEvent> // Added for context menu

/*
 * Helper function to generate distinct colors for multiple plots.
 * This is a simple implementation, more sophisticated ones exist.
 */
QColor generateColor(int index, bool darkTheme) {
	// Base colors - chosen for reasonable visibility on light/dark backgrounds
	const QList<QColor> baseColorsLight = {
		QColor("#17a2a2"), // Teal (Original Measured)
		QColor("#ff7f0e"), // Orange
		QColor("#2ca02c"), // Green
		QColor("#d62728"), // Red
		QColor("#9467bd"), // Purple
		QColor("#8c564b"), // Brown
		QColor("#e377c2"), // Pink
		QColor("#7f7f7f"), // Gray
		QColor("#bcbd22"), // Olive
		QColor("#1f77b4")  // Blue
	};
	const QList<QColor> baseColorsDark = {
		QColor("cyan"),    // Original Measured
		QColor("orange"),
		QColor("lightgreen"),
		QColor("red"),
		QColor("magenta"),
		QColor("yellow"),
		QColor("pink"),
		QColor("lightgray"),
		QColor("#dbdb8d"), // Dark Olive
		QColor("#aec7e8")  // Light Blue
	};

	const QList<QColor>& colors = darkTheme ? baseColorsDark : baseColorsLight;
	return colors[index % colors.size()];
}

QColor generateRefColor(int index, bool darkTheme) {
	// Generate a slightly offset/desaturated color for reference
	if (darkTheme) {
		const QList<QColor> refColorsDark = {
			QColor("yellow"), // Original Reference
			QColor("#FFBF00"), // Amber
			QColor("#DAA520"), // GoldenRod
			QColor("#FF8C00"), // DarkOrange
			QColor("#B8860B"), // DarkGoldenRod
			QColor("#FFA07A"), // LightSalmon
		};
		return refColorsDark[index % refColorsDark.size()];
	} else {
		const QList<QColor> refColorsLight = {
			QColor("lightgrey"), // Original Reference
			QColor("#D3D3D3"),
			QColor("#C0C0C0"),
			QColor("#A9A9A9"),
			QColor("#BEBEBE"),
			QColor("#B2BEB5"), // Ash gray
		};
		return refColorsLight[index % refColorsLight.size()];
	}
}

PhaseNoiseAnalyzerApp::PhaseNoiseAnalyzerApp(const QStringList& csvFilenames,
											 bool plotReference,
											 bool useDarkTheme,
											 int dpi,
											 QWidget *parent)
	: QMainWindow(parent),
	m_plotReferenceDefault(plotReference),
	m_useDarkTheme(useDarkTheme),
	m_dpi(dpi)
{
	// Load the embedded font.
	QFontDatabase::addApplicationFont(":/fonts/LiberationSans-Regular.ttf");
	QFontDatabase::addApplicationFont(":/fonts/LiberationMono-Regular.ttf");
	QFont font("Liberation Sans",8);
	this->setFont(font);

	// Initialize spot noise colors based on initial theme
	m_spotNoiseColor = m_useDarkTheme ? m_defaultSpotNoiseColorDark : m_defaultSpotNoiseColorLight;

	setupUi();
	applyTheme(); // Apply initial theme

	// Connect after legend exists (created in setupUi -> createPlotArea -> initPlot)
	if (m_plot && m_plot->legend) {
		connect(m_plot, &QCustomPlot::afterLayout, this, &PhaseNoiseAnalyzerApp::positionSpotNoiseTable);
		// Connect legend click for enable/disable
		connect(m_plot, &QCustomPlot::legendClick, this, &PhaseNoiseAnalyzerApp::onLegendItemClicked);
			// Connect context menu request
		// (Context menu policy set in setupUi)
		connect(m_plot, &QWidget::customContextMenuRequested, this, &PhaseNoiseAnalyzerApp::showPlotContextMenu);
	}

	// Load data if filenames provided
	for (const QString& filename : csvFilenames) {
		if (!filename.isEmpty()) {
			loadData(filename);
		} else {
			initPlot(); // Show empty plot if no data
		}
	}

	// Setup timer for delayed maximization
	m_startupTimer = new QTimer(this);
	m_startupTimer->setSingleShot(true);
	connect(m_startupTimer, &QTimer::timeout, this, &PhaseNoiseAnalyzerApp::showMaximizedWithDelay);
}

PhaseNoiseAnalyzerApp::~PhaseNoiseAnalyzerApp()
{
	// Qt's parent-child mechanism handles deletion of most UI elements
}

void PhaseNoiseAnalyzerApp::setupUi()
{
	setWindowTitle("Phase Noise Analyzer");

	// Central widget setup
	m_centralWidget = new QWidget(this);
	setCentralWidget(m_centralWidget);
	m_mainLayout = new QVBoxLayout(m_centralWidget);
	m_mainLayout->setContentsMargins(5, 5, 5, 5); // Keep some margin

	// Set window size and center it
	resize(Constants::WINDOW_WIDTH, Constants::WINDOW_HEIGHT);
	centerWindow();

	// Status bar
	m_statusBar = new QStatusBar(this);
	setStatusBar(m_statusBar);
	m_statusBar->showMessage("Ready");

	// Create UI elements
	createMenus();
	createToolbars();
	createPlotArea(); // Creates m_plot
	createToolPanels(); // Creates dock widget and controls

	// Enable context menu for the plot area
	m_plot->setContextMenuPolicy(Qt::CustomContextMenu);
	// Connection moved to constructor after m_plot is guaranteed to exist

	// Initial state synchronization
	m_refCheckbox->setChecked(m_plotReferenceDefault);
	m_darkCheckbox->setChecked(m_useDarkTheme);
	m_toggleReferenceAction->setChecked(m_plotReferenceDefault);
	m_toggleDarkThemeAction->setChecked(m_useDarkTheme);
	m_spotCheckbox->setChecked(m_showSpotNoise);
	m_spotTableCheckbox->setChecked(m_showSpotNoiseTable);
	m_toggleSpotNoiseAction->setChecked(m_showSpotNoise);
	m_toggleSpotNoiseTableAction->setChecked(m_showSpotNoiseTable);
	m_crosshairAction->setChecked(m_useCrosshair);
	m_tbCrosshairAction->setChecked(m_useCrosshair);
	m_measureAction->setChecked(m_measureMode);
	m_tbMeasureAction->setChecked(m_measureMode);
	m_filterCheckbox->setChecked(m_filteringEnabled);
	m_filterAction->setChecked(m_filteringEnabled);
	m_tbFilterAction->setChecked(m_filteringEnabled);
	m_spurRemovalCheckbox->setChecked(m_spurRemovalEnabled);
	m_spurRemovalAction->setChecked(m_spurRemovalEnabled);
	m_tbSpurRemovalAction->setChecked(m_spurRemovalEnabled);
}

void PhaseNoiseAnalyzerApp::centerWindow()
{
// Using QStyle::alignedRect for better cross-platform centering
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
	// Qt 5 approach
	setGeometry(
		QStyle::alignedRect(
			Qt::LeftToRight,
			Qt::AlignCenter,
			size(),
			qApp->desktop()->availableGeometry(this) // Use available geometry of the screen the window is on
			)
		);
#else
	// Qt 6 approach
	const QRect availableGeometry = screen()->availableGeometry();
	setGeometry(
		QStyle::alignedRect(
			Qt::LeftToRight,
			Qt::AlignCenter,
			size(),
			availableGeometry
			)
		);
#endif
}

void PhaseNoiseAnalyzerApp::showMaximizedWithDelay()
{
	showMaximized();
}

void PhaseNoiseAnalyzerApp::createMenus()
{
	// File menu
	QMenu* fileMenu = menuBar()->addMenu("&File");

	m_openAction = fileMenu->addAction("&Open CSV...");
	m_openAction->setShortcut(QKeySequence::Open);
	connect(m_openAction, &QAction::triggered, this, &PhaseNoiseAnalyzerApp::onOpenFile);

	m_savePlotAction = fileMenu->addAction("&Save Plot...");
	m_savePlotAction->setShortcut(QKeySequence::Save);
	connect(m_savePlotAction, &QAction::triggered, this, &PhaseNoiseAnalyzerApp::onSavePlot);

	m_exportDataAction = fileMenu->addAction("&Export Data...");
	connect(m_exportDataAction, &QAction::triggered, this, &PhaseNoiseAnalyzerApp::onExportData);

	m_exportSpotAction = fileMenu->addAction("&Export Spot Noise Data...");
	connect(m_exportSpotAction, &QAction::triggered, this, &PhaseNoiseAnalyzerApp::onExportSpotNoise);

	fileMenu->addSeparator();

	m_exitAction = fileMenu->addAction("E&xit");
	m_exitAction->setShortcut(QKeySequence::Quit);
	connect(m_exitAction, &QAction::triggered, this, &PhaseNoiseAnalyzerApp::close);

	// View menu
	QMenu* viewMenu = menuBar()->addMenu("&View");
	m_toggleDarkThemeAction = viewMenu->addAction("&Dark Theme", this, &PhaseNoiseAnalyzerApp::toggleTheme);
	m_toggleDarkThemeAction->setCheckable(true);

	m_toggleReferenceAction = viewMenu->addAction("Show &Reference Noise", this, &PhaseNoiseAnalyzerApp::toggleReference);
	m_toggleReferenceAction->setCheckable(true);

	m_toggleSpotNoiseAction = viewMenu->addAction("Show Spot Noise &Points", this, &PhaseNoiseAnalyzerApp::toggleSpotNoise);
	m_toggleSpotNoiseAction->setCheckable(true);

	m_toggleSpotNoiseTableAction = viewMenu->addAction("Show Spot Noise &Table", this, &PhaseNoiseAnalyzerApp::toggleSpotNoiseTable);
	m_toggleSpotNoiseTableAction->setCheckable(true);

	// Tools menu
	QMenu* toolsMenu = menuBar()->addMenu("&Tools");
	m_crosshairAction = toolsMenu->addAction("&Crosshair Cursor", this, &PhaseNoiseAnalyzerApp::toggleCrosshair);
	m_crosshairAction->setCheckable(true);
	m_measureAction = toolsMenu->addAction("&Measurement Tool", this, &PhaseNoiseAnalyzerApp::toggleMeasurementTool);
	m_measureAction->setCheckable(true);
	toolsMenu->addSeparator();
	m_filterAction = toolsMenu->addAction("&Filter Data", this, &PhaseNoiseAnalyzerApp::toggleDataFiltering);
	m_filterAction->setCheckable(true);
	m_spurRemovalAction = toolsMenu->addAction("Enable Spur Remo&val", this, &PhaseNoiseAnalyzerApp::toggleSpurRemoval);
	m_spurRemovalAction->setCheckable(true);

	// Help menu
	QMenu* helpMenu = menuBar()->addMenu("&Help");
	helpMenu->addAction("&About", this, [this]() {
		QMessageBox::about(this, "About Phase Noise Analyzer",
						   "<h3>Phase Noise Analyzer (C++/Qt)</h3>"
						   "<p>Copyright(c) 2025 Benjamin Vernoux</p>"
						   "<p><a href='mailto:bvernoux@gmail.com'>bvernoux@gmail.com</a></p>"
						   "<p>A tool for analyzing and visualizing phase noise data.</p>"
						   "<p>Version " + QString(VER_FILEVERSION_STR) + QString(VER_DATE_INFO_STR) + "</p>");
	});

	helpMenu->addAction("About &Qt", qApp, &QApplication::aboutQt);
}

void PhaseNoiseAnalyzerApp::createToolbars()
{
	m_mainToolbar = addToolBar("Main Toolbar");
	m_mainToolbar->setMovable(true);
	m_mainToolbar->setFloatable(true);

	// Add file operations (Consider adding icons later)
	m_tbOpenAction = m_mainToolbar->addAction("Open", this, &PhaseNoiseAnalyzerApp::onOpenFile);
	m_tbOpenAction->setToolTip("Open CSV file (Ctrl+O)");
	m_tbSaveAction = m_mainToolbar->addAction("Save", this, &PhaseNoiseAnalyzerApp::onSavePlot);
	m_tbSaveAction->setToolTip("Save plot as image (Ctrl+S)");

	m_mainToolbar->addSeparator();

	// Add view controls
	m_tbThemeAction = m_mainToolbar->addAction("Theme", this, [this](){ toggleTheme(!m_useDarkTheme); }); // Toggle directly
	m_tbThemeAction->setToolTip("Toggle dark/light theme");

	m_mainToolbar->addSeparator();

	// Add tool controls
	m_tbCrosshairAction = m_mainToolbar->addAction("Crosshair", this, &PhaseNoiseAnalyzerApp::toggleCrosshair);
	m_tbCrosshairAction->setToolTip("Enable crosshair cursor");
	m_tbCrosshairAction->setCheckable(true);

	m_tbMeasureAction = m_mainToolbar->addAction("Measure", this, &PhaseNoiseAnalyzerApp::toggleMeasurementTool);
	m_tbMeasureAction->setToolTip("Enable measurement tool");
	m_tbMeasureAction->setCheckable(true);

	m_mainToolbar->addSeparator();

	// Add filtering/spur controls
	m_tbFilterAction = m_mainToolbar->addAction("Filter", this, &PhaseNoiseAnalyzerApp::toggleDataFiltering);
	m_tbFilterAction->setToolTip("Enable/disable data filtering");
	m_tbFilterAction->setCheckable(true);

	m_tbSpurRemovalAction = m_mainToolbar->addAction("SpurRem", this, &PhaseNoiseAnalyzerApp::toggleSpurRemoval);
	m_tbSpurRemovalAction->setToolTip("Enable/disable spur removal");
	m_tbSpurRemovalAction->setCheckable(true);

	m_mainToolbar->addSeparator();

	// Matplotlib Navigation Equivalents
	m_homeAction = m_mainToolbar->addAction("Home", this, &PhaseNoiseAnalyzerApp::homeView);
	m_homeAction->setToolTip("Reset original view");

	// Pan/Zoom Buttons
	m_panzoomButton = new QPushButton("Pan/Zoom", this);
	m_panzoomButton->setToolTip("Pan axes with left mouse, zoom with wheel");
	m_panzoomButton->setCheckable(true);
	connect(m_panzoomButton, &QPushButton::clicked, this, &PhaseNoiseAnalyzerApp::panzoomButtonClicked);
	m_mainToolbar->addWidget(m_panzoomButton);
}

void PhaseNoiseAnalyzerApp::createPlotArea()
{
	m_plot = new QCustomPlot(m_centralWidget);
	m_mainLayout->addWidget(m_plot, 1); // Give plot area stretch factor

	// Configure interactions (initially allow drag/zoom, controlled by buttons later)
	m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables); // Allow selecting plottables

	// Connect plot signals
	connect(m_plot, &QCustomPlot::mouseMove, this, &PhaseNoiseAnalyzerApp::onPlotMouseMove);
	connect(m_plot, &QCustomPlot::mousePress, this, &PhaseNoiseAnalyzerApp::onPlotMousePress);
	connect(m_plot->yAxis, SIGNAL(rangeChanged(QCPRange)), this, SLOT(synchronizeYAxes(QCPRange)));

	// Initialize plot appearance (will be updated in initPlot/applyTheme)
	initPlot();
}

void PhaseNoiseAnalyzerApp::initPlot()
{
	if (!m_plot) {
		qWarning() << "initPlot: m_plot is null!";
		return;
	}

	// Clear graphs associated with datasets, but don't clear the datasets themselves
	for (PlotData& data : m_datasets) { data.graphMeasured = nullptr; data.graphReference = nullptr; data.graphReferenceOutline = nullptr; data.fillReferenceBase = nullptr; }
	m_plot->clearGraphs();
	m_plot->clearItems();  // Clear previous items like tracers, annotations, etc.

	// Reset pointers to plot objects that were potentially removed
	m_spotNoiseTableText = nullptr;
	m_cursorAnnotation = nullptr;
	m_cursorTracer = nullptr;
	m_measurementText = nullptr;
	m_titleElement = nullptr; // Reset since it will be recreated
	m_subtitleText = nullptr; // Reset since it will be recreated

	// --- Prepare for Layout Reset ---
	// Find the main axis rect - IMPORTANT: Keep this pointer safe!
	QCPAxisRect *mainAxisRect = nullptr;
	if (m_plot->axisRectCount() > 0) {
		mainAxisRect = m_plot->axisRect(0);
	} else {
		// If no axis rect exists, we absolutely need to create one.
		qWarning() << "initPlot: No axis rect found. Creating default.";
		mainAxisRect = new QCPAxisRect(m_plot, true); // Create with default axes
		// Don't add it to the layout yet, the layout will be cleared first.
	}

	// Keep the legend pointer safe if it exists
	QCPLegend *currentLegend = m_plot->legend; // Could be null

	// --- Explicitly Delete Old Title/Subtitle Elements ---
	// Remove from layout first (if they exist and are in a layout) then delete
	if (m_titleElement) {
		if (m_titleElement->layout()) m_titleElement->layout()->take(m_titleElement);
		delete m_titleElement;
		m_titleElement = nullptr;
	}
	if (m_subtitleText) {
		if (m_subtitleText->layout()) m_subtitleText->layout()->take(m_subtitleText);
		delete m_subtitleText;
		m_subtitleText = nullptr;
	}

	// --- Clear the Layout Grid Robustly ---
	// Take the elements we want to keep OUT of the layout first.
	if (mainAxisRect && mainAxisRect->layout()) {
		mainAxisRect->layout()->take(mainAxisRect); // Detach from layout
	}
	if (currentLegend && currentLegend->layout()) {
		// Check if it's in the main plot layout or an inset layout
		if (currentLegend->layout() == m_plot->plotLayout()) {
			currentLegend->layout()->take(currentLegend);
		} else if (mainAxisRect && currentLegend->layout() == mainAxisRect->insetLayout()) {
			// It's in the inset layout, which might be cleared/recreated depending
			// on mainAxisRect's state. Let's take it out for safety.
			mainAxisRect->insetLayout()->take(currentLegend);
		}
	}

	// Now clear the main plot layout. This should remove and delete
	// any other potentially lingering elements in the grid.
	m_plot->plotLayout()->clear(); // Removes and deletes elements IN THE GRID

	// --- Rebuild Layout Structure ---
	// Row 0: Title
	m_plot->plotLayout()->insertRow(0);
	m_titleElement = new QCPTextElement(m_plot, "Phase Noise", QFont("Liberation Sans", 12, QFont::Bold)); // Create NEW title
	m_titleElement->setObjectName("plotTitle");
	m_plot->plotLayout()->addElement(0, 0, m_titleElement);

	// Row 1: Subtitle
	m_plot->plotLayout()->insertRow(1);
	m_subtitleText = new QCPTextElement(m_plot, "", QFont("Liberation Sans", 9)); // Create NEW subtitle
	m_subtitleText->setObjectName("plotSubtitle");
	m_plot->plotLayout()->addElement(1, 0, m_subtitleText);

	// Row 2: Main Axis Rect (Re-add the one we kept)
	m_plot->plotLayout()->insertRow(2);
	if (!mainAxisRect->parent()) mainAxisRect->setParent(m_plot); // Ensure correct parentage
	m_plot->plotLayout()->addElement(2, 0, mainAxisRect);

	// --- Apply Theme Colors ---
	QColor bgColor, axisColor, tickColor, gridColor, labelColor, textColor;
	if (m_useDarkTheme) {
		bgColor = Constants::DARK_BG_COLOR; axisColor = Constants::DARK_AXIS_COLOR; tickColor = Constants::DARK_TICK_COLOR;
		gridColor = Constants::DARK_GRID_COLOR; labelColor = Constants::DARK_TEXT_COLOR; textColor = Constants::DARK_TEXT_COLOR;
	} else {
		bgColor = Constants::LIGHT_BG_COLOR; axisColor = Constants::LIGHT_AXIS_COLOR; tickColor = Constants::LIGHT_TICK_COLOR;
		gridColor = Constants::LIGHT_GRID_COLOR; labelColor = Constants::LIGHT_TEXT_COLOR; textColor = Constants::LIGHT_TEXT_COLOR;
	}
	// Store member colors
	m_tickLabelColor = tickColor;
	m_gridColor = gridColor;
	m_axisLabelColor = labelColor;
	m_textColor = textColor;
	m_annotationBgColor = m_useDarkTheme ? Constants::DARK_ANNOTATION_BG : Constants::LIGHT_ANNOTATION_BG;

	m_plot->setBackground(bgColor);
	mainAxisRect->setBackground(bgColor);

	// --- Configure Axes ---
	QCPAxis *xAxis = mainAxisRect->axis(QCPAxis::atBottom);
	QCPAxis *yAxis = mainAxisRect->axis(QCPAxis::atLeft);
	QCPAxis *xAxis2 = mainAxisRect->axis(QCPAxis::atTop);
	QCPAxis *yAxis2 = mainAxisRect->axis(QCPAxis::atRight);

	if (!xAxis || !yAxis || !xAxis2 || !yAxis2) {
		qWarning() << "initPlot: Default axes not found/created on mainAxisRect after layout rebuild.";
		mainAxisRect->setupFullAxesBox(); // Try creating them if missing
		xAxis = mainAxisRect->axis(QCPAxis::atBottom); // Re-fetch pointers
		yAxis = mainAxisRect->axis(QCPAxis::atLeft);
		xAxis2 = mainAxisRect->axis(QCPAxis::atTop);
		yAxis2 = mainAxisRect->axis(QCPAxis::atRight);
		if (!xAxis || !yAxis || !xAxis2 || !yAxis2) {
			qCritical() << "initPlot: Failed to ensure default axes exist.";
			return;
		}
	}

	// (Keep axis configuration code, ensuring layers are set)
	xAxis->setLayer("axes"); xAxis->setLabel("Frequency Offset (Hz)"); xAxis->setLabelColor(labelColor); xAxis->setBasePen(QPen(tickColor)); xAxis->setTickPen(QPen(tickColor)); xAxis->setSubTickPen(QPen(tickColor)); xAxis->setTickLabelColor(tickColor); xAxis->grid()->setLayer("grid"); xAxis->grid()->setPen(QPen(gridColor, 0.5)); xAxis->grid()->setSubGridPen(QPen(gridColor, 0.3, Qt::DotLine)); xAxis->grid()->setSubGridVisible(true); xAxis->setScaleType(QCPAxis::stLogarithmic); QSharedPointer<QCPAxisTickerSI> siTicker(new QCPAxisTickerSI); siTicker->setLogBase(10); xAxis->setTicker(siTicker); xAxis->setVisible(true); xAxis->setTickLabels(true);
	yAxis->setLayer("axes"); yAxis->setLabel("SSB Phase Noise (dBc/Hz)"); yAxis->setLabelColor(labelColor); yAxis->setBasePen(QPen(tickColor)); yAxis->setTickPen(QPen(tickColor)); yAxis->setSubTickPen(QPen(tickColor)); yAxis->setTickLabelColor(tickColor); yAxis->grid()->setLayer("grid"); yAxis->grid()->setPen(QPen(gridColor, 0.5)); yAxis->grid()->setSubGridPen(QPen(gridColor, 0.3, Qt::DotLine)); yAxis->grid()->setSubGridVisible(true); QSharedPointer<QCPAxisTickerFixed> fixedTickerY(new QCPAxisTickerFixed); fixedTickerY->setTickStep(Constants::Y_AXIS_MAJOR_TICK); fixedTickerY->setScaleStrategy(QCPAxisTickerFixed::ssNone); yAxis->setTicker(fixedTickerY); yAxis->setNumberFormat("f"); yAxis->setNumberPrecision(0); yAxis->setVisible(true); yAxis->setTickLabels(true);
	yAxis2->setLayer("axes"); yAxis2->setVisible(true); yAxis2->setTickLabels(true); yAxis2->setLabelColor(labelColor); yAxis2->setBasePen(QPen(tickColor)); yAxis2->setTickPen(QPen(tickColor)); yAxis2->setSubTickPen(QPen(tickColor)); yAxis2->setTickLabelColor(tickColor); yAxis2->grid()->setVisible(false); QSharedPointer<QCPAxisTickerFixed> fixedTickerY2(new QCPAxisTickerFixed); fixedTickerY2->setTickStep(Constants::Y_AXIS_MAJOR_TICK); fixedTickerY2->setScaleStrategy(QCPAxisTickerFixed::ssNone); yAxis2->setTicker(fixedTickerY2); yAxis2->setNumberFormat("f"); yAxis2->setNumberPrecision(0);
	xAxis2->setLayer("axes"); xAxis2->setVisible(false); xAxis2->setTickLabels(false); xAxis2->setLabelColor(labelColor); xAxis2->setBasePen(QPen(tickColor)); xAxis2->setTickPen(QPen(tickColor)); xAxis2->setSubTickPen(QPen(tickColor)); xAxis2->setTickLabelColor(tickColor); xAxis2->grid()->setVisible(false);

	// --- Configure Title and Subtitle ---
	if (m_titleElement) {
		m_titleElement->setText("Phase Noise");
		m_titleElement->setFont(QFont("Liberation Sans", 12, QFont::Bold));
		m_titleElement->setTextColor(textColor);
		m_titleElement->setTextFlags(Qt::AlignHCenter | Qt::AlignTop);
	}
	if (m_subtitleText) {
		m_subtitleText->setText("");
		m_subtitleText->setFont(QFont("Liberation Sans", 9));
		m_subtitleText->setTextColor(textColor);
		m_subtitleText->setTextFlags(Qt::AlignHCenter | Qt::AlignTop);
		m_subtitleText->setMargins(QMargins(0, 0, 0, 4));
	}

	// --- Configure Legend ---
	QCPLayoutInset *insetLayout = mainAxisRect->insetLayout();
	if (!currentLegend) { // If legend pointer was null (e.g., first run or previously removed), use the member pointer
		currentLegend = m_plot->legend;
	}
	if (!currentLegend) { // Still null? Create it.
		currentLegend = new QCPLegend;
		currentLegend->setParent(m_plot);
		currentLegend->setLayer("legend");
		m_plot->legend = currentLegend; // Assign to member pointer
	}

	// Add legend to the inset layout if it's not already there
	int legendIndex = -1;
	for (int i = 0; i < insetLayout->elementCount(); ++i) {
		if (insetLayout->elementAt(i) == currentLegend) {
			legendIndex = i;
			break;
		}
	}
	Qt::Alignment legendAlignment = Qt::AlignTop | Qt::AlignRight;
	if (legendIndex == -1) {
		if (currentLegend->layout()) currentLegend->layout()->take(currentLegend); // Ensure detached
		insetLayout->addElement(currentLegend, legendAlignment);
	} else {
		insetLayout->setInsetAlignment(legendIndex, legendAlignment);
	}
	currentLegend->setVisible(true);
	currentLegend->setBrush(QBrush(m_annotationBgColor));
	currentLegend->setBorderPen(QPen(tickColor));
	currentLegend->setTextColor(textColor);
	currentLegend->setSelectableParts(QCPLegend::spItems); // Make items clickable

	// Final layout updates
	m_plot->plotLayout()->simplify(); // Recalculate grid layout structure

	m_plot->replot();
}

void PhaseNoiseAnalyzerApp::synchronizeYAxes(const QCPRange &range)
{
	// Update the right y-axis to match the left y-axis
	m_plot->yAxis2->setRange(range);
	m_plot->replot(); // Ensure both axes are redrawn
}

void PhaseNoiseAnalyzerApp::updatePlot()
{
	if (!m_plot) {
		qWarning() << "updatePlot: m_plot is null!";
		return;
	}

	QCPAxisRect *mainAxisRect = nullptr;
	if (m_plot->axisRectCount() > 0) mainAxisRect = m_plot->axisRect(0);
	if (!mainAxisRect) {
		qWarning() << "updatePlot: No axis rect found."; return;
	}
	QCPAxis *xAxis = mainAxisRect->axis(QCPAxis::atBottom);
	QCPAxis *yAxis = mainAxisRect->axis(QCPAxis::atLeft);
	QCPAxis *yAxis2 = mainAxisRect->axis(QCPAxis::atRight);
	if (!xAxis || !yAxis || !yAxis2) {
		qWarning() << "updatePlot: Required axes not found on mainAxisRect."; return;
	}

	// --- Temporarily disable auto legend adding ---
	bool autoLegendWas = m_plot->autoAddPlottableToLegend();
	m_plot->setAutoAddPlottableToLegend(false);

	// --- Clear previous dynamic items (spot noise, measurement) ---
	for(auto item : std::as_const(m_spotNoiseMarkers)) { if (item) m_plot->removeItem(item); }
	m_spotNoiseMarkers.clear();
	for(auto item : std::as_const(m_spotNoiseLabels)) { if (item) m_plot->removeItem(item); }
	m_spotNoiseLabels.clear();
	// Keep measurement items unless explicitly cleared elsewhere

	// --- Clear existing graphs and legend items before adding new ones ---
	for (PlotData& data : m_datasets) {
		// Remove graphs from plot (this also removes them from legend if auto add was on)
		if (data.graphMeasured) m_plot->removeGraph(data.graphMeasured);
		if (data.graphReference) m_plot->removeGraph(data.graphReference);
		if (data.graphReferenceOutline) m_plot->removeGraph(data.graphReferenceOutline);
		if (data.fillReferenceBase) m_plot->removeGraph(data.fillReferenceBase);
		// Reset pointers
		data.graphMeasured = nullptr;
		data.graphReference = nullptr;
		data.graphReferenceOutline = nullptr;
		data.fillReferenceBase = nullptr;
	}
	// Explicitly clear any remaining legend items
	if (m_plot->legend) {
		m_plot->legend->clearItems();
	}

	// --- Determine Data Source & Apply Spur Removal ---
	applySpurRemoval(); // Modifies filtered data within m_datasets

	// --- Apply Theme Colors & Base Plot Setup ---
	QColor bgColor, axisColor, tickColor, gridColor, labelColor, textColor;
	if (m_useDarkTheme) {
		bgColor = Constants::DARK_BG_COLOR; axisColor = Constants::DARK_AXIS_COLOR; tickColor = Constants::DARK_TICK_COLOR;
		gridColor = Constants::DARK_GRID_COLOR; labelColor = Constants::DARK_TEXT_COLOR; textColor = Constants::DARK_TEXT_COLOR;
	} else {
		bgColor = Constants::LIGHT_BG_COLOR; axisColor = Constants::LIGHT_AXIS_COLOR; tickColor = Constants::LIGHT_TICK_COLOR;
		gridColor = Constants::LIGHT_GRID_COLOR; labelColor = Constants::LIGHT_TEXT_COLOR; textColor = Constants::LIGHT_TEXT_COLOR;
	}
	m_tickLabelColor = tickColor; m_gridColor = gridColor; m_axisLabelColor = labelColor; m_textColor = textColor; m_annotationBgColor = m_useDarkTheme ? Constants::DARK_ANNOTATION_BG : Constants::LIGHT_ANNOTATION_BG;

	m_plot->setBackground(bgColor);
	mainAxisRect->setBackground(bgColor);

	// --- Update Axes Appearance ---
	xAxis->setLabelColor(labelColor); xAxis->setBasePen(QPen(tickColor)); xAxis->setTickPen(QPen(tickColor)); xAxis->setSubTickPen(QPen(tickColor)); xAxis->setTickLabelColor(tickColor); xAxis->grid()->setPen(QPen(gridColor, 0.5)); xAxis->grid()->setSubGridPen(QPen(gridColor, 0.3, Qt::DotLine));
	yAxis->setLabelColor(labelColor); yAxis->setBasePen(QPen(tickColor)); yAxis->setTickPen(QPen(tickColor)); yAxis->setSubTickPen(QPen(tickColor)); yAxis->setTickLabelColor(tickColor); yAxis->grid()->setPen(QPen(gridColor, 0.5)); yAxis->grid()->setSubGridPen(QPen(gridColor, 0.3, Qt::DotLine));
	yAxis2->setBasePen(QPen(tickColor)); yAxis2->setTickPen(QPen(tickColor)); yAxis2->setSubTickPen(QPen(tickColor)); yAxis2->setTickLabelColor(tickColor);

	// --- Update Title/Subtitle ---
	if (m_titleElement) {
		m_titleElement->setTextColor(textColor);
		m_titleElement->setTextFlags(Qt::AlignHCenter | Qt::AlignTop);
	}
	if (m_subtitleText) {
		QString filenamePart;
		QString timestampPart;
		if (m_datasets.isEmpty()) {
			filenamePart = "No file loaded";
		} else if (m_datasets.size() == 1) {
			filenamePart = QFileInfo(m_datasets[0].filename).fileName();
			QFileInfo fileInfo(m_datasets[0].filename);
			if (fileInfo.exists()) timestampPart = fileInfo.lastModified().toString("yyyy-MM-dd HH:mm:ss");
		} else {
			filenamePart = QString("%1 files loaded").arg(m_datasets.size());
		}

		QString filterPart = m_filteringEnabled ? QString(" | Filter: %1(W=%2)").arg(m_filterTypeCombo->currentText()).arg(m_filterWindowSpin->value()) : "";
		QString spurPart = m_spurRemovalEnabled ? " | SpurRem:On" : "";
		m_subtitleText->setText(QString("%1 (%2)%3%4").arg(filenamePart).arg(timestampPart.isEmpty() ? "N/A" : timestampPart).arg(filterPart).arg(spurPart));
		m_subtitleText->setTextColor(textColor);
		m_subtitleText->setTextFlags(Qt::AlignHCenter | Qt::AlignTop);
		m_subtitleText->setMargins(QMargins(0, 0, 0, 4));
	}

	// --- Update Legend Appearance (border, background, text color) ---
	if (m_plot->legend) {
		m_plot->legend->setBrush(QBrush(m_annotationBgColor));
		m_plot->legend->setBorderPen(QPen(tickColor));
		m_plot->legend->setTextColor(textColor); // Default text color
	}

	// --- Update Grid Visibility ---
	bool showGrid = m_gridCheckbox->isChecked();
	xAxis->grid()->setVisible(showGrid);
	yAxis->grid()->setVisible(showGrid);
	xAxis->grid()->setSubGridVisible(showGrid);
	yAxis->grid()->setSubGridVisible(showGrid);

	// --- Update Axis Tickers ---
	QSharedPointer<QCPAxisTickerSI> siTicker = qSharedPointerDynamicCast<QCPAxisTickerSI>(xAxis->ticker());
	if (!siTicker) { siTicker = QSharedPointer<QCPAxisTickerSI>(new QCPAxisTickerSI); xAxis->setTicker(siTicker); }
	siTicker->setLogBase(10);
	QSharedPointer<QCPAxisTickerFixed> fixedTickerY_upd = QSharedPointer<QCPAxisTickerFixed>(new QCPAxisTickerFixed);
	fixedTickerY_upd->setTickStep(Constants::Y_AXIS_MAJOR_TICK); fixedTickerY_upd->setScaleStrategy(QCPAxisTickerFixed::ssNone);
	yAxis->setTicker(fixedTickerY_upd); yAxis->setNumberFormat("f"); yAxis->setNumberPrecision(0);
	QSharedPointer<QCPAxisTickerFixed> fixedTickerY2_upd = QSharedPointer<QCPAxisTickerFixed>(new QCPAxisTickerFixed);
	fixedTickerY2_upd->setTickStep(Constants::Y_AXIS_MAJOR_TICK); fixedTickerY2_upd->setScaleStrategy(QCPAxisTickerFixed::ssNone);
	yAxis2->setTicker(fixedTickerY2_upd); yAxis2->setNumberFormat("f"); yAxis2->setNumberPrecision(0);

	// --- Plot Data for Each Dataset ---
	bool isFirstVisible = true;
	QCPGraph* firstVisibleMeasuredGraph = nullptr;

	for (PlotData& data : m_datasets) {
		// Create/Update Graphs for this dataset
		const QVector<double>& freqData = data.frequencyOffset;
		const QVector<double>& noiseData = m_spurRemovalEnabled ? data.phaseNoiseFiltered : (m_filteringEnabled ? data.phaseNoiseFiltered : data.phaseNoise);
		const QVector<double>& refData = m_filteringEnabled ? data.referenceNoiseFiltered : data.referenceNoise;
		QString baseName = (m_datasets.size() > 1) ? data.displayName : "Measured";
		bool plotRef = m_refCheckbox->isChecked();

		QCPPlottableLegendItem* measuredLegendItem = nullptr;
		QCPPlottableLegendItem* refLegendItem = nullptr;

		// --- Measured Graph ---
		if (!freqData.isEmpty()) {
			data.graphMeasured = m_plot->addGraph(xAxis, yAxis); // Add graph
			data.graphMeasured->setName(baseName);
			data.graphMeasured->setPen(QPen(data.measuredColor, 1.5));
			data.graphMeasured->setData(freqData, noiseData);
			data.graphMeasured->setSelectable(QCP::stDataRange);
			data.graphMeasured->setVisible(data.isVisible); // Set visibility

			// Manually create and add the legend item
			if (m_plot->legend) {
				measuredLegendItem = new QCPPlottableLegendItem(m_plot->legend, data.graphMeasured);
				m_plot->legend->addItem(measuredLegendItem);
			}
			// Do NOT call data.graphMeasured->addToLegend();

			if (data.isVisible && isFirstVisible) {
				firstVisibleMeasuredGraph = data.graphMeasured;
				isFirstVisible = false;
			}
		}

		// --- Reference Graph ---
		if (plotRef && data.hasReferenceData && !freqData.isEmpty()) {
			QVector<double> validRefFreq, validRefNoise;
			for(int k=0; k<freqData.size(); ++k) {
				if (k < refData.size() && !std::isnan(refData[k])) {
					validRefFreq.append(freqData[k]);
					validRefNoise.append(refData[k]);
				}
			}
			if (!validRefFreq.isEmpty()) {
				data.graphReference = m_plot->addGraph(xAxis, yAxis); // Add graph
				data.graphReference->setName(baseName + " (Ref)");
				data.graphReference->setData(validRefFreq, validRefNoise);
				data.graphReference->setSelectable(QCP::stNone);
				data.graphReference->setVisible(data.isVisible); // Set visibility

				if (m_useDarkTheme) {
					data.graphReference->setPen(QPen(data.referenceColor, 1.5));
					data.graphReference->setBrush(Qt::NoBrush);
				} else {
					data.fillReferenceBase = m_plot->addGraph(xAxis, yAxis); // Add baseline graph
					// We will set data later after ranges are known
					data.fillReferenceBase->setVisible(false);
					// data.fillReferenceBase->removeFromLegend(); // Not needed as autoAdd is false

					data.graphReference->setPen(Qt::NoPen);
					QColor refFillColor = data.referenceColor; refFillColor.setAlphaF(0.7f);
					data.graphReference->setBrush(QBrush(refFillColor));
					data.graphReference->setChannelFillGraph(data.fillReferenceBase);

					data.graphReferenceOutline = m_plot->addGraph(xAxis, yAxis); // Add outline graph
					data.graphReferenceOutline->setData(validRefFreq, validRefNoise);
					data.graphReferenceOutline->setPen(QPen(Qt::darkGray, 0.5));
					data.graphReferenceOutline->setBrush(Qt::NoBrush);
					data.graphReferenceOutline->setSelectable(QCP::stNone);
					// data.graphReferenceOutline->removeFromLegend(); // Not needed
					data.graphReferenceOutline->setVisible(data.isVisible); // Also control outline visibility
				}
				// Manually create and add the legend item
				if (m_plot->legend) {
					refLegendItem = new QCPPlottableLegendItem(m_plot->legend, data.graphReference);
					m_plot->legend->addItem(refLegendItem);
				}
				// Do NOT call data.graphReference->addToLegend();
			}
		}

		// --- Update Legend Item Appearance (Strikethrough) ---
		if (measuredLegendItem) {
			QFont itemFont = measuredLegendItem->font();
			itemFont.setStrikeOut(!data.isVisible);
			measuredLegendItem->setFont(itemFont);
			measuredLegendItem->setTextColor(m_textColor); // Keep original color
		}
		if (refLegendItem) {
			QFont itemFont = refLegendItem->font();
			itemFont.setStrikeOut(!data.isVisible);
			refLegendItem->setFont(itemFont);
			refLegendItem->setTextColor(m_textColor); // Keep original color
		}
		// --- End Update Legend Item Appearance ---

	} // End loop through datasets

	// --- Axis Ranges (Set after graphs potentially added data) ---
	double xMin = Constants::FREQ_POINTS[m_minFreqSliderIndex];
	double xMax = Constants::FREQ_POINTS[m_maxFreqSliderIndex];
	xMin = qMax(Constants::X_AXIS_MIN, xMin);
	if (xMax <= xMin) xMax = xMin * 10;
	xAxis->setRange(xMin, xMax);

	double yMin = m_yMinSpin->value();
	double yMax = m_yMaxSpin->value();
	if (yMin >= yMax) yMax = yMin + Constants::Y_AXIS_MAJOR_TICK;
	yAxis->setRange(yMin, yMax);
	yAxis2->setRange(yMin, yMax);

	// Update baseline graphs for reference fill *after* setting final Y range
	for (PlotData& data : m_datasets) {
		if (data.fillReferenceBase && data.isVisible) { // Only update if visible
			QSharedPointer<QCPGraphDataContainer> baseData = data.fillReferenceBase->data();
				// Need to regenerate the valid data points for the baseline
			QVector<double> validRefFreq;
			const QVector<double>& refData = m_filteringEnabled ? data.referenceNoiseFiltered : data.referenceNoise;
			for(int k=0; k<data.frequencyOffset.size(); ++k) {
				if (k < refData.size() && !std::isnan(refData[k])) {
					validRefFreq.append(data.frequencyOffset[k]);
				}
			}
			if (!validRefFreq.isEmpty()) {
				QVector<double> baseValues(validRefFreq.size(), yAxis->range().lower);
				data.fillReferenceBase->setData(validRefFreq, baseValues); // Set data here
				data.fillReferenceBase->setVisible(true); // Make sure it's visible now
			} else {
				data.fillReferenceBase->setVisible(false); // Hide if no valid ref data
			}
		} else if (data.fillReferenceBase) {
			data.fillReferenceBase->setVisible(false); // Hide if dataset not visible
		}
		// Ensure outline visibility matches dataset visibility
		if(data.graphReferenceOutline) {
			data.graphReferenceOutline->setVisible(data.isVisible);
		}
	}

	// --- Calculate and Draw Spot Noise Points/Labels ---
	calculateSpotNoise(); // Calculates based on first visible dataset
	if (m_showSpotNoise && firstVisibleMeasuredGraph) { // Use the first visible graph pointer
		QCPLayer *overlayLayer = m_plot->layer("overlay");
		if (!overlayLayer) overlayLayer = m_plot->layer("main");
		for (auto it = m_spotNoiseData.constBegin(); it != m_spotNoiseData.constEnd(); ++it) {
			const QString& displayName = it.key();
			double actualFreq = it.value().first;
			double actualNoise = it.value().second;

			QCPItemTracer* tracer = new QCPItemTracer(m_plot);
			if (overlayLayer) tracer->setLayer(overlayLayer);
			tracer->setGraph(firstVisibleMeasuredGraph); // Attach tracer to the correct graph
			tracer->setGraphKey(actualFreq);
			tracer->setInterpolating(true); // Ensure it interpolates if key not exact
			tracer->setStyle(QCPItemTracer::tsCircle);
			tracer->setPen(QPen(m_spotNoiseColor));
			tracer->setBrush(m_spotNoiseColor);
			tracer->setSize(6);
			tracer->setSelectable(false);
			m_spotNoiseMarkers.append(tracer);

			QCPItemText* label = new QCPItemText(m_plot);
			if (overlayLayer) label->setLayer(overlayLayer);
			label->setText(QString("%1\n%2 dBc/Hz").arg(displayName).arg(actualNoise, 0, 'f', 1));
			label->setFont(QFont("Liberation Sans", 8));
			label->setColor(m_textColor);
			label->setBrush(QBrush(m_annotationBgColor));
			label->setPen(QPen(Qt::NoPen));
			label->setPadding(QMargins(3, 3, 3, 3));
			label->setSelectable(false);
			label->position->setParentAnchor(tracer->position);

			double logXMin = qLn(xAxis->range().lower);
			double logXMax = qLn(xAxis->range().upper);
			double currentLogX = (actualFreq > 0) ? qLn(actualFreq) : logXMin;
			const double yOffset = 25; double xOffset = 0;
			Qt::Alignment hAlign = Qt::AlignHCenter; Qt::Alignment vAlign = Qt::AlignBottom;
			double logRangeSize = logXMax - logXMin;
			if (logRangeSize > 1e-6) {
				if (currentLogX < logXMin + logRangeSize * 0.25) { xOffset = 40; hAlign = Qt::AlignLeft; }
				else if (currentLogX > logXMax - logRangeSize * 0.25) { xOffset = -40; hAlign = Qt::AlignRight; }
			}
			label->position->setCoords(xOffset, -yOffset);
			label->setTextAlignment(hAlign | vAlign);
			m_spotNoiseLabels.append(label);
		}
	}

	// --- Add/Update Spot Noise Table ---
	addSpotNoiseTable();

	// --- Restore auto legend setting and Final Replot ---
	m_plot->setAutoAddPlottableToLegend(autoLegendWas); // Restore original setting
	if (m_plot->legend) {
		m_plot->legend->setVisible(m_plot->legend->itemCount() > 0);
	}
	m_plot->plotLayout()->simplify();
	m_plot->replot();
}

void PhaseNoiseAnalyzerApp::createToolPanels()
{
	m_plotDock = new QDockWidget("Plot Controls", this);
	m_plotDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
	m_plotDock->setFeatures(QDockWidget::DockWidgetFloatable | QDockWidget::DockWidgetMovable);

	m_plotWidget = new QWidget(m_plotDock); // Container for controls
	m_plotLayout = new QVBoxLayout(m_plotWidget);
	m_plotLayout->setAlignment(Qt::AlignTop); // Keep controls packed at the top

	// --- Y-axis range controls ---
	QGroupBox* yRangeGroup = new QGroupBox("SSB Phase Noise Range");
	QFormLayout* yRangeLayout = new QFormLayout(yRangeGroup);

	m_yMinSpin = new QDoubleSpinBox();
	m_yMinSpin->setRange(Constants::Y_AXIS_MIN, Constants::Y_AXIS_MAX);
	m_yMinSpin->setValue(Constants::Y_AXIS_DEFAULT_MIN);
	m_yMinSpin->setSingleStep(Constants::Y_AXIS_MAJOR_TICK);
	m_yMinSpin->setSuffix(" dBc/Hz");
	connect(m_yMinSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PhaseNoiseAnalyzerApp::updatePlotLimits);
	yRangeLayout->addRow("Min:", m_yMinSpin);

	m_yMaxSpin = new QDoubleSpinBox();
	m_yMaxSpin->setRange(Constants::Y_AXIS_MIN, Constants::Y_AXIS_MAX);
	m_yMaxSpin->setValue(Constants::Y_AXIS_DEFAULT_MAX);
	m_yMaxSpin->setSingleStep(Constants::Y_AXIS_MAJOR_TICK);
	m_yMaxSpin->setSuffix(" dBc/Hz");
	connect(m_yMaxSpin, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, &PhaseNoiseAnalyzerApp::updatePlotLimits);
	yRangeLayout->addRow("Max:", m_yMaxSpin);
	m_plotLayout->addWidget(yRangeGroup);

	// --- Frequency range sliders group ---
	QGroupBox* freqSliderGroup = new QGroupBox("Frequency Offset Range");
	QFormLayout* freqSliderLayout = new QFormLayout(freqSliderGroup);
	m_minFreqSlider = new QSlider(Qt::Horizontal);
	m_minFreqSlider->setRange(0, Constants::FREQ_POINTS.size() - 1);
	m_minFreqSlider->setValue(m_minFreqSliderIndex);
	// Add labels for slider range (optional)
	QHBoxLayout* minFreqLayout = new QHBoxLayout;
	minFreqLayout->addWidget(m_minFreqSlider);
	// minFreqLayout->addWidget(new QLabel(Utils::formatFrequencyValue(Constants::FREQ_POINTS[m_minFreqSliderIndex]))); // Add label if needed
	connect(m_minFreqSlider, &QSlider::valueChanged, this, &PhaseNoiseAnalyzerApp::onMinFreqSliderChanged);
	freqSliderLayout->addRow("Min Freq (Hz):", minFreqLayout);

	m_maxFreqSlider = new QSlider(Qt::Horizontal);
	m_maxFreqSlider->setRange(0, Constants::FREQ_POINTS.size() - 1);
	m_maxFreqSlider->setValue(m_maxFreqSliderIndex);
	QHBoxLayout* maxFreqLayout = new QHBoxLayout;
	maxFreqLayout->addWidget(m_maxFreqSlider);
	// maxFreqLayout->addWidget(new QLabel(Utils::formatFrequencyValue(Constants::FREQ_POINTS[m_maxFreqSliderIndex]))); // Add label if needed
	connect(m_maxFreqSlider, &QSlider::valueChanged, this, &PhaseNoiseAnalyzerApp::onMaxFreqSliderChanged);
	freqSliderLayout->addRow("Max Freq (Hz):", maxFreqLayout);
	m_plotLayout->addWidget(freqSliderGroup);

	// --- Visual controls ---
	QGroupBox* visualGroup = new QGroupBox("Visual Settings");
	QVBoxLayout* visualLayout = new QVBoxLayout(visualGroup);

	m_refCheckbox = new QCheckBox("Show Reference Noise");
	connect(m_refCheckbox, &QCheckBox::stateChanged, this, [this](int state){ toggleReference(state == Qt::Checked); });
	visualLayout->addWidget(m_refCheckbox);

	m_spotCheckbox = new QCheckBox("Show Spot Noise Points");
	connect(m_spotCheckbox, &QCheckBox::stateChanged, this, [this](int state){ toggleSpotNoise(state == Qt::Checked); });
	visualLayout->addWidget(m_spotCheckbox);

	m_spotTableCheckbox = new QCheckBox("Show Spot Noise Table");
	connect(m_spotTableCheckbox, &QCheckBox::stateChanged, this, [this](int state){ toggleSpotNoiseTable(state == Qt::Checked); });
	visualLayout->addWidget(m_spotTableCheckbox);

	m_gridCheckbox = new QCheckBox("Show Grid");
	m_gridCheckbox->setChecked(true); // Default grid on
	connect(m_gridCheckbox, &QCheckBox::stateChanged, this, [this](int state){ toggleGrid(state == Qt::Checked); });
	visualLayout->addWidget(m_gridCheckbox);

	m_darkCheckbox = new QCheckBox("Dark Theme");
	connect(m_darkCheckbox, &QCheckBox::stateChanged, this, [this](int state){ toggleTheme(state == Qt::Checked); });
	visualLayout->addWidget(m_darkCheckbox);

	m_spurRemovalCheckbox = new QCheckBox("Enable Spur Removal");
	connect(m_spurRemovalCheckbox, &QCheckBox::stateChanged, this, [this](int state){ toggleSpurRemoval(state == Qt::Checked); });
	visualLayout->addWidget(m_spurRemovalCheckbox);

	QPushButton* spotColorBtn = new QPushButton("Spot Noise Color"); // Use local var, no member needed
	connect(spotColorBtn, &QPushButton::clicked, this, [this](){ changeLineColor("spot_noise"); });
	visualLayout->addWidget(spotColorBtn);

	m_plotLayout->addWidget(visualGroup);

	// --- Data Filtering group ---
	QGroupBox* filterGroup = new QGroupBox("Data Filtering");
	QVBoxLayout* filterLayout = new QVBoxLayout(filterGroup);

	m_filterCheckbox = new QCheckBox("Enable Filtering"); // Control panel checkbox
	connect(m_filterCheckbox, &QCheckBox::stateChanged, this, [this](int state){ toggleDataFiltering(state == Qt::Checked); });
	filterLayout->addWidget(m_filterCheckbox);

	QFormLayout* filterTypeLayout = new QFormLayout();
	m_filterTypeCombo = new QComboBox();
	m_filterTypeCombo->addItems({"Moving Average", "Median Filter", "Savitzky-Golay"});
	connect(m_filterTypeCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &PhaseNoiseAnalyzerApp::applyDataFiltering);
	filterTypeLayout->addRow("Filter Type:", m_filterTypeCombo);

	m_filterWindowSpin = new QSpinBox();
	m_filterWindowSpin->setRange(Constants::MIN_WINDOW_SIZE, Constants::MAX_WINDOW_SIZE);
	m_filterWindowSpin->setValue(Constants::DEFAULT_WINDOW_SIZE);
	m_filterWindowSpin->setSingleStep(2); // Only odd values
	connect(m_filterWindowSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PhaseNoiseAnalyzerApp::forceOddWindowSize);
	connect(m_filterWindowSpin, QOverload<int>::of(&QSpinBox::valueChanged), this, &PhaseNoiseAnalyzerApp::applyDataFiltering);
	filterTypeLayout->addRow("Window Size:", m_filterWindowSpin);
	filterLayout->addLayout(filterTypeLayout);

	m_plotLayout->addWidget(filterGroup);

	// Add spacer at the bottom
	m_plotLayout->addStretch(1);

	m_plotDock->setWidget(m_plotWidget);
	addDockWidget(Qt::RightDockWidgetArea, m_plotDock);
}

void PhaseNoiseAnalyzerApp::applyTheme()
{
	// Always set Fusion style for consistent cross-platform appearance
	QApplication::setStyle(QStyleFactory::create("Fusion"));

	QPalette palette;

	if (m_useDarkTheme) {
		// Apply dark theme palette
		palette.setColor(QPalette::Window, Constants::DARK_PALETTE_WINDOW);
		palette.setColor(QPalette::WindowText, Constants::DARK_PALETTE_WINDOW_TEXT);
		palette.setColor(QPalette::Base, Constants::DARK_PALETTE_BASE);
		palette.setColor(QPalette::AlternateBase, Constants::DARK_PALETTE_ALT_BASE);
		palette.setColor(QPalette::ToolTipBase, Constants::DARK_PALETTE_TOOLTIP_BASE);
		palette.setColor(QPalette::ToolTipText, Constants::DARK_PALETTE_TOOLTIP_TEXT);
		palette.setColor(QPalette::Text, Constants::DARK_PALETTE_TEXT);
		palette.setColor(QPalette::Button, Constants::DARK_PALETTE_BUTTON);
		palette.setColor(QPalette::ButtonText, Constants::DARK_PALETTE_BUTTON_TEXT);
		palette.setColor(QPalette::BrightText, Constants::DARK_PALETTE_BRIGHT_TEXT);
		palette.setColor(QPalette::Link, Constants::DARK_PALETTE_LINK);
		palette.setColor(QPalette::Highlight, Constants::DARK_PALETTE_HIGHLIGHT);
		palette.setColor(QPalette::HighlightedText, Constants::DARK_PALETTE_HIGHLIGHT_TEXT);

		// Handle disabled state colors for better contrast
		palette.setColor(QPalette::Disabled, QPalette::Text, Constants::DARK_GRID_COLOR);
		palette.setColor(QPalette::Disabled, QPalette::ButtonText, Constants::DARK_GRID_COLOR);

		// Update spot noise color if it's using the light theme default
		if (m_spotNoiseColor == m_defaultSpotNoiseColorLight) {
			m_spotNoiseColor = m_defaultSpotNoiseColorDark;
		}
	} else {
		// Apply light theme palette
#if QT_VERSION < QT_VERSION_CHECK(6, 0, 0)
		// Qt 5 approach
		palette = QApplication::style()->standardPalette();
#else
		// Qt 6 handles themes differently than Qt 5
		// First reset to standard palette to clear any previous customizations
		QApplication::setPalette(QApplication::style()->standardPalette());
		// Then retrieve the fresh standard palette for our light theme modifications
		palette = QApplication::style()->standardPalette();

// Note: Using QGuiApplication::palette() would get the system palette,
// but might include unwanted OS-specific styling
#endif

		// Update spot noise color if it's using the dark theme default
		if (m_spotNoiseColor == m_defaultSpotNoiseColorDark) {
			m_spotNoiseColor = m_defaultSpotNoiseColorLight;
		}
	}

	// Apply the new palette
	QApplication::setPalette(palette);

	// Update plot appearance
	if (m_datasets.isEmpty()) {
		initPlot(); // Re-initialize empty plot with new theme
	} else {
		// Update colors for existing datasets
		for (int i = 0; i < m_datasets.size(); ++i) {
			m_datasets[i].measuredColor = getNextColor(i, m_useDarkTheme);
			m_datasets[i].referenceColor = getNextRefColor(i, m_useDarkTheme);
		}
		updatePlot(); // Re-plot existing data with new theme
	}
}

void PhaseNoiseAnalyzerApp::loadData(const QString& filename)
{
	QFile file(filename);
	if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
		QMessageBox::critical(this, "Error Loading Data", QString("Could not open file: %1").arg(filename));
		qWarning() << "Failed to open file:" << filename << file.errorString();
		return;
	}

	PlotData newDataset;
	newDataset.filename = filename;
	newDataset.displayName = QFileInfo(filename).completeBaseName(); // Use base name for legend
	newDataset.isVisible = true; // Default to visible

	QTextStream in(&file);
	int lineNum = 0;
	bool firstLineCheck = true;
	int columnCount = 0;
	bool currentHasReference = false;

	while (!in.atEnd()) {
		lineNum++;
		QString line = in.readLine().trimmed();
		if (line.isEmpty() || line.startsWith('#') || line.startsWith(';')) {
			continue; // Skip empty lines and comments
		}

		QStringList fields = line.split(QRegularExpression("[,\\s]+"), Qt::SkipEmptyParts); // Split by comma or whitespace in Qt 5.15

		if (firstLineCheck) {
			columnCount = fields.size();
			currentHasReference = (columnCount >= 3); // Assume ref data if 3+ columns
			newDataset.hasReferenceData = currentHasReference; // Store in dataset
			if (currentHasReference) {
				qInfo() << "Detected 3 or more columns, attempting to read reference noise.";
			} else {
				qInfo() << "Detected fewer than 3 columns, reading only frequency and measured noise.";
				// If user requested reference but file doesn't have it
				if (m_plotReferenceDefault) {
					qWarning("Reference noise plotting was enabled, but file has < 3 columns. Disabling.");
					m_plotReferenceDefault = false; // Update the default/initial state
					// Keep checkbox state as user preference, maybe they want to see ref for *other* files
					// m_refCheckbox->setChecked(false); // Update UI checkbox
					m_toggleReferenceAction->setChecked(false); // Update menu action
				}
			}
			firstLineCheck = false;
		}

		if (fields.size() < 2) {
			qWarning() << "Skipping line" << lineNum << ": Not enough data fields (" << fields.size() << ")";
			continue;
		}

		bool okFreq, okNoise, okRef = true;
		double freq = fields[0].toDouble(&okFreq);
		double noise = fields[1].toDouble(&okNoise);
		double ref = std::numeric_limits<double>::quiet_NaN(); // Default to NaN

		if (currentHasReference && fields.size() >= 3) {
			ref = fields[2].toDouble(&okRef);
		} else {
			okRef = true; // Treat as OK if no ref data expected/found
		}

		if (!okFreq || !okNoise || !okRef) {
			qWarning() << "Skipping line" << lineNum << ": Could not parse numeric data -" << fields;
			continue;
		}

		// Add data (ensure frequency is positive for log scale)
		if (freq > 0) {
			newDataset.frequencyOffset.append(freq);
			newDataset.phaseNoise.append(noise);
			if (currentHasReference) {
				newDataset.referenceNoise.append(ref);
			} else {
				newDataset.referenceNoise.append(std::numeric_limits<double>::quiet_NaN()); // Append NaN if no ref data
			}
		} else {
			qWarning() << "Skipping line" << lineNum << ": Frequency offset must be positive for log scale (" << freq << ")";
		}
	}
	file.close();

	if (newDataset.frequencyOffset.isEmpty()) {
		QMessageBox::critical(this, "Error Loading Data", QString("No valid data points found in file: %1").arg(QFileInfo(filename).fileName()));
		qWarning() << "No valid data loaded from" << filename;
		return;
	}

	// Assign colors
	int datasetIndex = m_datasets.size();
	newDataset.measuredColor = getNextColor(datasetIndex, m_useDarkTheme);
	newDataset.referenceColor = getNextRefColor(datasetIndex, m_useDarkTheme);

	// Initialize filtered data for this dataset
	newDataset.phaseNoiseFiltered = newDataset.phaseNoise;
	newDataset.referenceNoiseFiltered = newDataset.referenceNoise;

	m_datasets.append(newDataset);

	qInfo() << "Loaded" << newDataset.frequencyOffset.size() << "data points from" << QFileInfo(filename).fileName();
	m_statusBar->showMessage(QString("Loaded %1 data points from %2").arg(newDataset.frequencyOffset.size()).arg(QFileInfo(filename).fileName()));

	// Adjust frequency range sliders based on data (using the first dataset's range for now)
	if (m_datasets.size() == 1 && !newDataset.frequencyOffset.isEmpty()) {
		double minFreqData = *std::min_element(newDataset.frequencyOffset.constBegin(), newDataset.frequencyOffset.constEnd());
		double maxFreqData = *std::max_element(newDataset.frequencyOffset.constBegin(), newDataset.frequencyOffset.constEnd());

		double viewMinFreq = qMax(Constants::X_AXIS_MIN, minFreqData * 0.9);
		double viewMaxFreq = qMin(Constants::X_AXIS_MAX * 10, maxFreqData * 1.1); // Allow slightly beyond max constant
		if (viewMaxFreq <= viewMinFreq) {
			viewMaxFreq = viewMinFreq * 10;
		}

		m_minFreqSliderIndex = findClosestFreqStepIndex(viewMinFreq);
		m_maxFreqSliderIndex = findClosestFreqStepIndex(viewMaxFreq);

		// Ensure min <= max
		if (m_maxFreqSliderIndex < m_minFreqSliderIndex) {
			m_maxFreqSliderIndex = qMin(m_minFreqSliderIndex + 1, Constants::FREQ_POINTS.size() - 1);
			if (m_minFreqSliderIndex > m_maxFreqSliderIndex) { // Still crossed if min was last index
				m_minFreqSliderIndex = qMax(0, m_maxFreqSliderIndex - 1);
			}
		}

		// Block signals while setting slider values to prevent premature updates
		m_minFreqSlider->blockSignals(true);
		m_maxFreqSlider->blockSignals(true);
		m_minFreqSlider->setValue(m_minFreqSliderIndex);
		m_maxFreqSlider->setValue(m_maxFreqSliderIndex);
		m_minFreqSlider->blockSignals(false);
		m_maxFreqSlider->blockSignals(false);

		qInfo() << "Adjusted frequency range sliders based on data from" << QFileInfo(filename).fileName();
	}

	// Update UI components
	updatePlot(); // Update plot with new data

	// Update window title based on loaded files
	if (m_datasets.size() == 1) {
		setWindowTitle(QString("Phase Noise Analyzer - %1").arg(QFileInfo(filename).fileName()));
	} else {
		setWindowTitle(QString("Phase Noise Analyzer - %1 Files").arg(m_datasets.size()));
	}
	// Set default output filename based on the *first* input file loaded
	if (m_datasets.size() == 1) {
		QFileInfo fileInfo(filename);
		m_outputFilename = fileInfo.path() + "/" + fileInfo.completeBaseName() + ".png";
	}
}

int PhaseNoiseAnalyzerApp::findClosestFreqStepIndex(double freq) {
	if (Constants::FREQ_POINTS.isEmpty()) return 0;

	// Find the lower bound index
	auto it = std::lower_bound(Constants::FREQ_POINTS.constBegin(), Constants::FREQ_POINTS.constEnd(), freq);
	int idx = std::distance(Constants::FREQ_POINTS.constBegin(), it);

	// Handle edge cases
	if (idx == 0) return 0;
	if (idx == Constants::FREQ_POINTS.size()) return Constants::FREQ_POINTS.size() - 1;

	// Check which neighbor is closer
	if (qFabs(Constants::FREQ_POINTS[idx] - freq) < qFabs(Constants::FREQ_POINTS[idx - 1] - freq)) {
		return idx;
	} else {
		return idx - 1;
	}
}

void PhaseNoiseAnalyzerApp::onMinFreqSliderChanged(int value) {
	if (value >= m_maxFreqSliderIndex) {
		// If slider tries to go >= max, block it just below max
		m_minFreqSlider->blockSignals(true);
		m_minFreqSlider->setValue(qMax(0, m_maxFreqSliderIndex - 1));
		m_minFreqSlider->blockSignals(false);
		m_minFreqSliderIndex = m_minFreqSlider->value(); // Update index
		// Don't update plot yet, let the corrected value signal trigger it if needed
		return;
	}
	m_minFreqSliderIndex = value;
	updatePlot();
}

void PhaseNoiseAnalyzerApp::onMaxFreqSliderChanged(int value) {
	if (value <= m_minFreqSliderIndex) {
		// If slider tries to go <= min, block it just above min
		m_maxFreqSlider->blockSignals(true);
		m_maxFreqSlider->setValue(qMin(Constants::FREQ_POINTS.size() - 1, m_minFreqSliderIndex + 1));
		m_maxFreqSlider->blockSignals(false);
		m_maxFreqSliderIndex = m_maxFreqSlider->value(); // Update index
		// Don't update plot yet
		return;
	}
	m_maxFreqSliderIndex = value;
	updatePlot();
}

void PhaseNoiseAnalyzerApp::updatePlotLimits() {
	if (!m_plot || m_datasets.isEmpty()) return;

	double yMin = m_yMinSpin->value();
	double yMax = m_yMaxSpin->value();

	// Ensure min < max
	if (yMin >= yMax) {
		if (sender() == m_yMinSpin) {
			yMax = yMin + Constants::Y_AXIS_MAJOR_TICK;
			m_yMaxSpin->blockSignals(true); // Prevent infinite loop
			m_yMaxSpin->setValue(yMax);
			m_yMaxSpin->blockSignals(false);
		} else {
			yMin = yMax - Constants::Y_AXIS_MAJOR_TICK;
			m_yMinSpin->blockSignals(true);
			m_yMinSpin->setValue(yMin);
			m_yMinSpin->blockSignals(false);
		}
	}

	m_plot->yAxis->setRange(yMin, yMax);
	m_plot->yAxis2->setRange(yMin, yMax);
	m_plot->replot();
}


void PhaseNoiseAnalyzerApp::forceOddWindowSize(int value) {
	// This slot is connected to valueChanged. Ensure it becomes odd.
	if (m_filterWindowSpin) { // Check pointer validity
		if (value % 2 == 0) {
			// Block signals to prevent re-triggering applyDataFiltering immediately
			m_filterWindowSpin->blockSignals(true);
			m_filterWindowSpin->setValue(value + 1);
			m_filterWindowSpin->blockSignals(false);
		}
		// If applyDataFiltering is connected to valueChanged, it will trigger
		// after this function returns if the value was changed.
		// applyDataFiltering(); // Call explicitly if needed, or rely on signal connection
	}
}

// --- Action Toggles ---

void PhaseNoiseAnalyzerApp::toggleTheme(bool checked) {
	// Determine the new state based on the source (checkbox or menu/toolbar action)
	if (sender() == m_darkCheckbox || sender() == m_toggleDarkThemeAction) {
		m_useDarkTheme = checked; // Use the checked state directly
	} else { // Assume toggle button click (m_tbThemeAction)
		m_useDarkTheme = !m_useDarkTheme;
	}

	// Synchronize UI elements
	m_darkCheckbox->setChecked(m_useDarkTheme);
	m_toggleDarkThemeAction->setChecked(m_useDarkTheme);
	// Toolbar action doesn't need check state managed here

	applyTheme(); // Apply the theme change
}

void PhaseNoiseAnalyzerApp::toggleReference(bool checked) {
	bool newState;
	if (sender() == m_refCheckbox || sender() == m_toggleReferenceAction) {
		newState = checked;
	} else {
		newState = !m_toggleReferenceAction->isChecked(); // Toggle based on action state if called programmatically
	}

	// Check if *any* dataset has reference data before warning
	bool anyHasRef = false;
	for(const auto& data : m_datasets) { if (data.hasReferenceData) { anyHasRef = true; break; } }

	if (!anyHasRef && newState) {
		//QMessageBox::warning(this, "No Reference Data", "Cannot show reference noise because no valid reference data was found in the loaded file.");
		//m_refCheckbox->setChecked(false); // Keep UI consistent
		//m_toggleReferenceAction->setChecked(false);
		// Allow toggling even if no ref data, plot just won't show anything
		// return; // Don't proceed
	}

	// Update state and UI elements
	m_refCheckbox->setChecked(newState);
	m_toggleReferenceAction->setChecked(newState);

	// Update plot immediately
	updatePlot();
}

void PhaseNoiseAnalyzerApp::toggleSpotNoise(bool checked) {
	if (sender() == m_spotCheckbox || sender() == m_toggleSpotNoiseAction) {
		m_showSpotNoise = checked;
	} else {
		m_showSpotNoise = !m_showSpotNoise;
	}
	m_spotCheckbox->setChecked(m_showSpotNoise);
	m_toggleSpotNoiseAction->setChecked(m_showSpotNoise);
	updatePlot();
}

void PhaseNoiseAnalyzerApp::toggleSpotNoiseTable(bool checked) {
	if (sender() == m_spotTableCheckbox || sender() == m_toggleSpotNoiseTableAction) {
		m_showSpotNoiseTable = checked;
	} else {
		m_showSpotNoiseTable = !m_showSpotNoiseTable;
	}
	m_spotTableCheckbox->setChecked(m_showSpotNoiseTable);
	m_toggleSpotNoiseTableAction->setChecked(m_showSpotNoiseTable);
	updatePlot();
}

void PhaseNoiseAnalyzerApp::toggleGrid(bool checked) {
	bool showGrid = checked; // Directly use the state from checkbox signal

	if(m_plot) {
		m_plot->xAxis->grid()->setVisible(showGrid);
		m_plot->yAxis->grid()->setVisible(showGrid);
		m_plot->xAxis->grid()->setSubGridVisible(showGrid);
		m_plot->yAxis->grid()->setSubGridVisible(showGrid);
		m_plot->replot();
	}
}

void PhaseNoiseAnalyzerApp::toggleCrosshair(bool checked) {
	// Determine new state based on sender or direct call
	if (sender() == m_crosshairAction || sender() == m_tbCrosshairAction) {
		m_useCrosshair = checked;
	} else {
		m_useCrosshair = checked; // Assume direct call sets the state
	}

	// Sync UI
	if (m_crosshairAction) m_crosshairAction->setChecked(m_useCrosshair);
	if (m_tbCrosshairAction) m_tbCrosshairAction->setChecked(m_useCrosshair);

	if (m_useCrosshair) {
		// --- Disable other exclusive tools ---
		if (m_measureMode) {
			toggleMeasurementTool(false);
		}
		if (m_activeTool == ActiveTool::PanZoom) {
			m_activeTool = ActiveTool::None;
			if(m_panzoomButton) m_panzoomButton->setChecked(false);
			// Optionally restore default interactions or just disable panzoom interaction flags
			m_plot->setInteraction(QCP::iRangeDrag, false);
			m_plot->setInteraction(QCP::iRangeZoom, false);
		}
		// Restore selection possibility if needed when crosshair is on
		// m_plot->setInteraction(QCP::iSelectPlottables, true);
		// ... other selections ...

	} else {
		// Remove crosshair visuals immediately
		if (m_cursorAnnotation) {
			m_plot->removeItem(m_cursorAnnotation);
			m_cursorAnnotation = nullptr;
		}
		if (m_cursorTracer) {
			m_plot->removeItem(m_cursorTracer);
			m_cursorTracer = nullptr;
		}
		m_plot->replot();
		// If no other tool is active, restore default interactions
		if (m_activeTool == ActiveTool::None && !m_measureMode) {
			m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iSelectItems | QCP::iSelectLegend | QCP::iSelectAxes | QCP::iSelectOther);
		}
	}
}

void PhaseNoiseAnalyzerApp::toggleMeasurementTool(bool checked) {
	// Determine new state based on sender or direct call
	if (sender() == m_measureAction || sender() == m_tbMeasureAction) {
		m_measureMode = checked;
	} else {
		m_measureMode = checked; // Assume direct call sets the state
	}

	// Sync UI
	if(m_measureAction) m_measureAction->setChecked(m_measureMode);
	if(m_tbMeasureAction) m_tbMeasureAction->setChecked(m_measureMode);

	m_measureStartPoint = QPointF(); // Reset start point

	if (m_measureMode) {
		// --- Disable other exclusive tools ---
		if (m_useCrosshair) {
			toggleCrosshair(false);
		}
		if (m_activeTool == ActiveTool::PanZoom) {
			m_activeTool = ActiveTool::None;
			if(m_panzoomButton) m_panzoomButton->setChecked(false);
			m_plot->setInteraction(QCP::iRangeDrag, false);
			m_plot->setInteraction(QCP::iRangeZoom, false);
		}
		// Disable QCP interactions while measuring? Maybe not necessary if mousePress handles it.
		// m_plot->setInteractions(QCP::iNone);

	} else {
		// Remove measurement visuals immediately
		for (QCPAbstractItem* item : std::as_const(m_measurementItems)) {
			m_plot->removeItem(item);
		}
		m_measurementItems.clear();
		if (m_measurementText) {
			m_plot->removeItem(m_measurementText);
			m_measurementText = nullptr;
		}
		m_plot->replot();
		// If no other tool is active, restore default interactions
		if (m_activeTool == ActiveTool::None && !m_useCrosshair) {
			m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iSelectItems | QCP::iSelectLegend | QCP::iSelectAxes | QCP::iSelectOther);
		}
	}
}

void PhaseNoiseAnalyzerApp::toggleDataFiltering(bool checked) {
	if (sender() == m_filterCheckbox || sender() == m_filterAction || sender() == m_tbFilterAction) {
		m_filteringEnabled = checked;
	} else {
		m_filteringEnabled = !m_filteringEnabled;
	}

	// Sync UI
	m_filterCheckbox->setChecked(m_filteringEnabled);
	m_filterAction->setChecked(m_filteringEnabled);
	m_tbFilterAction->setChecked(m_filteringEnabled);

	// Enable/disable filter controls
	m_filterTypeCombo->setEnabled(m_filteringEnabled);
	m_filterWindowSpin->setEnabled(m_filteringEnabled);

	// Re-apply filtering or revert to original data
	if (m_filteringEnabled) {
		applyDataFiltering(); // This will call updatePlot
	} else {
		updatePlot(); // Plot original data
	}
}

void PhaseNoiseAnalyzerApp::toggleSpurRemoval(bool checked) {
	// Check if *any* dataset has reference data before allowing spur removal
	bool anyHasRef = false;
	for(const auto& data : m_datasets) { if (data.hasReferenceData) { anyHasRef = true; break; } }

	if (!anyHasRef && checked) {
		QMessageBox::warning(this, "Spur Removal Unavailable", "Spur removal requires reference noise data, which was not found in any loaded file.");
		// Ensure checkboxes/actions remain unchecked
		m_spurRemovalCheckbox->setChecked(false);
		m_tbSpurRemovalAction->setChecked(false);
		m_spurRemovalAction->setChecked(false); // Menu action
		return;
	}

	if (sender() == m_spurRemovalCheckbox || sender() == m_tbSpurRemovalAction || sender() == m_spurRemovalAction) {
		m_spurRemovalEnabled = checked;
	} else {
		m_spurRemovalEnabled = !m_spurRemovalEnabled;
	}

	// Sync UI
	m_spurRemovalCheckbox->setChecked(m_spurRemovalEnabled);
	m_tbSpurRemovalAction->setChecked(m_spurRemovalEnabled);
	m_spurRemovalAction->setChecked(m_spurRemovalEnabled);

	// Update plot to apply/remove spur effect
	updatePlot();
}

// --- Filtering and Spur Removal Logic ---

void PhaseNoiseAnalyzerApp::applyDataFiltering()
{
	// Only apply if filtering is actually enabled and data exists
	if (!m_filteringEnabled || m_datasets.isEmpty()) {
		// If called when disabled, just ensure plot uses original data
		if (!m_filteringEnabled) updatePlot();
		return;
	}

	QString filterType = m_filterTypeCombo->currentText();
	int window = m_filterWindowSpin->value();

	try {
		for (PlotData& data : m_datasets) {
			if (data.frequencyOffset.isEmpty()) continue; // Skip empty datasets

			if (filterType == "Moving Average") {
				data.phaseNoiseFiltered = Utils::movingAverage(data.phaseNoise, window);
				if (data.hasReferenceData) data.referenceNoiseFiltered = Utils::movingAverage(data.referenceNoise, window);
			} else if (filterType == "Median Filter") {
				data.phaseNoiseFiltered = Utils::medianFilter(data.phaseNoise, window);
				if (data.hasReferenceData) data.referenceNoiseFiltered = Utils::medianFilter(data.referenceNoise, window);
			} else if (filterType == "Savitzky-Golay") {
				data.phaseNoiseFiltered = Utils::savitzkyGolay(data.phaseNoise, window);
				if (data.hasReferenceData) data.referenceNoiseFiltered = Utils::savitzkyGolay(data.referenceNoise, window);
			} else {
				// Should not happen, revert to original if type is unknown
				data.phaseNoiseFiltered = data.phaseNoise;
				data.referenceNoiseFiltered = data.referenceNoise;
			}
		}

		updatePlot();
		m_statusBar->showMessage(QString("Applied %1 filter (window=%2)").arg(filterType).arg(window));
		qInfo() << "Applied filter:" << filterType << "with window" << window;

	} catch (const std::exception& e) {
		QMessageBox::warning(this, "Filtering Error", QString("Error applying filter: %1").arg(e.what()));
		qWarning() << "Filtering error:" << e.what();
		// Disable filtering on error
		toggleDataFiltering(false);
	} catch (...) {
		QMessageBox::warning(this, "Filtering Error", "An unknown error occurred during filtering.");
		qWarning() << "Unknown filtering error occurred.";
		toggleDataFiltering(false);
	}
}

// Apply Spur Removal
void PhaseNoiseAnalyzerApp::applySpurRemoval() {
	// This function modifies phaseNoiseFiltered for each dataset based on its referenceNoiseFiltered
	// It assumes applyDataFiltering (if enabled) has already populated the filtered vectors.
	// If filtering is disabled, it should operate on copies of the original data.

	// Check overall enabling flag first
	if (!m_spurRemovalEnabled) {
		// If spur removal just got disabled, ensure filtered matches source (original or filtered)
		for (PlotData& data : m_datasets) {
			if (m_filteringEnabled) { /* Do nothing, keep filtered */ }
			else { data.phaseNoiseFiltered = data.phaseNoise; data.referenceNoiseFiltered = data.referenceNoise; }
		}
		return;
	}

	for (PlotData& data : m_datasets) {
		if (!data.hasReferenceData || data.frequencyOffset.isEmpty()) {
			// If not enabled or no reference data, ensure filtered data matches source
			if (m_filteringEnabled) { /* Keep filtered */ }
			else { data.phaseNoiseFiltered = data.phaseNoise; data.referenceNoiseFiltered = data.referenceNoise; }
			continue; // Skip this dataset
		}

		// Determine source data: Use already filtered data if filtering is ON, else use original.
		const QVector<double>& sourceRef = m_filteringEnabled ? data.referenceNoiseFiltered : data.referenceNoise;
		const QVector<double>& sourceMeas = m_filteringEnabled ? data.phaseNoiseFiltered : data.phaseNoise; // Start point for measurement

		// Work on a copy of the measurement data that will become the new filtered measurement data
		QVector<double> processedMeas = sourceMeas;

		int N = sourceRef.size();
		if (N < 3) {
			data.phaseNoiseFiltered = processedMeas; // Not enough data to process
			continue; // Skip this dataset
		}

		// --- Method 1: Baseline comparison ---
		QVector<double> baseline = Utils::rollingMedian(sourceRef, Constants::DEFAULT_SPUR_WINDOW_SIZE);
		QVector<bool> isSpur(N, false);
		for(int i=0; i<N; ++i) {
			if (!std::isnan(sourceRef[i]) && !std::isnan(baseline[i]) &&
				(sourceRef[i] - baseline[i]) > Constants::SPUR_THRESHOLD) {
				isSpur[i] = true;
			}
		}

		int i = 0;
		while (i < N) {
			if (isSpur[i]) {
				int start = i;
				while (i < N && isSpur[i]) {
					i++;
				}
				int end = i - 1; // Inclusive end index of spur segment

				// Find valid neighbors for interpolation
				int left = start - 1;
				while (left >= 0 && isSpur[left]) left--; // Find first non-spur to the left
				if (left < 0) left = 0; // Clamp to beginning if needed

				int right = end + 1;
				while (right < N && isSpur[right]) right++; // Find first non-spur to the right
				if (right >= N) right = N - 1; // Clamp to end if needed

				double leftVal = processedMeas[left];
				double rightVal = processedMeas[right];
				double leftFreq = data.frequencyOffset[left];
				double rightFreq = data.frequencyOffset[right];


				// Interpolate over the segment [start, end] using neighbors [left, right]
				for (int j = start; j <= end; ++j) {
					// Check if neighbors are distinct to avoid division by zero
					if (right > left && qAbs(rightFreq - leftFreq) > 1e-9) { // Check freq diff
						processedMeas[j] = Utils::linearInterpolate(leftFreq, leftVal, rightFreq, rightVal, data.frequencyOffset[j]);
					} else {
						// If neighbors are the same point or too close, just use the left value
						processedMeas[j] = leftVal;
					}
				}
				// Continue search after the processed segment ('i' is already advanced)
			} else {
				i++; // Move to next point if not a spur
			}
		}

		// --- Method 2: Edge Detection (applied *after* baseline method) ---
		// Use the intermediate processed measurement and original reference for detection
		const QVector<double>& currentRef = sourceRef; // Use original/filtered ref for detection edges
		QVector<double> finalMeas = processedMeas; // Operate on the result of method 1

		i = 1;
		while (i < N - 1) {
			// Check for rising edge in reference noise
			if (!std::isnan(currentRef[i]) && !std::isnan(currentRef[i-1]) &&
				(currentRef[i] - currentRef[i - 1]) > Constants::SPUR_THRESHOLD)
			{
				int start = i; // Start of potential spur region
				int j = start + 1;
				// Find the corresponding falling edge
				while (j < N) {
					if (!std::isnan(currentRef[j]) && !std::isnan(currentRef[j-1]) &&
						(currentRef[j-1] - currentRef[j]) > Constants::SPUR_THRESHOLD)
					{
						break; // Found falling edge
					}
					j++;
				}

				if (j < N) { // Found a falling edge at index j
					// Interpolate measured data from point start-1 to j
					double leftVal = finalMeas[start - 1];
					double rightVal = finalMeas[j];
					double leftFreq = data.frequencyOffset[start-1];
					double rightFreq = data.frequencyOffset[j];

					for (int k = start; k < j; ++k) { // Interpolate up to (but not including) the end point j
						if (qFabs(rightFreq-leftFreq) > 1e-9) { // Avoid division by zero
							finalMeas[k] = Utils::linearInterpolate(leftFreq, leftVal, rightFreq, rightVal, data.frequencyOffset[k]);
						} else {
							finalMeas[k] = leftVal; // Assign left value if frequencies are too close
						}
					}
					i = j; // Continue search after the falling edge
				} else {
					// No falling edge found, extend left value to the end
					double leftVal = finalMeas[start - 1];
					for (int k = start; k < N; ++k) {
						finalMeas[k] = leftVal;
					}
					i = N; // End the loop
				}
			} else {
				i++; // Move to the next point
			}
		}

		// Update the filtered phase noise data for this dataset
		data.phaseNoiseFiltered = finalMeas;

		// Ensure reference is also set correctly in filtered data (freq offset is never filtered)
		if (!m_filteringEnabled) { // If filtering was off, copy original ref
			data.referenceNoiseFiltered = data.referenceNoise;
		} // If filtering was ON, ref is already filtered
	} // end for each dataset

	m_statusBar->showMessage("Spur removal applied");
	qInfo() << "Spur removal applied.";
}

void PhaseNoiseAnalyzerApp::calculateSpotNoise()
{
	m_spotNoiseData.clear();
	if (m_datasets.isEmpty()) return;

	// Find the *first visible* dataset for spot noise calculation
	const PlotData* firstVisibleData = nullptr;
	for(const auto& data : m_datasets) {
		if (data.isVisible && data.graphMeasured && !data.frequencyOffset.isEmpty()) {
			firstVisibleData = &data;
			break;
		}
	}

	if (!firstVisibleData) {
		qWarning() << "calculateSpotNoise: No visible dataset found to calculate spot noise from.";
		return; // No visible data to calculate from
	}

	// Use the data currently plotted in the first visible measured graph
	QSharedPointer<QCPGraphDataContainer> dataContainer = firstVisibleData->graphMeasured->data();

	// Get current view range
	double xMinView = m_plot->xAxis->range().lower;
	double xMaxView = m_plot->xAxis->range().upper;

	for (const auto& freqInfo : Constants::FREQ_POINT_INFOS) {
		double targetFreq = freqInfo.value;

		// Check if frequency is within the current view range
		if (targetFreq < xMinView || targetFreq > xMaxView) {
			continue;
		}

		// Find the closest data point in the *measured graph's data* to the target frequency
		double minDist = std::numeric_limits<double>::max();
		double closestFreq = std::numeric_limits<double>::quiet_NaN();
		double closestNoise = std::numeric_limits<double>::quiet_NaN();
		bool found = false;

		// Iterate through the graph data to find the closest point
		for (auto it = dataContainer->constBegin(); it != dataContainer->constEnd(); ++it) {
			double currentFreq = it->key; // Frequency (x-axis)
			double dist = qAbs(qLn(currentFreq) - qLn(targetFreq)); // Use log distance for better search on log scale

			if (dist < minDist) {
				minDist = dist;
				closestFreq = currentFreq;
				closestNoise = it->value; // Noise (y-axis)
				found = true;
			}
		}

		if (found) {
			// Check if the found frequency is reasonably close (e.g., within half a decade)
			if (qFabs(qLn(closestFreq) - qLn(targetFreq)) < qLn(5.0)) // Within factor of 5
			{
				m_spotNoiseData[freqInfo.displayName] = qMakePair(closestFreq, closestNoise);
			} else {
				qWarning() << "Spot noise target" << targetFreq << "Hz - closest data point" << closestFreq << "Hz is too far, skipping.";
			}
		}
	}
	qInfo() << "Calculated" << m_spotNoiseData.size() << "spot noise points.";
}

void PhaseNoiseAnalyzerApp::addSpotNoiseTable()
{
	// --- Cleanup ---
	if (m_spotNoiseTableText) {
		if (m_plot) m_plot->removeItem(m_spotNoiseTableText);
		m_spotNoiseTableText = nullptr;
	}

	// --- Conditions to Show ---
	bool shouldShow = m_plot && m_showSpotNoiseTable && !m_spotNoiseData.isEmpty();
	if (!shouldShow) {
		if (m_plot) m_plot->replot(QCustomPlot::rpQueuedReplot);
		return;
	}

	// --- Sort Data ---
	QVector<QPair<double, QString>> sortedPoints;
	for(auto it = m_spotNoiseData.constBegin(); it != m_spotNoiseData.constEnd(); ++it) {
		double targetFreq = Constants::FREQ_DISPLAY_TO_VALUE.value(it.key());
		sortedPoints.append(qMakePair(targetFreq, it.key()));
	}
	std::sort(sortedPoints.begin(), sortedPoints.end(),
			  [](const QPair<double, QString>& a, const QPair<double, QString>& b) {
				  return a.first < b.first;
			  });

	// --- Analyze data for formatting decisions ---
	int maxFreqLength = 0;
	int maxValueWidth = 0;  // Track the total width needed for value display

	// Find maximum lengths for alignment
	for (const auto& pair : sortedPoints) {
		QString formattedLabel = Constants::FREQ_DISPLAY_TO_FORMATTED.value(pair.second, pair.second);
		maxFreqLength = qMax(maxFreqLength, formattedLabel.length());

		// For values, determine total width needed including sign and decimals
		double noiseValue = m_spotNoiseData[pair.second].second;
		QString valueStr = QString::number(noiseValue, 'f', 2);  // Include decimals
		maxValueWidth = qMax(maxValueWidth, valueStr.length());
	}

	// --- Build table lines with precise formatting ---
	QStringList lines;

	// Title line (will be centered later)
	lines.append("Spot Noise");

	// Data lines with exact alignment
	for (const auto& pair : sortedPoints) {
		QString displayName = pair.second;
		QString formattedLabel = Constants::FREQ_DISPLAY_TO_FORMATTED.value(displayName, displayName);
		double noiseValue = m_spotNoiseData[displayName].second;

		// Right-align frequency value
		QString freqPart = formattedLabel;
		while (freqPart.length() < maxFreqLength) {
			freqPart = " " + freqPart;  // Right-align by adding spaces to the left
		}

		// Format the value with consistent alignment of negative signs
		QString valueStr = QString::number(noiseValue, 'f', 2);

		// Ensure consistent width for values column
		// For negative values, the sign is already included in valueStr
		// For positive values, we add a space to maintain alignment
		if (!valueStr.startsWith('-')) {
			valueStr = " " + valueStr;
		}

		// Make sure all value strings have the same width
		while (valueStr.length() < maxValueWidth + 1) { // +1 for the space we added to positive numbers
			valueStr = " " + valueStr;
		}

		// Combine parts with fixed spacing
		QString line = freqPart + " :" + valueStr + " dBc/Hz";
		lines.append(line);
	}

	// --- Calculate the maximum line width for centering the title ---
	int maxLineWidth = 0;
	for (const QString& line : lines) {
		maxLineWidth = qMax(maxLineWidth, line.length());
	}

	// Center the title
	if (!lines.isEmpty()) {
		QString title = lines[0];
		int padding = (maxLineWidth - title.length()) / 2;
		QString spaces(padding, ' ');
		lines[0] = spaces + title;
	}

	// Join all lines with newlines
	QString tableText = lines.join("\n");

	// --- Create or Get QCPItemText ---
	if (!m_spotNoiseTableText) {
		m_spotNoiseTableText = new QCPItemText(m_plot);

		QCPLayer *tableLayer = m_plot->layer("overlay");
		if (!tableLayer) tableLayer = m_plot->layer("legend");
		if (!tableLayer) tableLayer = m_plot->layer("main");
		if (tableLayer) m_spotNoiseTableText->setLayer(tableLayer);

		m_spotNoiseTableText->setClipToAxisRect(false);
		m_spotNoiseTableText->setSelectable(false);
		m_spotNoiseTableText->setPadding(QMargins(8, 8, 8, 8));
	}

	// Update appearance - Use monospace font to ensure column alignment
	m_spotNoiseTableText->setText(tableText);
	m_spotNoiseTableText->setFont(QFont("Liberation Mono", 9));
	m_spotNoiseTableText->setColor(m_textColor);
	m_spotNoiseTableText->setPen(QPen(m_tickLabelColor));
	m_spotNoiseTableText->setBrush(QBrush(m_annotationBgColor));
	// Using left alignment to preserve our manual spacing
	m_spotNoiseTableText->setTextAlignment(Qt::AlignLeft);
	m_spotNoiseTableText->setPositionAlignment(Qt::AlignTop | Qt::AlignRight);
	m_spotNoiseTableText->setVisible(true);

	// Request replot
	if (m_plot) m_plot->replot(QCustomPlot::rpQueuedReplot);
}

void PhaseNoiseAnalyzerApp::positionSpotNoiseTable()
{
	// Check if everything needed exists and the table should be visible
	if (!m_plot || !m_plot->legend || !m_spotNoiseTableText || !m_spotNoiseTableText->visible()) {
		return;
	}

	// --- Positioning Logic ---
	// Place the spot noise table below the legend
	const int verticalSpacing = 5;   // Pixels below legend
	const int horizontalOffset = 132;

	// Get legend geometry IN PIXELS relative to the QCustomPlot widget
	QRect legendRect = m_plot->legend->outerRect();

	// Calculate desired position for the spot noise table IN PIXELS
	// Using bottom-right of legend as reference + offsets
	QPointF targetPixelPos = legendRect.bottomLeft() + QPointF(horizontalOffset, verticalSpacing);

	// --- Apply Position to the Item ---
	m_spotNoiseTableText->position->setType(QCPItemPosition::ptAbsolute);
	m_spotNoiseTableText->position->setPixelPosition(targetPixelPos);
}

// --- Color Change ---
void PhaseNoiseAnalyzerApp::changeLineColor(const QString& lineType) {
	// Simplified: Only allow changing spot noise color
	// Changing individual trace colors would require a more complex UI
	QColor initialColor;
	QString title;

	if (lineType == "spot_noise") {
		initialColor = m_spotNoiseColor;
		title = "Select Spot Noise Color";
	} else {
		qWarning() << "Unsupported line type for color change:" << lineType;
		return;
	}

	QColor newColor = QColorDialog::getColor(initialColor, this, title);
	if (newColor.isValid()) {
		if (lineType == "spot_noise") {
			m_spotNoiseColor = newColor;
		}
		updatePlot(); // Redraw with new color
	}
}

// --- Toolbar Navigation Actions ---

void PhaseNoiseAnalyzerApp::homeView() {
	if (!m_plot) return;
	// Rescale axes based on currently plotted data
	bool first = true;
	for (const auto& data : m_datasets) {
		if (!data.isVisible) continue;
		if (data.graphMeasured) data.graphMeasured->rescaleAxes(first); // Rescale based on first visible measured data
		if (data.graphReference) data.graphReference->rescaleAxes(false); // Add ref data to range, don't overwrite
		first = false;
	}

	// Optionally restore default Y range or slider range if needed
	/*
	m_yMinSpin->setValue(Constants::Y_AXIS_DEFAULT_MIN);
	m_yMaxSpin->setValue(Constants::Y_AXIS_DEFAULT_MAX);
	m_minFreqSlider->setValue(0); // Or original data-based index
	m_maxFreqSlider->setValue(Constants::FREQ_POINTS.size() - 1); // Or original data-based index
	*/

	// Update internal state and plot
	// m_minFreqSliderIndex = m_minFreqSlider->value(); // Sliders might not reflect overall data range now
	// m_maxFreqSliderIndex = m_maxFreqSlider->value();
	updatePlot(); // Re-apply ranges and redraw

	// Reset tool state
	m_activeTool = ActiveTool::None;
	m_panzoomButton->setChecked(false);
	m_plot->setInteraction(QCP::iRangeDrag, false); // Disable explicit drag/zoom
	m_plot->setInteraction(QCP::iRangeZoom, false);
	m_plot->setInteractions(QCP::iSelectPlottables); // Allow selection only

	m_statusBar->showMessage("View reset to default.");
}

void PhaseNoiseAnalyzerApp::panzoomButtonClicked(bool checked) {
	if (!m_plot) return;

	if (checked) { // Enable PanZoom
		// Deactivate other tools
		if (m_useCrosshair) {
			toggleCrosshair(false); // Disable crosshair tool and update UI
		}
		if (m_measureMode) {
			toggleMeasurementTool(false); // Disable measure tool and update UI
		}

		// Set panzoom as the active tool
		m_activeTool = ActiveTool::PanZoom;

		// Configure QCustomPlot interactions specifically for panning/dragging
		m_plot->setInteraction(QCP::iRangeDrag, true);   // **** Enable dragging ****
		m_plot->setInteraction(QCP::iRangeZoom, true);   // **** Enable wheel/right-mouse zoom **** (needed for pan mode zoom)
		m_plot->setInteraction(QCP::iSelectItems, false);       // Disable item selection
		m_plot->setInteraction(QCP::iSelectPlottables, false); // Disable plottable selection
		m_plot->setInteraction(QCP::iSelectAxes, false);       // Disable axis selection
		m_plot->setInteraction(QCP::iSelectLegend, false);     // Disable legend selection
		m_plot->setInteraction(QCP::iSelectOther, false);      // Disable other selection

		// Configure axisRect specifically for pan behavior
		// Allow dragging on both axes
		m_plot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical);
		// Allow zooming via wheel/right-mouse drag on both axes
		m_plot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical);

		m_statusBar->showMessage("PanZoom mode enabled (Left-drag to pan, Right-drag/Wheel to zoom)");

	} else { // Disable PanZoom (if it was the active tool)
		if (m_activeTool == ActiveTool::PanZoom) {
			m_activeTool = ActiveTool::None;
			// Restore default interactions (or enable selection if desired)
			m_plot->setInteractions(QCP::iRangeDrag | QCP::iRangeZoom | QCP::iSelectPlottables | QCP::iSelectItems | QCP::iSelectLegend | QCP::iSelectAxes | QCP::iSelectOther);
			m_plot->axisRect()->setRangeDrag(Qt::Horizontal | Qt::Vertical); // Re-enable default dragging
			m_plot->axisRect()->setRangeZoom(Qt::Horizontal | Qt::Vertical); // Re-enable default zooming

			m_statusBar->showMessage("PanZoom mode disabled.");
		}
	}
}

void PhaseNoiseAnalyzerApp::configureSubplots() {
	// QCustomPlot doesn't have a direct subplot configuration tool like matplotlib.
	// Re-purpose this button to reset the view (same as Home).
	homeView();
	QMessageBox::information(this, "Configure Subplots", "Plot view has been reset to default.");
}

// Handle clicks on legend items to toggle visibility using strikethrough
void PhaseNoiseAnalyzerApp::onLegendItemClicked(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event)
{
	Q_UNUSED(legend)
	Q_UNUSED(event)
	if (!item || !m_plot || !m_plot->legend) return; // Safety checks

	QCPPlottableLegendItem *plItem = qobject_cast<QCPPlottableLegendItem*>(item);
	if (!plItem) return; // Only handle plottable items for now

	int datasetIndex = -1;
	// Find the dataset associated with this plottable's legend item
	for (int i = 0; i < m_datasets.size(); ++i) {
		// Check both measured and reference graphs, as either could be the legend item clicked
		if (m_datasets[i].graphMeasured == plItem->plottable() || m_datasets[i].graphReference == plItem->plottable()) {
			datasetIndex = i;
			break;
		}
	}

	if (datasetIndex != -1) {
		PlotData& data = m_datasets[datasetIndex]; // Use reference
		data.isVisible = !data.isVisible; // Toggle visibility flag
		qDebug() << "Toggled visibility for" << data.displayName << "to" << data.isVisible;

		// --- Update appearance of ALL legend items associated with this dataset ---
		// (This will be handled within updatePlot now)

		updatePlot(); // Redraw the plot (will show/hide graphs based on isVisible and update legend appearance)
	}
}

// Slot to show context menu on the plot
void PhaseNoiseAnalyzerApp::showPlotContextMenu(const QPoint &pos)
{
	if (!m_plot || !m_plot->legend) return;

	// Find which legend item (if any) was clicked by checking bounding rects
	QCPAbstractLegendItem* clickedLegendItem = nullptr;
	for (int i = 0; i < m_plot->legend->itemCount(); ++i) {
		QCPAbstractLegendItem* item = m_plot->legend->item(i);
		if (item && item->rect().contains(pos)) { // Check if click is within item's rect
			clickedLegendItem = item;
			break;
		}
	}

	// Cast to the specific type we expect (plottable legend item)
	QCPPlottableLegendItem* plItem = qobject_cast<QCPPlottableLegendItem*>(clickedLegendItem);

	if (plItem) {
		// Find the corresponding dataset index
		int datasetIndex = -1;
		for(int i=0; i<m_datasets.size(); ++i) {
			// Check both measured and reference graphs
			if (m_datasets[i].graphMeasured == plItem->plottable() || m_datasets[i].graphReference == plItem->plottable()) {
				datasetIndex = i;
				break;
			}
		}

		if (datasetIndex != -1) {
			QMenu contextMenu(this);
			// Use dataset's display name in the menu item
			QAction *removeAction = contextMenu.addAction(QString("Remove '%1'").arg(m_datasets[datasetIndex].displayName));
			// Store the index in the action's data
			removeAction->setData(datasetIndex);
			connect(removeAction, &QAction::triggered, this, &PhaseNoiseAnalyzerApp::removeSelectedDataset);
			contextMenu.exec(m_plot->mapToGlobal(pos)); // Show menu at global cursor pos
		}
	}
	// Can add other context menu items here (e.g., for axes, general plot area) later
}

// Slot to remove the dataset triggered by the context menu
void PhaseNoiseAnalyzerApp::removeSelectedDataset()
{
	QAction *action = qobject_cast<QAction*>(sender());
	if (!action || !m_plot || !m_plot->legend) return; // Added legend check

	bool ok;
	int indexToRemove = action->data().toInt(&ok);

	if (ok && indexToRemove >= 0 && indexToRemove < m_datasets.size()) {
		PlotData& dataToRemove = m_datasets[indexToRemove];
		QString removedName = dataToRemove.displayName; // Store name before removal

		qInfo() << "Removing dataset:" << removedName;

		// --- Find and remove legend items associated with this dataset ---
		// Iterate backwards for safe removal while iterating
		for (int i = m_plot->legend->itemCount() - 1; i >= 0; --i) {
			QCPPlottableLegendItem* plItem = qobject_cast<QCPPlottableLegendItem*>(m_plot->legend->item(i));
			if (plItem) {
				if (plItem->plottable() == dataToRemove.graphMeasured || plItem->plottable() == dataToRemove.graphReference) {
					m_plot->legend->removeItem(i); // Remove from legend widget by index
				}
			}
		}
		// --- End Legend Item Removal ---

		// IMPORTANT: Remove graphs associated with this dataset from QCustomPlot first!
		if (dataToRemove.graphMeasured) m_plot->removeGraph(dataToRemove.graphMeasured);
		if (dataToRemove.graphReference) m_plot->removeGraph(dataToRemove.graphReference);
		if (dataToRemove.graphReferenceOutline) m_plot->removeGraph(dataToRemove.graphReferenceOutline);
		if (dataToRemove.fillReferenceBase) m_plot->removeGraph(dataToRemove.fillReferenceBase);
		// Pointers are implicitly cleared by removeGraph and will be null in the struct after removal anyway

		// Remove the data from our internal list
		m_datasets.removeAt(indexToRemove);

		// Update everything else
		updatePlot(); // Redraw plot without the removed graphs/legend items

		// Update window title if needed
		if (m_datasets.isEmpty()) {
			setWindowTitle("Phase Noise Analyzer");
			initPlot(); // Reset plot if last dataset was removed
		} else if (m_datasets.size() == 1) {
			setWindowTitle(QString("Phase Noise Analyzer - %1").arg(QFileInfo(m_datasets[0].filename).fileName()));
		} else {
			setWindowTitle(QString("Phase Noise Analyzer - %1 Files").arg(m_datasets.size()));
		}

		m_statusBar->showMessage(QString("Removed dataset '%1'").arg(removedName));
	} else {
		qWarning() << "Failed to remove dataset - invalid index or action data.";
	}
}

// --- Plot Interaction Handlers ---

void PhaseNoiseAnalyzerApp::onPlotMouseMove(QMouseEvent* event) {
	if (!m_plot || m_datasets.isEmpty()) return;

	// Convert pixel coordinates to axis coordinates
	double x = m_plot->xAxis->pixelToCoord(event->pos().x());
	double y = m_plot->yAxis->pixelToCoord(event->pos().y());

	// Update status bar always
	m_statusBar->showMessage(QString("Frequency: %1 Hz, SSB Phase Noise: %2 dBc/Hz")
								 .arg(Utils::formatFrequencyValue(x))
								 .arg(y, 0, 'f', 2));

	// Find the first visible measured graph
	QCPGraph* targetGraph = nullptr;
	for (const auto& data : m_datasets) {
		if (data.isVisible && data.graphMeasured) {
			targetGraph = data.graphMeasured;
			break;
		}
	}

	// Handle crosshair
	if (m_useCrosshair && targetGraph) {
		// Find the closest data point on the *measured* graph to the mouse cursor's x-coordinate
		double closestKey = std::numeric_limits<double>::quiet_NaN();
		double closestValue = std::numeric_limits<double>::quiet_NaN();
		double minDist = std::numeric_limits<double>::max();
		bool found = false;

		QSharedPointer<QCPGraphDataContainer> dataContainer = targetGraph->data();
		for (auto it = dataContainer->constBegin(); it != dataContainer->constEnd(); ++it) {
			double currentKey = it->key;
			// Find distance in plot coordinates (pixels) might be more intuitive? Or log space?
			// Let's use log distance on x-axis as before.
			if (x > 0 && currentKey > 0) { // Check positivity for log
				double dist = qAbs(qLn(currentKey) - qLn(x));
				if (dist < minDist) {
					minDist = dist;
					closestKey = currentKey;
					closestValue = it->value;
					found = true;
				}
			}
		}

		if (found) {
			// --- Update Tracer ---
			if (!m_cursorTracer) {
				m_cursorTracer = new QCPItemTracer(m_plot);
				m_cursorTracer->setStyle(QCPItemTracer::tsCircle);
				m_cursorTracer->setPen(QPen(Qt::red));
				m_cursorTracer->setBrush(Qt::red);
				m_cursorTracer->setSize(7);
				m_cursorTracer->setSelectable(false);
			}
			m_cursorTracer->setGraph(targetGraph); // Ensure it's linked to the correct graph
			m_cursorTracer->setGraphKey(closestKey);   // Move tracer to the data point
			m_cursorTracer->setVisible(true);

			// --- Update Annotation ---
			QString annotationText = QString("Freq: %1\nNoise: %2")
										 .arg(Utils::formatFrequencyValue(closestKey))
										 .arg(closestValue, 0, 'f', 2);

			if (!m_cursorAnnotation) {
				m_cursorAnnotation = new QCPItemText(m_plot);
				m_cursorAnnotation->setLayer("overlay"); // Draw on top
				m_cursorAnnotation->setFont(QFont("Liberation Sans", 9));
				m_cursorAnnotation->setColor(m_textColor);
				m_cursorAnnotation->setBrush(QBrush(m_annotationBgColor));
				m_cursorAnnotation->setPen(QPen(m_tickLabelColor)); // Border
				m_cursorAnnotation->setPadding(QMargins(5, 5, 5, 5));
				m_cursorAnnotation->setSelectable(false);
				// Position relative to tracer
				m_cursorAnnotation->position->setParentAnchor(m_cursorTracer->position);
			}
			m_cursorAnnotation->setText(annotationText);
			m_cursorAnnotation->setVisible(true);

			// Smart positioning based on cursor x-position relative to plot width
			double plotWidth = m_plot->axisRect()->width();
			double cursorXPixel = event->pos().x() - m_plot->axisRect()->left(); // X relative to axis rect
			if (plotWidth > 0 && cursorXPixel > plotWidth * 0.7) { // Cursor on right side (check plotWidth > 0)
				m_cursorAnnotation->position->setCoords(-45, 25); // Offset right and up
				m_cursorAnnotation->setTextAlignment(Qt::AlignRight | Qt::AlignBottom);
			} else { // Cursor on left or middle
				m_cursorAnnotation->position->setCoords(35, 25); // Offset left and up
				m_cursorAnnotation->setTextAlignment(Qt::AlignLeft | Qt::AlignBottom);
			}

			m_plot->replot(QCustomPlot::rpQueuedReplot); // Queue replot for efficiency
		} else {
			// Hide if no close point found? Or keep last position?
			// Let's hide them if no point is found near cursor X.
			if (m_cursorTracer) m_cursorTracer->setVisible(false);
			if (m_cursorAnnotation) m_cursorAnnotation->setVisible(false);
			m_plot->replot(QCustomPlot::rpQueuedReplot);
		}
	} // end if m_useCrosshair
}

void PhaseNoiseAnalyzerApp::onPlotMousePress(QMouseEvent* event) {
	if (!m_plot || m_datasets.isEmpty() || !m_measureMode) return;

	if (event->button() == Qt::LeftButton) {
		double x = m_plot->xAxis->pixelToCoord(event->pos().x());
		double y = m_plot->yAxis->pixelToCoord(event->pos().y());

		// Ensure x is valid (positive) for log scale calculations if needed later
		if (x <= 0) return;

		if (m_measureStartPoint.isNull()) { // First click
			m_measureStartPoint.setX(x);
			m_measureStartPoint.setY(y);

			// Clear previous measurement items
			for (QCPAbstractItem* item : std::as_const(m_measurementItems)) m_plot->removeItem(item);
			m_measurementItems.clear();
			if (m_measurementText) { m_plot->removeItem(m_measurementText); m_measurementText = nullptr; }


			// Add marker for start point using QCPItemTracer
			QCPItemTracer* startTracer = new QCPItemTracer(m_plot);
			startTracer->position->setCoords(x,y); // Direct coordinates
			startTracer->setStyle(QCPItemTracer::tsCircle);
			startTracer->setPen(QPen(Qt::red));
			startTracer->setBrush(Qt::red);
			startTracer->setSize(7);
			startTracer->setSelectable(false);
			m_measurementItems.append(startTracer);

			m_statusBar->showMessage(QString("Measurement: Start point set at Freq=%1, Noise=%2. Click end point.")
										 .arg(Utils::formatFrequencyValue(x)).arg(y, 0, 'f', 2));

		} else { // Second click - complete measurement
			double x1 = m_measureStartPoint.x();
			double y1 = m_measureStartPoint.y();
			double x2 = x;
			double y2 = y;

			// Add end marker (Tracer)
			QCPItemTracer* endTracer = new QCPItemTracer(m_plot);
			endTracer->position->setCoords(x2,y2);
			endTracer->setStyle(QCPItemTracer::tsCircle);
			endTracer->setPen(QPen(Qt::red));
			endTracer->setBrush(Qt::red);
			endTracer->setSize(7);
			endTracer->setSelectable(false);
			m_measurementItems.append(endTracer);

			// Add connecting line
			QCPItemLine* line = new QCPItemLine(m_plot);
			line->start->setCoords(x1, y1);
			line->end->setCoords(x2, y2);
			line->setPen(QPen(Qt::red, 1.5, Qt::DashLine));
			line->setSelectable(false);
			m_measurementItems.append(line);

			// Calculate slope (dB/decade)
			QString slopeStr = "N/A";
			if (x2 > x1 && x1 > 0) { // Ensure positive frequencies and x2 > x1
				double decadeChange = qLn(x2 / x1) / qLn(10.0); // log10(x2/x1)
				if (qAbs(decadeChange) > 1e-6) { // Avoid division by near-zero
					double slope = (y2 - y1) / decadeChange;
					slopeStr = QString::number(slope, 'f', 2) + " dB/decade";
				}
			}

			// Create measurement text
			QString text = QString("P1: %1, %2 dBc/Hz\n"
								   "P2: %3, %4 dBc/Hz\n"
								   "Delta: %5 dB\n"
								   "Slope: %6")
							   .arg(Utils::formatFrequencyValue(x1))
							   .arg(y1, 0, 'f', 2)
							   .arg(Utils::formatFrequencyValue(x2))
							   .arg(y2, 0, 'f', 2)
							   .arg(y2 - y1, 0, 'f', 2)
							   .arg(slopeStr);

			// Add text annotation
			if (!m_measurementText) {
				m_measurementText = new QCPItemText(m_plot);
				m_measurementText->setLayer("overlay");
				m_measurementText->setFont(QFont("Liberation Sans", 9));
				m_measurementText->setColor(m_textColor);
				m_measurementText->setBrush(QBrush(m_annotationBgColor));
				m_measurementText->setPen(QPen(m_tickLabelColor));
				m_measurementText->setPadding(QMargins(5, 5, 5, 5));
				m_measurementText->setSelectable(false);
			}
			m_measurementText->setText(text);

			// Position text near midpoint (geometric mean for x)
			double midX = (x1 > 0 && x2 > 0) ? qPow(10, (qLn(x1) + qLn(x2)) / (2.0*qLn(10))) : (x1+x2)/2.0;
			double midY = (y1 + y2) / 2.0;
			m_measurementText->position->setCoords(midX, midY);
			// Add offset, maybe anchor differently
			m_measurementText->setTextAlignment(Qt::AlignLeft | Qt::AlignBottom); // Anchor text bottom-left
			// Apply pixel offset
			QPointF pixelOffset(25, -25); // Offset right and up
			m_measurementText->position->setPixelPosition(m_measurementText->position->pixelPosition() + pixelOffset);

			m_statusBar->showMessage(QString("Measurement complete. Delta: %1 dB, Slope: %2").arg(y2 - y1, 0, 'f', 2).arg(slopeStr));

			// Reset for next measurement
			m_measureStartPoint = QPointF();
		}
		m_plot->replot(); // Update display
	} // end if LeftButton
}

// --- File I/O ---

void PhaseNoiseAnalyzerApp::onOpenFile()
{
	QStringList filenames = QFileDialog::getOpenFileNames(
		this, "Open CSV File(s)", "", "CSV Files (*.csv *.txt);;All Files (*)"
		);

	if (!filenames.isEmpty()) {
		// Reset states before loading new data
		m_filteringEnabled = false;
		m_spurRemovalEnabled = false;
		//m_datasets.clear(); // Option: Clear existing datasets if loading new set? Or just append? Let's append for now.
		m_filterCheckbox->setChecked(false);
		m_spurRemovalCheckbox->setChecked(false);
		m_filterAction->setChecked(false);
		m_tbFilterAction->setChecked(false);
		m_spurRemovalAction->setChecked(false);
		m_tbSpurRemovalAction->setChecked(false);
		// Reset spot noise markers/labels
		for(auto item : std::as_const(m_spotNoiseMarkers)) { if (item) m_plot->removeItem(item); } m_spotNoiseMarkers.clear();
		for(auto item : std::as_const(m_spotNoiseLabels)) { if (item) m_plot->removeItem(item); } m_spotNoiseLabels.clear();
		// Keep user preference for reference plotting if possible
		m_plotReferenceDefault = m_toggleReferenceAction->isChecked();

		for (const QString& filename : filenames) {
			loadData(filename);
		}
	}
}

void PhaseNoiseAnalyzerApp::onSavePlot()
{
	if (!m_plot) return;

	QString defaultFilename = m_outputFilename; // Use last saved or default
	if (defaultFilename.isEmpty() || defaultFilename == "Phase_Noise_Report.png") {
		if (!m_datasets.isEmpty()) {
			// Base default name on the first loaded file if multiple exist
			QFileInfo fileInfo(m_datasets.first().filename);
			defaultFilename = fileInfo.path() + "/" + fileInfo.completeBaseName() + "_Comparison.png"; // Suggest comparison name
		} else {
			defaultFilename = "Phase_Noise_Report.png";
		}
	}

	QString filename = QFileDialog::getSaveFileName(
		this, "Save Plot", defaultFilename,
		"PNG Files (*.png);;PDF Files (*.pdf);;JPEG Files (*.jpg);;BMP Files (*.bmp);;All Files (*)"
		);

	if (!filename.isEmpty()) {
		bool success = false;
		QFileInfo fi(filename);
		QString suffix = fi.suffix().toLower();

		if (suffix == "png") {
			success = m_plot->savePng(filename, 0, 0, 1.0, -1, m_dpi); // Use member DPI
		} else if (suffix == "pdf") {
			success = m_plot->savePdf(filename, 0, 0, QCP::epNoCosmetic); // No cosmetic pen scaling
		} else if (suffix == "jpg" || suffix == "jpeg") {
			success = m_plot->saveJpg(filename, 0, 0, 1.0, -1, m_dpi);
		} else if (suffix == "bmp") {
			success = m_plot->saveBmp(filename, 0, 0, 1.0, m_dpi);
		} else {
			// Default to PNG if extension unknown or missing
			QString pngFilename = fi.path() + "/" + fi.completeBaseName() + ".png";
			success = m_plot->savePng(pngFilename, 0, 0, 1.0, -1, m_dpi);
			QMessageBox::information(this, "File Type", QString("Unknown file type '%1', saving as PNG (%2).").arg(suffix).arg(QFileInfo(pngFilename).fileName()));
			filename = pngFilename; // Update filename to what was actually saved
		}

		if (success) {
			m_statusBar->showMessage(QString("Plot saved to %1").arg(QFileInfo(filename).fileName()));
			m_outputFilename = filename; // Update last saved name
			qInfo() << "Plot saved successfully to" << filename;
		} else {
			QMessageBox::critical(this, "Error Saving Plot", QString("Failed to save plot to %1.").arg(filename));
			qWarning() << "Failed to save plot to" << filename;
		}
	}
}

void PhaseNoiseAnalyzerApp::onExportData()
{
	if (m_datasets.isEmpty()) {
		QMessageBox::information(this, "No Data", "No data loaded to export.");
		return;
	}

	QString defaultFilename = "exported_data.csv";
	if (!m_datasets.isEmpty()) {
		QFileInfo fileInfo(m_datasets.first().filename); // Base on first file
		defaultFilename = fileInfo.path() + "/" + fileInfo.completeBaseName() + "_AllData_exported.csv";
	}

	QString filename = QFileDialog::getSaveFileName(
		this, "Export Data", defaultFilename, "CSV Files (*.csv);;All Files (*)"
		);

	if (!filename.isEmpty()) {
		QFile file(filename);
		if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			QTextStream out(&file);

			// Write Header
			out << "Frequency Offset (Hz)";
			for (const auto& data : m_datasets) {
				out << "," << data.displayName << " Phase Noise (dBc/Hz)";
				if (data.hasReferenceData) {
					out << "," << data.displayName << " Reference Noise (dBc/Hz)";
				}
			}
			out << "\n";

			// Find max number of points across all datasets (assuming frequency points might differ)
			int maxPoints = 0;
			for (const auto& data : m_datasets) { maxPoints = qMax(maxPoints, data.frequencyOffset.size()); }

			// Iterate through rows based on maxPoints
			for (int i = 0; i < maxPoints; ++i) {
				// Export frequency from the first dataset if available
				out << (i < m_datasets[0].frequencyOffset.size() ? QString::number(m_datasets[0].frequencyOffset[i], 'g', 9) : "");

				for (const auto& data : m_datasets) {
					const QVector<double>& noiseData = (m_spurRemovalEnabled || m_filteringEnabled) ? data.phaseNoiseFiltered : data.phaseNoise;
					const QVector<double>& refData = m_filteringEnabled ? data.referenceNoiseFiltered : data.referenceNoise;

					out << "," << (i < noiseData.size() ? QString::number(noiseData[i], 'f', 3) : "");
					if (data.hasReferenceData) {
						out << "," << (i < refData.size() && !std::isnan(refData[i]) ? QString::number(refData[i], 'f', 3) : "");
					}
				}
				out << "\n";
			}
			file.close();
			m_statusBar->showMessage(QString("Data exported to %1").arg(QFileInfo(filename).fileName()));
			qInfo() << "Data exported to" << filename;
		} else {
			QMessageBox::critical(this, "Error Exporting Data", QString("Could not open file for writing: %1").arg(filename));
			qWarning() << "Failed to open file for export:" << filename;
		}
	}
}

void PhaseNoiseAnalyzerApp::onExportSpotNoise()
{
	if (m_spotNoiseData.isEmpty()) {
		QMessageBox::information(this, "No Data", "No spot noise data calculated to export.");
		return;
	}

	QString defaultFilename = "spot_noise_data.csv";
	if (!m_datasets.isEmpty()) {
		QFileInfo fileInfo(m_datasets.first().filename);
		defaultFilename = fileInfo.path() + "/" + fileInfo.completeBaseName() + "_spot_noise.csv";
	}

	QString filename = QFileDialog::getSaveFileName(
		this, "Export Spot Noise Data", defaultFilename, "CSV Files (*.csv);;All Files (*)"
		);

	if (!filename.isEmpty()) {
		QFile file(filename);
		if (file.open(QIODevice::WriteOnly | QIODevice::Text)) {
			QTextStream out(&file);
			out << "Frequency Point,Actual Frequency (Hz),Phase Noise (dBc/Hz)\n";

			// Sort points by frequency for export consistency
			QVector<QPair<double, QString>> sortedPoints;
			for(auto it = m_spotNoiseData.constBegin(); it != m_spotNoiseData.constEnd(); ++it) {
				double targetFreq = Constants::FREQ_DISPLAY_TO_VALUE.value(it.key());
				sortedPoints.append(qMakePair(targetFreq, it.key()));
			}
			std::sort(sortedPoints.begin(), sortedPoints.end(),
					  [](const QPair<double, QString>& a, const QPair<double, QString>& b) { return a.first < b.first; });


			for (const auto& pair : sortedPoints) {
				QString displayName = pair.second;
				double actualFreq = m_spotNoiseData[displayName].first;
				double noiseValue = m_spotNoiseData[displayName].second;
				out << displayName << ","
					<< QString::number(actualFreq, 'g', 9) << ","
					<< QString::number(noiseValue, 'f', 3) << "\n";
			}
			file.close();
			m_statusBar->showMessage(QString("Spot noise data exported to %1").arg(QFileInfo(filename).fileName()));
			qInfo() << "Spot noise data exported to" << filename;
		} else {
			QMessageBox::critical(this, "Error Exporting Data", QString("Could not open file for writing: %1").arg(filename));
			qWarning() << "Failed to open file for spot noise export:" << filename;
		}
	}
}

void PhaseNoiseAnalyzerApp::closeEvent(QCloseEvent *event)
{
	// Add save settings logic here if needed later
	QMainWindow::closeEvent(event);
}

// Helper function (was missing from original class def, but used)
QString PhaseNoiseAnalyzerApp::freqFormatter(double value, int precision) {
	return Utils::formatFrequencyTick(value, precision); // Delegate to utility function
}

// Helper to get distinct colors for multiple traces
QColor PhaseNoiseAnalyzerApp::getNextColor(int index, bool darkTheme)
{
	// Use the helper function defined earlier
	return generateColor(index, darkTheme);
}

// Helper to get distinct reference colors/fills for multiple traces
QColor PhaseNoiseAnalyzerApp::getNextRefColor(int index, bool darkTheme)
{
	// Use the helper function defined earlier
	return generateRefColor(index, darkTheme);
}
