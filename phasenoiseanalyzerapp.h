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

#ifndef PHASENOISEANALYZERAPP_H
#define PHASENOISEANALYZERAPP_H

#include <QMainWindow>
#include <QVector>
#include <QString>
#include <QMap>
#include <QList>
#include <QPair>
#include <QColor>
#include <QPointF>
#include <QMenu> // Include for context menu

#include "qcustomplot.h" // Include QCustomPlot header
#include "constants.h"
#include "utils.h"

// Forward declarations for Qt classes to reduce header dependencies
class QAction;
class QToolBar;
class QStatusBar;
class QDockWidget;
class QCheckBox;
class QComboBox;
class QPushButton;
class QGroupBox;
class QSlider;
class QSpinBox;
class QDoubleSpinBox;
class QTableWidget;
class QSplitter;
class QVBoxLayout;
class QTimer;
class QMouseEvent; // Forward declare for event parameter type
class QContextMenuEvent; // Forward declare for event parameter type

// --- Custom Axis Ticker Definition ---

class QCPAxisTickerSI : public QCPAxisTickerLog
{
	// Q_OBJECT // No Q_OBJECT needed if not using signals/slots specifically for this class

public:
	QCPAxisTickerSI() : QCPAxisTickerLog() {
		setLogBase(10.0); // Ensure log base is 10 for power-of-10 filtering
	}

	// Override the main generation function to filter ticks afterwards
	virtual void generate(const QCPRange &range, const QLocale &locale, QChar formatChar, int precision,
						  QVector<double> &ticks, QVector<double> *subTicks, QVector<QString> *tickLabels) Q_DECL_OVERRIDE
	{
		// 1. Let the base Log ticker generate everything first
		QCPAxisTickerLog::generate(range, locale, formatChar, precision, ticks, subTicks, tickLabels);

		// 2. Filter Major Ticks to Keep Only (approx) Powers of 10
		QVector<double> filteredTicks;
		const double epsilon = 1e-9; // Tolerance for floating point comparison
		int originalTickCount = ticks.size(); // Store original count

		if (!ticks.isEmpty()) { // Avoid processing if base class returned nothing
            for (double tick : std::as_const(ticks)) {
				// Check positivity for log calculation, handle near-zero carefully
				if (tick > epsilon) {
					double log10Tick = std::log10(tick);
					// Keep if log10 is very close to an integer
					if (qAbs(log10Tick - std::round(log10Tick)) < epsilon) {
						// Avoid adding duplicate ticks if filtering brings them too close
						if (filteredTicks.isEmpty() || qAbs(tick - filteredTicks.last()) > epsilon * qMax(1.0, qAbs(tick))) {
							filteredTicks.append(tick);
						}
					}
				}
				// Optionally handle tick == 0 if needed (e.g., return "0 Hz") but unlikely for pure log
			}

			// Fallback logic if filtering removed all ticks but original had some
			if (filteredTicks.isEmpty() && originalTickCount > 0) {
				double firstTick = ticks.first();
				double lastTick = ticks.last();
				filteredTicks.append(firstTick); // Keep at least the first
				// Add last only if distinct from first
				if (qAbs(lastTick - firstTick) > epsilon * qMax(1.0, qAbs(firstTick))) {
					filteredTicks.append(lastTick);
				}
				// Sort fallback ticks just in case first/last were out of order originally
				std::sort(filteredTicks.begin(), filteredTicks.end());
				qWarning() << "QCPAxisTickerSI: Filtering removed all power-of-10 ticks, falling back to first/last original ticks for range:" << range;
			}
		}
		// Replace original ticks with the filtered ones
		ticks = filteredTicks;

		// 3. Regenerate Labels based *only* on the filtered Ticks
		if (tickLabels) {
			tickLabels->clear();
			for (double tick : std::as_const(ticks)) {
				// Use our overridden getTickLabel which calls the SI formatter
				tickLabels->append(getTickLabel(tick, locale, formatChar, precision));
			}
		}

		// 4. Subticks: Keep the ones generated by the base class.
		//    They might look dense relative to the filtered major ticks, but recalculating
		//    them accurately here is complex. This is usually an acceptable compromise.
		if (!subTicks) {
			// No subticks requested
		} else if (ticks.isEmpty()) {
			subTicks->clear(); // No major ticks -> no subticks
		}
		// else: Keep subTicks as generated by QCPAxisTickerLog::generate

	} // End of generate()

	// Override getTickLabel to use our custom SI formatting function
	virtual QString getTickLabel(double tick, const QLocale &locale, QChar formatChar, int precision) Q_DECL_OVERRIDE
	{
		Q_UNUSED(locale)
		Q_UNUSED(formatChar)
		Q_UNUSED(precision)

		// Use Utils::formatFrequencyTick for SI prefixes (Hz, kHz, MHz)
		// Handle edge case near zero for safety, although log shouldn't generate it
		if (tick <= 1e-9) return QStringLiteral("0 Hz");
		// Precision argument to formatFrequencyTick isn't strictly used by its current logic
		return Utils::formatFrequencyTick(tick, 3);
	}

}; // End of class QCPAxisTickerSI


