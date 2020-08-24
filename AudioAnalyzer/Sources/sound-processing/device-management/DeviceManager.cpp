/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "DeviceManager.h"

using namespace audio_analyzer;

bool DeviceManager::reconnect(AudioEnumeratorHelper& enumerator, DataSource type, const string& id) {
	deviceRelease();

	state = State::eOK;

	std::optional<utils::MediaDeviceWrapper> deviceOpt;
	switch (type) {
	case DataSource::eDEFAULT_INPUT:
		deviceOpt = enumerator.getDefaultDevice(utils::MediaDeviceType::eINPUT);
		if (!deviceOpt) {
			state = State::eERROR_MANUAL;
			return true;
		}
		break;

	case DataSource::eDEFAULT_OUTPUT:
		deviceOpt = enumerator.getDefaultDevice(utils::MediaDeviceType::eOUTPUT);
		if (!deviceOpt) {
			state = State::eERROR_MANUAL;
			return true;
		}
		break;

	case DataSource::eID:
		deviceOpt = enumerator.getDevice(id);
		if (!deviceOpt) {
			state = State::eERROR_MANUAL;
			return true;
		}
		break;
	}

	audioDeviceHandle = std::move(deviceOpt.value());

	captureManager = CaptureManager{ logger, audioDeviceHandle };

	if (!captureManager.isValid()) {
		if (!captureManager.isRecoverable()) {
			state = State::eFATAL;
			logger.debug(L"device manager fatal");
			return false;
		}

		logger.debug(L"device manager recoverable, but won't try this time");

		deviceRelease();
		return true;
	}

	auto deviceInfo = audioDeviceHandle.readDeviceInfo();
	diSnapshot.status = true;
	diSnapshot.id = deviceInfo.id;
	diSnapshot.description = deviceInfo.desc;
	diSnapshot.name = legacyNumber < 104 ? deviceInfo.fullFriendlyName : deviceInfo.name;
	diSnapshot.formatString = captureManager.getFormatString();
	diSnapshot.type = audioDeviceHandle.getType();
	diSnapshot.format = captureManager.getWaveFormat();

	return true;
}

void DeviceManager::deviceRelease() {
	captureManager = { };
	audioDeviceHandle = { };
	diSnapshot = { };
}
