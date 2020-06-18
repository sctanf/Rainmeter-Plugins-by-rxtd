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
#include <Vector2D.h>
#include "Color.h"
#include "RainmeterWrappers.h"
#include "OptionParser.h"

namespace rxtd::audio_analyzer {
	class Spectrogram : public SoundHandler {
	public:
		struct Params {
		private:
			friend Spectrogram;

			double resolution { };
			index length { };
			istring sourceName { };
			string prefix = { };
			utils::Color baseColor { };
			utils::Color maxColor { };

			struct ColorDescription {
				float widthInverted;
				utils::Color color;
			};
			std::vector<float> colorLevels;
			std::vector<ColorDescription> colors;
			double colorMinValue { };
			double colorMaxValue { };
		};

	private:
		Params params;

		index samplesPerSec { };

		index blockSize { };

		index counter = 0;
		index lastLineIndex = 0;
		index sourceSize = 0;
		bool changed = false;
		index lastNonZeroLine = 0;

		mutable string propString { };

		utils::Vector2D<uint32_t> buffer;
		string filepath { };

	public:
		void setParams(const Params& _params);

		static std::optional<Params> parseParams(const utils::OptionParser::OptionMap& optionMap, utils::Rainmeter::ContextLogger &cl, const utils::Rainmeter& rain);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;

		bool isValid() const override {
			return true; // TODO
		}
		array_view<float> getData(layer_t layer) const override {
			return { };
		}
		layer_t getLayersCount() const override {
			return 0;
		}

		const wchar_t* getProp(const isview& prop) const override;
		bool isStandalone() override {
			return true;
		}

	private:
		void updateParams();
		void writeFile(const DataSupplier& dataSupplier);
		void fillLine(array_view<float> data);
		void fillLineMulticolor(array_view<float> data);
	};
}