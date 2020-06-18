/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "SoundHandler.h"
#include "OptionParser.h"
#include "RainmeterWrappers.h"

namespace rxtd::audio_analyzer {
	class BlockMean : public SoundHandler {
	public:
		struct Params {
		private:
			friend BlockMean;

			double attackTime { };
			double decayTime { };
			double resolution { };
		};
	protected:
		Params params { };

		index samplesPerSec { };

		double attackDecayConstants[2] { };
		index blockSize { };

		index counter = 0;
		double intermediateResult = 0.0;
		float result = 0.0;

		mutable string propString { };

	public:
		void setParams(Params params);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override { };

		bool isValid() const override {
			return true;
		}
		array_view<float> getData(layer_t layer) const override {
			return { &result, 1 };
		}
		layer_t getLayersCount() const override {
			return 1;
		}

		const wchar_t* getProp(const isview& prop) const override;

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl);

	private:
		void recalculateConstants();
		virtual void finishBlock() = 0;
	};

	class BlockRms : public BlockMean {
	public:
		void process(const DataSupplier& dataSupplier) override;

	private:
		void finishBlock() override;
	};

	class BlockPeak : public BlockMean {
	public:
		void process(const DataSupplier& dataSupplier) override;

	private:
		void finishBlock() override;
	};
}