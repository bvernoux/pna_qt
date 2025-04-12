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

#ifndef UTILS_H
#define UTILS_H

#include <QVector>
#include <QString>

namespace Utils {

// Frequency formatting
QString formatFrequencyTick(double freq, int precision); // For axis ticks
QString formatFrequencyValue(double freq); // For display values (like spot noise)

// Data Filtering (Basic Implementations)
QVector<double> movingAverage(const QVector<double>& data, int windowSize);
QVector<double> medianFilter(const QVector<double>& data, int windowSize);
QVector<double> savitzkyGolay(const QVector<double>& data, int windowSize, int polyOrder = 3);

// Spur Removal Helper
QVector<double> rollingMedian(const QVector<double>& data, int windowSize);

// Interpolation
double linearInterpolate(double x1, double y1, double x2, double y2, double x);

} // namespace Utils

#endif // UTILS_H
