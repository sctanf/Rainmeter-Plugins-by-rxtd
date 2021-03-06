/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "kiss_fft-lib/KissFft.hh"

namespace rxtd::audio_utils {
	class FFT {
		using FftImpl = kiss_fft::KissFft<float>;

		index fftSize{ };
		float scalar{ };

		std::vector<float> window;

		FftImpl kiss;

		// need separate input buffer because of window application
		std::vector<FftImpl::scalar_type> inputBuffer;
		std::vector<FftImpl::complex_type> outputBuffer;

	public:
		FFT() = default;

		void setParams(index newSize, bool correctScalar, std::vector<float> window);

		[[nodiscard]]
		double getDC() const;
		
		[[nodiscard]]
		float getBinMagnitude(index binIndex) const;

		void process(array_view<float> wave);
	};
}