class PhaseNoiseAnalyzerApp : public QMainWindow
{
	Q_OBJECT
	// --- Data Structure for a Single Dataset ---
	struct PlotData {
		QString filename;
		QString displayName; // Short name for legend
		QVector<double> frequencyOffset;
		QVector<double> phaseNoise;
		QVector<double> referenceNoise;
		QVector<double> phaseNoiseFiltered; // For filtering/spur removal
		QVector<double> referenceNoiseFiltered; // For filtering
		bool hasReferenceData = false;
		bool isVisible = true; // Controlled by legend click
		QColor measuredColor;
		QColor referenceColor; // Line color (dark) or fill baseline color (light)

		// QCPGraph pointers associated with this data
		QCPGraph* graphMeasured = nullptr;
		QCPGraph* graphReference = nullptr; // Reference line (dark) or fill graph (light)
		QCPGraph* graphReferenceOutline = nullptr; // Outline for light theme fill
		QCPGraph* fillReferenceBase = nullptr;  // Baseline for light theme fill
	};

public:
	PhaseNoiseAnalyzerApp(const QStringList& csvFilenames = QStringList(),
						  bool plotReference = false,
						  bool useDarkTheme = false,
						  int dpi = Constants::DEFAULT_DPI,
						  QWidget *parent = nullptr);
	~PhaseNoiseAnalyzerApp();

	// Timer for delayed maximization
	QTimer* m_startupTimer = nullptr;

public slots:
	void showMaximizedWithDelay(); // Slot for delayed maximization

protected:
	void closeEvent(QCloseEvent *event) override;

private slots:
	// File Actions
	void onOpenFile();
	void onSavePlot();
	void onExportData();
	void onExportSpotNoise();

	// View Actions
	void toggleTheme(bool checked = false); // Accept bool for checkbox signal
	void toggleReference(bool checked = false);
	void toggleSpotNoise(bool checked = false);
	void toggleSpotNoiseTable(bool checked = false);
	void toggleGrid(bool checked = false);

	// Tool Actions
	void toggleCrosshair(bool checked = false);
	void toggleMeasurementTool(bool checked = false);
	void toggleDataFiltering(bool checked = false); // Main toggle
	void toggleSpurRemoval(bool checked = false);

	// Plot Control Actions
	void updatePlotLimits();
	void onMinFreqSliderChanged(int value);
	void onMaxFreqSliderChanged(int value);
	void applyDataFiltering(); // Triggered by button or filter change when enabled
	void changeLineColor(const QString& lineType); // Parameterized color change

	// Toolbar Actions
	void homeView();
	void panzoomButtonClicked(bool checked);

	// Legend/Dataset Actions
	void onLegendItemClicked(QCPLegend *legend, QCPAbstractLegendItem *item, QMouseEvent *event);
	void showPlotContextMenu(const QPoint &pos);
	void removeSelectedDataset(); // Slot connected to context menu action
	void configureSubplots(); // Might just reset view

	// Plot Interaction Slots
	void onPlotMouseMove(QMouseEvent* event);
	void onPlotMousePress(QMouseEvent* event);

	// Utility Slots
	void forceOddWindowSize(int value);

	//positionSpotNoiseTable
	void positionSpotNoiseTable(void);

	// Synchronizes right y-axis with left y-axis during range changes
	void synchronizeYAxes(const QCPRange &range);

private:
	void setupUi();
	void createMenus();
	void createToolbars();
	void createPlotArea();
	void createToolPanels();
	void centerWindow();
	void applyTheme(); // Apply current theme (light/dark)

	void loadData(const QString& filename);
	void updateDataTable();
	void initPlot(); // Initialize plot appearance, axes etc.
	void updatePlot(); // Update plot with current data and settings
	void calculateSpotNoise(); // Calculate spot noise values from current data
	void addSpotNoiseTable(); // Add the text table to the plot
	void applySpurRemoval(); // Apply spur removal algorithm
	QString freqFormatter(double value, int precision); // For axis ticks
	int findClosestFreqStepIndex(double freq); // Helper for sliders

	QColor getNextColor(int index, bool darkTheme);
	QColor getNextRefColor(int index, bool darkTheme);

	// --- Data Members ---
	QString m_outputFilename;
	bool m_plotReferenceDefault; // Initial setting from args/constructor
	bool m_useDarkTheme;
	int m_dpi;

	// Data Storage for Multiple Datasets
	QList<PlotData> m_datasets;

	QVector<double> m_frequencyOffsetFiltered;
	QVector<double> m_phaseNoiseFiltered;
	QVector<double> m_referenceNoiseFiltered;
	bool m_filteringEnabled = false;
	bool m_spurRemovalEnabled = false;

	// Spot Noise Data
	// Store as Map: Display Name -> Pair(Actual Freq, Noise Value)
	QMap<QString, QPair<double, double>> m_spotNoiseData;

	// UI State
	bool m_showSpotNoise = true;
	bool m_showSpotNoiseTable = true;
	bool m_useCrosshair = false;
	bool m_measureMode = false;
	QPointF m_measureStartPoint; // For measurement tool (in axis coords)
	enum class ActiveTool { None, PanZoom } m_activeTool = ActiveTool::None;


