/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../SoundHandler.h"
#include "Vector2D.h"
#include "BandResampler.h"
#include "ResamplerProvider.h"

namespace rxtd::audio_analyzer {
	class legacy_FiniteTimeFilter : public ResamplerProvider {
	public:
		enum class SmoothingCurve {
			FLAT,
			LINEAR,
			EXPONENTIAL,
		};

		struct Params {
		private:
			friend legacy_FiniteTimeFilter;

			istring sourceId{ };

			SmoothingCurve smoothingCurve{ };
			index smoothingFactor{ };
			double exponentialFactor{ };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.sourceId == rhs.sourceId
					&& lhs.smoothingCurve == rhs.smoothingCurve
					&& lhs.smoothingFactor == rhs.smoothingFactor
					&& lhs.exponentialFactor == rhs.exponentialFactor;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params{ };

		index samplesPerSec{ };

		// pastValues[Layer][FilterSize][Band]
		std::vector<utils::Vector2D<float>> pastValues;
		utils::Vector2D<float> values;
		index pastValuesIndex = 0;

		double smoothingNormConstant{ };

		bool changed = true;
		const SoundHandler* source = nullptr;

	public:
		static std::optional<Params> parseParams(const OptionMap& optionMap, Logger& cl);

		void setParams(Params _params, Channel channel);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void _process(const DataSupplier& dataSupplier) override;
		void _finish(const DataSupplier& dataSupplier) override;

		array_view<float> getData(layer_t layer) const override {
			if (params.smoothingFactor <= 1) {
				return source->getData(layer);
			}

			return values[layer];
		}

		layer_t getLayersCount() const override {
			if (params.smoothingFactor <= 1) {
				return source->getLayersCount();
			}

			return layer_t(values.getBuffersCount());
		}

	private:
		void adjustSize();
		void copyValues();
		void applyTimeFiltering();
	};
}
