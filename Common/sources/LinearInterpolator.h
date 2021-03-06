/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

namespace rxtd::utils {
	class LinearInterpolator {
		double valMin = 0;
		double linMin = 0;
		double alpha = 1;
		double reverseAlpha = 1;

	public:
		LinearInterpolator() = default;

		LinearInterpolator(double linMin, double linMax, double valMin, double valMax) {
			setParams(linMin, linMax, valMin, valMax);
		}

		void setParams(double linMin, double linMax, double valMin, double valMax) {
			this->linMin = linMin;
			this->valMin = valMin;
			this->alpha = (valMax - valMin) / (linMax - linMin);
			this->reverseAlpha = 1 / alpha;
		}

		[[nodiscard]]
		double toValue(double linear) const {
			return valMin + (linear - linMin) * alpha;
		}

		[[nodiscard]]
		double toLinear(double value) const {
			return linMin + (value - valMin) * reverseAlpha;
		}
	};

	class LinearInterpolatorF {
		float valMin = 0;
		float linMin = 0;
		float alpha = 1;
		float reverseAlpha = 1;

	public:
		LinearInterpolatorF() = default;

		LinearInterpolatorF(float linMin, float linMax, float valMin, float valMax) {
			setParams(linMin, linMax, valMin, valMax);
		}

		void setParams(float linMin, float linMax, float valMin, float valMax) {
			this->linMin = linMin;
			this->valMin = valMin;
			this->alpha = (valMax - valMin) / (linMax - linMin);
			this->reverseAlpha = 1 / alpha;
		}

		[[nodiscard]]
		float toValue(float linear) const {
			return valMin + (linear - linMin) * alpha;
		}

		[[nodiscard]]
		float toLinear(float value) const {
			return linMin + (value - valMin) * reverseAlpha;
		}
	};
}
