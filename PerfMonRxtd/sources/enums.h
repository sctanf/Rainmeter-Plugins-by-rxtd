/* Copyright (C) 2018 buckb
 * Copyright (C) 2018 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#pragma once

#undef UNIQUE_NAME

namespace pmre {
	enum class ResultString : unsigned char {
		NUMBER,
		ORIGINAL_NAME,
		UNIQUE_NAME,
		DISPLAY_NAME,
	};
	enum class RollupFunction : unsigned char {
		SUM,
		AVERAGE,
		MINIMUM,
		MAXIMUM,
		FIRST,
	};
}
