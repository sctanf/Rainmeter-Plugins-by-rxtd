/*
 * Copyright (C) 2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once
#include "../SoundHandler.h"

namespace rxtd::audio_analyzer {
	class BandResampler;

	class ResamplerProvider : public SoundHandler {
	public:
		virtual ~ResamplerProvider() = default;

		virtual BandResampler* getResampler();
	};
}
