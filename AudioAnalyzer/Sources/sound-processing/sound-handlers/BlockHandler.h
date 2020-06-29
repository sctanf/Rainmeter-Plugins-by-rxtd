/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "SoundHandler.h"
#include "RainmeterWrappers.h"
#include "../../audio-utils/LogarithmicIRF.h"

namespace rxtd::audio_analyzer {
	class BlockHandler : public SoundHandler {
	public:
		struct Params {
		private:
			friend BlockHandler;

			double attackTime { };
			double decayTime { };
			double resolution { };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.attackTime == rhs.attackTime
					&& lhs.decayTime == rhs.decayTime
					&& lhs.resolution == rhs.resolution;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	protected:
		Params params { };

		index samplesPerSec { };

		audio_utils::LogarithmicIRF filter { };
		index blockSize { };

		index counter = 0;
		double intermediateResult = 0.0;
		float result = 0.0;

		mutable string propString { };

	public:
		virtual void setParams(Params params);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override { }

		array_view<float> getData(layer_t layer) const override {
			return { &result, 1 };
		}

		const wchar_t* getProp(const isview& prop) const override;

		static std::optional<Params> parseParams(const utils::OptionMap& optionMap, utils::Rainmeter::Logger &cl);

	private:
		void recalculateConstants();
		virtual void finishBlock() = 0;
	};

	class BlockRms : public BlockHandler {
	public:
		void process(const DataSupplier& dataSupplier) override;

	protected:
		void processRms(array_view<float> wave);
		void finishBlock() override;
	};

	class BlockPeak : public BlockHandler {
	public:
		void process(const DataSupplier& dataSupplier) override;

	protected:
		void finishBlock() override;
	};
}
