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
#include "Color.h"
#include "option-parser/OptionMap.h"
#include "RainmeterWrappers.h"
#include "DiscreetInterpolator.h"
#include "image-utils/LinedImageHelper.h"
#include "image-utils/StripedImage.h"

namespace rxtd::audio_analyzer {
	class WaveForm : public SoundHandler {
	public:
		enum class LineDrawingPolicy {
			NEVER,
			BELOW_WAVE,
			ALWAYS,
		};

		struct Params {
		private:
			friend WaveForm;

			double resolution{ };
			index width{ };
			index height{ };
			string prefix = L".";
			utils::Color backgroundColor{ };
			utils::Color waveColor{ };
			utils::Color lineColor{ };
			LineDrawingPolicy lineDrawingPolicy{ };
			double gain{ };
			bool peakAntialiasing{ };
			bool moving{ };
			bool fading{ };
			index supersamplingSize{ };

			double minDistinguishableValue{ };

			// generated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.resolution == rhs.resolution
					&& lhs.width == rhs.width
					&& lhs.height == rhs.height
					&& lhs.prefix == rhs.prefix
					&& lhs.backgroundColor == rhs.backgroundColor
					&& lhs.waveColor == rhs.waveColor
					&& lhs.lineColor == rhs.lineColor
					&& lhs.lineDrawingPolicy == rhs.lineDrawingPolicy
					&& lhs.gain == rhs.gain
					&& lhs.peakAntialiasing == rhs.peakAntialiasing
					&& lhs.supersamplingSize == rhs.supersamplingSize
					&& lhs.minDistinguishableValue == rhs.minDistinguishableValue;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		index samplesPerSec{ };

		Params params;

		index blockSize{ };

		index counter = 0;
		double min{ };
		double max{ };
		bool changed = false;

		mutable string propString{ };

		utils::StripedImage image{ };
		std::vector<utils::StripedImage::PixelColor> stripBuffer{ };

		string filepath{ };

		utils::DiscreetInterpolator interpolator;

	public:
		void setParams(const Params& _params, Channel channel);

		static std::optional<Params> parseParams(
			const utils::OptionMap& optionMap,
			utils::Rainmeter::Logger& cl,
			const utils::Rainmeter& rain
		);

		void setSamplesPerSec(index samplesPerSec) override;
		void reset() override;

		void process(const DataSupplier& dataSupplier) override;
		void processSilence(const DataSupplier& dataSupplier) override;
		void finish(const DataSupplier& dataSupplier) override;

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
		// void fillLine(array_span<uint32_t> buffer);
		void fillLine(array_span<utils::Color> buffer);
	};
}
