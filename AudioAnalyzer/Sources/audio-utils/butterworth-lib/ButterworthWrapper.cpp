/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "ButterworthWrapper.h"
#include "iir.h"

using namespace audio_utils;

FilterParameters ButterworthWrapper::calcCoefLowPass(index _order, double samplingFrequency, double cutoffFrequency) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	cutoffFrequency = std::min(cutoffFrequency, samplingFrequency * 0.5);
	const double digitalFreq = cutoffFrequency / samplingFrequency;

	double* aCoef = dcof_bwlp(order, digitalFreq);
	int* bCoef = ccof_bwlp(order);
	const double scalingFactor = sf_bwlp(order, digitalFreq);

	std::vector<double> b;
	b.resize(order + 1);
	for (index i = 0; i < index(b.size()); ++i) {
		b[i] = bCoef[i];
	}

	std::vector<double> a;
	a.resize(order + 1);
	for (index i = 0; i < index(a.size()); ++i) {
		a[i] = aCoef[i];
	}

	free(aCoef);
	free(bCoef);

	return { a, b, scalingFactor };
}

FilterParameters ButterworthWrapper::calcCoefHighPass(index _order, double samplingFrequency, double cutoffFrequency) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	cutoffFrequency = std::min(cutoffFrequency, samplingFrequency * 0.5);
	const double digitalFreq = cutoffFrequency / samplingFrequency;

	double* aCoef = dcof_bwhp(order, digitalFreq);
	int* bCoef = ccof_bwhp(order);
	const double scalingFactor = sf_bwhp(order, digitalFreq);

	std::vector<double> b;
	b.resize(order + 1);
	for (index i = 0; i < index(b.size()); ++i) {
		b[i] = bCoef[i];
	}

	std::vector<double> a;
	a.resize(order + 1);
	for (index i = 0; i < index(a.size()); ++i) {
		a[i] = aCoef[i];
	}

	free(aCoef);
	free(bCoef);

	return { a, b, scalingFactor };
}

FilterParameters ButterworthWrapper::calcCoefBandPass(
	index _order,
	double samplingFrequency,
	double lowerCutoffFrequency, double upperCutoffFrequency
) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	lowerCutoffFrequency = std::min(lowerCutoffFrequency, samplingFrequency * 0.5);
	upperCutoffFrequency = std::min(upperCutoffFrequency, samplingFrequency * 0.5);
	const double digitalFreq1 = lowerCutoffFrequency / samplingFrequency;
	const double digitalFreq2 = upperCutoffFrequency / samplingFrequency;

	double* aCoef = dcof_bwbp(order, digitalFreq1, digitalFreq2);
	int* bCoef = ccof_bwbp(order);
	const double scalingFactor = sf_bwbp(order, digitalFreq1, digitalFreq2);

	std::vector<double> b;
	b.resize(order + 1);
	for (index i = 0; i < index(b.size()); ++i) {
		b[i] = bCoef[i];
	}

	std::vector<double> a;
	a.resize(order + 1);
	for (index i = 0; i < index(a.size()); ++i) {
		a[i] = aCoef[i];
	}

	free(aCoef);
	free(bCoef);

	return { a, b, scalingFactor };
}

FilterParameters ButterworthWrapper::calcCoefBandStop(
	index _order,
	double samplingFrequency,
	double lowerCutoffFrequency, double upperCutoffFrequency
) {
	const int order = int(_order);
	if (order < 0) {
		return { };
	}

	lowerCutoffFrequency = std::min(lowerCutoffFrequency, samplingFrequency * 0.5);
	upperCutoffFrequency = std::min(upperCutoffFrequency, samplingFrequency * 0.5);
	const double digitalFreq1 = lowerCutoffFrequency / samplingFrequency;
	const double digitalFreq2 = upperCutoffFrequency / samplingFrequency;

	double* aCoef = dcof_bwbs(order, digitalFreq1, digitalFreq2);
	double* bCoef = ccof_bwbs(order, digitalFreq1, digitalFreq2);
	const double scalingFactor = sf_bwbs(order, digitalFreq1, digitalFreq2);

	std::vector<double> b;
	b.resize(order + 1);
	for (index i = 0; i < index(b.size()); ++i) {
		b[i] = bCoef[i];
	}

	std::vector<double> a;
	a.resize(order + 1);
	for (index i = 0; i < index(a.size()); ++i) {
		a[i] = aCoef[i];
	}

	free(aCoef);
	free(bCoef);

	return { a, b, scalingFactor };
}
