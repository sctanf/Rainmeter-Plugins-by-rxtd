/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "GrowingVector.h"
#include "SoundHandler.h"
#include "../../audio-utils/CustomizableValueTransformer.h"

namespace rxtd::audio_analyzer {
	class Loudness : public SoundHandler {
		using CVT = audio_utils::CustomizableValueTransformer;

	public:
		struct Params {
			CVT transformer{ };
			double gatingLimit{ };
			double updatesPerSecond{ };
			double timeWindowMs{ };
			double gatingDb{ };
			bool ignoreGatingForSilence{ };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.transformer == rhs.transformer
					&& lhs.gatingLimit == rhs.gatingLimit
					&& lhs.updatesPerSecond == rhs.updatesPerSecond
					&& lhs.timeWindowMs == rhs.timeWindowMs
					&& lhs.gatingDb == rhs.gatingDb
					&& lhs.ignoreGatingForSilence == rhs.ignoreGatingForSilence;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params{ };

		index blocksCount{ };
		index blockSize{ };
		double blockNormalizer{ };
		index blockCounter{ };

		double blockIntermediate{ };
		std::vector<double> blocks;
		index nextBlockIndex{ };

		double prevValue{ };
		double gatingValueCoefficient{ };
		index minBlocksCount{ };

	public:
		[[nodiscard]]
		bool checkSameParams(const std::any& p) const override {
			return compareParamsEquals(params, p);
		}

		[[nodiscard]]
		ParseResult parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			index legacyNumber
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(const std::any& _params, Logger& cl, std::any& snapshotAny) override;

		// void vReset() final;
		void vProcess(ProcessContext context) override;

		// bool vGetProp(const isview& prop, utils::BufferPrinter& printer) const override;

		void pushMicroBlock(double value);

	private:
		void pushNextValue(double value);
	};
}
