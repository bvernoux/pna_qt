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
**             Date: 24 May 2025                                          **
**          Version: 1.0.1.0                                              **
****************************************************************************/
#ifndef CONSTANTS_H
#define CONSTANTS_H

#include <QColor>
#include <QString>
#include <QVector>
#include <QMap>
#include <QPair>

namespace Constants {

// Application settings
constexpr double SPUR_THRESHOLD = 5.0; // dB above local baseline for spur
constexpr int DEFAULT_SPUR_WINDOW_SIZE = 21;

// Y-axis limits constants
constexpr double Y_AXIS_MIN = -200.0;
constexpr double Y_AXIS_MAX = 10.0;
constexpr double Y_AXIS_DEFAULT_MIN = -200.0;
constexpr double Y_AXIS_DEFAULT_MAX = -50.0;
constexpr double Y_AXIS_MAJOR_TICK = 10.0;
constexpr double Y_AXIS_MINOR_TICK = 5.0;

// X-axis constants
constexpr double X_AXIS_MIN = 0.1; // Min positive value for log scale
constexpr double X_AXIS_MAX = 1e7; // Max default value

// Forward declare the frequency points
inline const QVector<double> FREQ_POINTS = {0.1, 1.0, 10.0, 100.0, 1000.0, 10000.0, 100000.0, 1000000.0, 10000000.0};

struct FrequencyPointInfo {
    double value;
    QString displayName;
    QString formattedName;
};

// Forward declarations
inline FrequencyPointInfo getFreqInfo(double freq);
inline QVector<FrequencyPointInfo> generateFreqPointInfos();

// Function implementations
inline FrequencyPointInfo getFreqInfo(double freq) {
    FrequencyPointInfo info;
    info.value = freq;
    if (freq >= 1e6) {
        info.displayName = QStringLiteral("%1 MHz").arg(freq / 1e6, 0, 'f', 0);
        info.formattedName = QStringLiteral("%1 MHz").arg(freq / 1e6, 0, 'f', 3);
    } else if (freq >= 1e3) {
        info.displayName = QStringLiteral("%1 kHz").arg(freq / 1e3, 0, 'f', 0);
        info.formattedName = QStringLiteral("%1 kHz").arg(freq / 1e3, 0, 'f', 3);
    } else {
        info.displayName = QStringLiteral("%1 Hz").arg(freq, 0, 'f', 0);
        info.formattedName = QStringLiteral("%1 Hz").arg(freq, 0, 'f', 3);
    }
    return info;
}

inline QVector<FrequencyPointInfo> generateFreqPointInfos() {
    QVector<FrequencyPointInfo> infos;
    for(double freq : FREQ_POINTS) {
        infos.append(getFreqInfo(freq));
    }
    return infos;
}

inline const QVector<FrequencyPointInfo> FREQ_POINT_INFOS = generateFreqPointInfos();

inline QMap<QString, double> createFreqDisplayToValueMap() {
    QMap<QString, double> map;
    for(const auto& info : FREQ_POINT_INFOS) {
        map[info.displayName] = info.value;
    }
    return map;
}

inline const QMap<QString, double> FREQ_DISPLAY_TO_VALUE = createFreqDisplayToValueMap();

inline QMap<QString, QString> createFreqDisplayToFormattedMap() {
    QMap<QString, QString> map;
    for(const auto& info : FREQ_POINT_INFOS) {
        map[info.displayName] = info.formattedName;
    }
    return map;
}

inline const QMap<QString, QString> FREQ_DISPLAY_TO_FORMATTED = createFreqDisplayToFormattedMap();

// UI constants
constexpr int DEFAULT_WINDOW_SIZE = 11; // Filter window
constexpr int MIN_WINDOW_SIZE = 3;
constexpr int MAX_WINDOW_SIZE = 51;
constexpr int DEFAULT_DPI = 150; // Default DPI for plot saving
constexpr int WINDOW_WIDTH = 1200;
constexpr int WINDOW_HEIGHT = 800;

// Theme color constants (Using QColor for direct use in Qt)
const QColor DARK_BG_COLOR = QColor("#1c1c1c");
const QColor DARK_AXIS_COLOR = QColor("black");
const QColor DARK_TICK_COLOR = QColor("lightgrey");
const QColor DARK_GRID_COLOR = QColor("#555555");
const QColor DARK_TEXT_COLOR = QColor("white");
const QColor DARK_ANNOTATION_BG = QColor("#333333");

const QColor LIGHT_BG_COLOR = QColor("#FFFFFF");
const QColor LIGHT_AXIS_COLOR = QColor("white");
const QColor LIGHT_TICK_COLOR = QColor("black");
const QColor LIGHT_GRID_COLOR = QColor("darkgrey");
const QColor LIGHT_TEXT_COLOR = QColor("black");
const QColor LIGHT_ANNOTATION_BG = QColor("white");

// UI palette colors
const QColor DARK_PALETTE_WINDOW = QColor(53, 53, 53);
const QColor DARK_PALETTE_WINDOW_TEXT = QColor(255, 255, 255);
const QColor DARK_PALETTE_BASE = QColor(25, 25, 25);
const QColor DARK_PALETTE_ALT_BASE = QColor(53, 53, 53);
const QColor DARK_PALETTE_TOOLTIP_BASE = QColor(0, 0, 0);
const QColor DARK_PALETTE_TOOLTIP_TEXT = QColor(255, 255, 255);
const QColor DARK_PALETTE_TEXT = QColor(255, 255, 255);
const QColor DARK_PALETTE_BUTTON = QColor(53, 53, 53);
const QColor DARK_PALETTE_BUTTON_TEXT = QColor(255, 255, 255);
const QColor DARK_PALETTE_BRIGHT_TEXT = QColor(0, 128, 255); // Note: Might not map perfectly in palette
const QColor DARK_PALETTE_LINK = QColor(42, 130, 218);
const QColor DARK_PALETTE_HIGHLIGHT = QColor(42, 130, 218);
const QColor DARK_PALETTE_HIGHLIGHT_TEXT = QColor(0, 0, 0);

// Default plot line colors
const QColor DEFAULT_MEASURED_COLOR_LIGHT_1 = QColor("#17a2a2"); // Base color for first trace
const QColor DEFAULT_REFERENCE_COLOR_LIGHT_1 = QColor("lightgrey"); // Fill color for first trace
const QColor DEFAULT_SPOT_NOISE_COLOR_LIGHT = QColor("red"); // Spot noise color remains consistent

const QColor DEFAULT_MEASURED_COLOR_DARK_1 = QColor("cyan"); // Base color for first trace
const QColor DEFAULT_REFERENCE_COLOR_DARK_1 = QColor("yellow"); // Line color for first trace
const QColor DEFAULT_SPOT_NOISE_COLOR_DARK = QColor("orange"); // Spot noise color remains consistent

} // namespace Constants

#endif // CONSTANTS_H
