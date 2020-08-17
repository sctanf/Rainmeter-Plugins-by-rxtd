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
#include "../../audio-utils/CustomizableValueTransformer.h"

namespace rxtd::audio_analyzer {
	class BlockHandler : public SoundHandler {
		using CVT = audio_utils::CustomizableValueTransformer;

	public:
		struct Params {
			double updateIntervalMs{ };

			double legacy_attackTime{ };
			double legacy_decayTime{ };
			CVT transformer{ };

			// autogenerated
			friend bool operator==(const Params& lhs, const Params& rhs) {
				return lhs.updateIntervalMs == rhs.updateIntervalMs
					&& lhs.transformer == rhs.transformer;
			}

			friend bool operator!=(const Params& lhs, const Params& rhs) {
				return !(lhs == rhs);
			}
		};

	private:
		Params params{ };

		index blockSize{ };

	protected:
		index counter = 0;

	public:
		[[nodiscard]]
		bool checkSameParams(const std::any& p) const override {
			return compareParamsEquals(params, p);
		}

		void setParams(const std::any& p) override {
			params = std::any_cast<Params>(p);
		}

		[[nodiscard]]
		ParseResult parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain,
			index legacyNumber
		) const override;

	protected:
		[[nodiscard]]
		ConfigurationResult vConfigure(Logger& cl) override;

		void vReset() final;
		void vProcess(array_view<float> wave) final;

		bool vGetProp(const isview& prop, utils::BufferPrinter& printer) const override;

		void setNextValue(float value);

		[[nodiscard]]
		index getBlockSize() const {
			return blockSize;
		}

		virtual void _process(array_view<float> wave) = 0;
		virtual void finishBlock() = 0;
		virtual void _reset() = 0;
	};


	class BlockRms : public BlockHandler {
		double intermediateResult = 0.0;

	public:
		void _process(array_view<float> wave) override;

	protected:
		void finishBlock() override;
		void _reset() override;
	};

	class BlockPeak : public BlockHandler {
		float intermediateResult = 0.0;

	public:
		void _process(array_view<float> wave) override;

	protected:
		void finishBlock() override;
		void _reset() override;
	};
}