	// Axis Range State
	int m_minFreqSliderIndex = 0;
	int m_maxFreqSliderIndex = Constants::FREQ_POINTS.size() - 1;

	// Colors
	QColor m_spotNoiseColor;
	QColor m_tickLabelColor;
	QColor m_gridColor;
	QColor m_axisLabelColor;
	QColor m_textColor;
	QColor m_annotationBgColor;
	QList<QColor> mDefaultPlotColors; // Predefined list for multiple traces

	const QColor m_defaultMeasuredColorLight_1 = Constants::DEFAULT_MEASURED_COLOR_LIGHT_1;
	const QColor m_defaultReferenceColorLight_1 = Constants::DEFAULT_REFERENCE_COLOR_LIGHT_1;
	const QColor m_defaultSpotNoiseColorLight = Constants::DEFAULT_SPOT_NOISE_COLOR_LIGHT;
	const QColor m_defaultMeasuredColorDark_1 = Constants::DEFAULT_MEASURED_COLOR_DARK_1;
	const QColor m_defaultReferenceColorDark_1 = Constants::DEFAULT_REFERENCE_COLOR_DARK_1;
	const QColor m_defaultSpotNoiseColorDark = Constants::DEFAULT_SPOT_NOISE_COLOR_DARK;

	// --- UI Elements ---
	QWidget* m_centralWidget = nullptr;
	QVBoxLayout* m_mainLayout = nullptr;
	QStatusBar* m_statusBar = nullptr;

	// Menus & Actions
	QAction* m_openAction = nullptr;
	QAction* m_savePlotAction = nullptr;
	QAction* m_exportDataAction = nullptr;
	QAction* m_exportSpotAction = nullptr;
	QAction* m_exitAction = nullptr;
	QAction* m_toggleDarkThemeAction = nullptr;
	QAction* m_toggleReferenceAction = nullptr;
	QAction* m_toggleSpotNoiseAction = nullptr;
	QAction* m_toggleSpotNoiseTableAction = nullptr;
	QAction* m_crosshairAction = nullptr;
	QAction* m_measureAction = nullptr;
	QAction* m_filterAction = nullptr; // Menu action for filtering
	QAction* m_spurRemovalAction = nullptr; // Menu action for spur removal

	// Toolbars & Toolbar Actions
	QToolBar* m_mainToolbar = nullptr;
	QAction* m_tbOpenAction = nullptr;
	QAction* m_tbSaveAction = nullptr;
	QAction* m_tbThemeAction = nullptr;
	QAction* m_tbCrosshairAction = nullptr;
	QAction* m_tbMeasureAction = nullptr;
	QAction* m_tbFilterAction = nullptr; // Toolbar action for filtering
	QAction* m_tbSpurRemovalAction = nullptr; // Toolbar action for spur removal
	QAction* m_homeAction = nullptr;
	QPushButton* m_panzoomButton = nullptr;

	// Plot Area
	QCustomPlot* m_plot = nullptr; // The plot widget

	// Plot Objects (managed by QCustomPlot)
	QCPGraph* m_fillReferenceBelow = nullptr; // Fill area for light theme
	QVector<QCPItemTracer*> m_spotNoiseMarkers;
	QVector<QCPItemText*> m_spotNoiseLabels;
	QCPItemText* m_spotNoiseTableText = nullptr;
	QCPItemText* m_cursorAnnotation = nullptr;
	QCPItemTracer* m_cursorTracer = nullptr; // Tracks data point for annotation
	QVector<QCPAbstractItem*> m_measurementItems; // Holds lines and markers
	QCPItemText* m_measurementText = nullptr;
	QCPTextElement* m_titleElement = nullptr;
	QCPTextElement* m_subtitleText = nullptr;

	// Tool Panels (Dock Widget)
	QDockWidget* m_plotDock = nullptr;
	QWidget* m_plotWidget = nullptr;
	QVBoxLayout* m_plotLayout = nullptr;

	// Controls within Dock
	QDoubleSpinBox* m_yMinSpin = nullptr;
	QDoubleSpinBox* m_yMaxSpin = nullptr;
	QSlider* m_minFreqSlider = nullptr;
	QSlider* m_maxFreqSlider = nullptr;
	QCheckBox* m_refCheckbox = nullptr;
	QCheckBox* m_spotCheckbox = nullptr;
	QCheckBox* m_spotTableCheckbox = nullptr;
	QCheckBox* m_gridCheckbox = nullptr;
	QCheckBox* m_darkCheckbox = nullptr;
	QCheckBox* m_spurRemovalCheckbox = nullptr;

	QCheckBox* m_filterCheckbox = nullptr;
	QComboBox* m_filterTypeCombo = nullptr;
	QSpinBox* m_filterWindowSpin = nullptr;
	QPushButton* m_applyFilterBtn = nullptr;

	QTableWidget* m_dataTable = nullptr;
	QPushButton* m_exportDataBtn = nullptr;
	QPushButton* m_exportSpotBtn = nullptr;
};


#endif // PHASENOISEANALYZERAPP_H
