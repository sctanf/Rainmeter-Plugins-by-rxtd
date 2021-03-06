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
#include "BandResampler.h"

namespace rxtd::audio_analyzer {
	class BandCascadeTransformer : public SoundHandler {
		enum class MixFunction {
			AVERAGE,
			PRODUCT,
		};

		struct Params {
			double minWeight{ };
			double targetWeight{ };
			double zeroLevelHard{ };
			MixFunction mixFunction{ };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.minWeight == rhs.minWeight
					&& lhs.targetWeight == rhs.targetWeight
					&& lhs.zeroLevelHard == rhs.zeroLevelHard
					&& lhs.mixFunction == rhs.mixFunction;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

		Params params{ };

		BandResampler* resamplerPtr = nullptr;

		struct CascadeMeta {
			index offset{ };
			index nextChunkIndex{ };
			array_view<float> data;
			float maxValue{ };
		};

		std::vector<CascadeMeta> snapshot;

	public:
		[[nodiscard]]
		bool checkSameParams(const ParamsContainer& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		ParseResult parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			index legacyNumber
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) override;

	public:
		void vProcess(ProcessContext context, ExternalData& externalData) override;

	private:
		[[nodiscard]]
		float computeForBand(index band) const;
	};
}
