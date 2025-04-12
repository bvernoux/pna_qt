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

#include "utils.h"
#include <QtMath> // For qPow, qFabs, qLn
#include <limits> // For std::numeric_limits

namespace Utils {

QString formatFrequencyTick(double freq, int precision) {
	Q_UNUSED(precision); // We'll control precision internally

	// Handle zero or very small numbers
	if (qFabs(freq) < 1e-9) return QStringLiteral("0");

	if (freq >= 1e6) { // MHz range
		double val = freq / 1e6;
		// Show one decimal place for MHz, unless it's exactly an integer
		QString s = QString::number(val, 'f', 1);
		if (s.endsWith(".0")) {
			s.chop(2); // Remove ".0"
		}
		return s + "M";
	} else if (freq >= 1e3) { // kHz range
		double val = freq / 1e3;
		// Show no decimal places for kHz
		QString s = QString::number(val, 'f', 0);
		return s + "k";
	} else { // Hz range (< 1000 Hz)
		// Show no decimal places for Hz ticks (e.g., 1, 10, 100)
		// Handle sub-Hz ticks if necessary (e.g., 0.1 Hz)
		if (freq < 1.0 && freq > 0) {
			// Show one decimal for sub-Hz ticks like 0.1
			return QString::number(freq, 'f', 1);
		} else {
			// For 1, 10, 100 Hz etc., show no decimals
			return QString::number(freq, 'f', 0);
		}
	}
}

QString formatFrequencyValue(double freq) {
	if (qFabs(freq) < std::numeric_limits<double>::epsilon()) return QStringLiteral("0 Hz");
	if (freq >= 1e6) {
		return QStringLiteral("%1 MHz").arg(freq / 1e6, 0, 'f', 2);
	} else if (freq >= 1e3) {
		return QStringLiteral("%1 kHz").arg(freq / 1e3, 0, 'f', 2);
	} else {
		return QStringLiteral("%1 Hz").arg(freq, 0, 'f', 2);
	}
}

QVector<double> movingAverage(const QVector<double>& data, int windowSize) {
	if (windowSize % 2 == 0) windowSize++; // Ensure odd
	if (windowSize < 3 || data.isEmpty()) return data;

	int halfWindow = windowSize / 2;
	QVector<double> smoothed(data.size());
	QVector<double> validData; // Handle potential NaNs if necessary

	// Simple padding at edges: replicate edge value
	for (int i = 0; i < data.size(); ++i) {
		double currentSum = 0;
		int count = 0;
		for (int j = -halfWindow; j <= halfWindow; ++j) {
			int index = i + j;
			if (index >= 0 && index < data.size()) {
				// Assuming data doesn't contain NaN/inf here
				currentSum += data[index];
				count++;
			}
		}
		if (count > 0) {
			smoothed[i] = currentSum / count;
		} else {
			smoothed[i] = data[i]; // Should not happen with valid window/data
		}
	}
	return smoothed;
}

// Simple (less efficient) median filter implementation
QVector<double> medianFilter(const QVector<double>& data, int windowSize) {
	if (windowSize % 2 == 0) windowSize++; // Ensure odd
	if (windowSize < 3 || data.isEmpty()) return data;

	int halfWindow = windowSize / 2;
	QVector<double> filtered(data.size());
	std::vector<double> window; // Use std::vector for sorting

	for (int i = 0; i < data.size(); ++i) {
		window.clear();
		for (int j = -halfWindow; j <= halfWindow; ++j) {
			int index = i + j;
			// Edge handling: Clamp index to valid range (like some median filter implementations)
			index = std::max(0, std::min(static_cast<int>(data.size()) - 1, index));
			// Assuming data doesn't contain NaN/inf
			window.push_back(data[index]);
		}
		// Sort the window and pick the median
		std::sort(window.begin(), window.end());
		filtered[i] = window[window.size() / 2];
	}
	return filtered;
}

// Basic Rolling Median - similar to medianFilter above, for spur removal baseline
QVector<double> rollingMedian(const QVector<double>& data, int windowSize) {
	// This is functionally the same as the medianFilter provided above for simplicity.
	// A more efficient implementation would use sliding window data structures.
	return medianFilter(data, windowSize);
}

// Savitzky-Golay Filter - Basic Implementation using precomputed coefficients (common cases)
// WARNING: This is a very simplified version. A robust implementation requires
// calculating coefficients based on window, order, and derivative.
// This example uses coefficients for smoothing (0th derivative), polyorder 3.
QVector<double> savitzkyGolay(const QVector<double>& data, int windowSize, int polyOrder) {
	if (windowSize % 2 == 0) windowSize++; // Ensure odd
	if (windowSize < 5 || polyOrder >= windowSize || data.size() < windowSize) {
		// Return original data if parameters are invalid or data is too small
		// A more robust version might try lower order/window or throw error
		return data;
	}

	// Coefficients for smoothing (0th derivative), polyorder=3
	// Source: Numerical Recipes or online calculators
	// These need to be adjusted or calculated if windowSize/polyOrder changes significantly.
	// Example for windowSize=5, polyOrder=3 (or 2):
	const std::vector<double> coeffs_5 = {-3, 12, 17, 12, -3}; double norm_5 = 35.0;
	// Example for windowSize=7, polyOrder=3:
	const std::vector<double> coeffs_7 = {-2, 3, 6, 7, 6, 3, -2}; double norm_7 = 21.0;
	// Example for windowSize=11, polyOrder=3:
	const std::vector<double> coeffs_11 = {-36, 9, 44, 69, 84, 89, 84, 69, 44, 9, -36}; double norm_11 = 429.0;
	// Example for windowSize=21, polyOrder=3 (might need higher precision):
	// Coefficients get complex, better to calculate dynamically. Using 11 as fallback.

	const std::vector<double>* coeffs_ptr;
	double norm;

	// Select coefficients based on window size (add more cases as needed)
	if (windowSize == 5) { coeffs_ptr = &coeffs_5; norm = norm_5; }
	else if (windowSize == 7) { coeffs_ptr = &coeffs_7; norm = norm_7; }
	else if (windowSize >=9 && windowSize <= 15 ) { coeffs_ptr = &coeffs_11; norm = norm_11; windowSize = 11; } // Approximation
	else if (windowSize > 15) { coeffs_ptr = &coeffs_11; norm = norm_11; windowSize = 11; } // Approximation
	else { return data; } // Unsupported size for this simple implementation

	const std::vector<double>& coeffs = *coeffs_ptr;
	int halfWindow = windowSize / 2;
	QVector<double> smoothed(data.size());

	for (int i = 0; i < data.size(); ++i) {
		double sum = 0;
		for (int j = -halfWindow; j <= halfWindow; ++j) {
			int index = i + j;
			// Edge handling: Reflect indices
			if (index < 0) index = -index;
			if (index >= data.size()) index = 2 * (data.size() - 1) - index;
			// Ensure index is still valid after reflection
			index = std::max(0, std::min(static_cast<int>(data.size()) - 1, index));
			sum += coeffs[j + halfWindow] * data[index];
		}
		smoothed[i] = sum / norm;
	}

	// Handle edges more carefully (e.g., copy original values for first/last halfWindow points)
	// This simple version might have edge artifacts.
	for(int i=0; i<halfWindow; ++i) smoothed[i] = data[i];
	for(int i=data.size()-halfWindow; i<data.size(); ++i) smoothed[i] = data[i];

	return smoothed;
}

double linearInterpolate(double x1, double y1, double x2, double y2, double x) {
	if (qFabs(x2 - x1) < std::numeric_limits<double>::epsilon()) {
		return y1; // Avoid division by zero, return start value
	}
	return y1 + (y2 - y1) * (x - x1) / (x2 - x1);
}

} // namespace Utils
