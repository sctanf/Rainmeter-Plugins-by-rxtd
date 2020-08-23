﻿/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioParent.h"

#include "ParamParser.h"

using namespace audio_analyzer;

AudioParent::AudioParent(utils::Rainmeter&& _rain) :
	ParentBase(std::move(_rain)),
	deviceManager(logger, [this](MyWaveFormat format) {
		channelMixer.setFormat(format);
		orchestrator.setFormat(format.samplesPerSec, format.channelLayout);
	}) {
	setUseResultString(false);

	if (deviceManager.getState() == DeviceManager::State::eFATAL) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	notificationClient = {
		[=](auto ptr) {
			*ptr = new utils::CMMNotificationClient{ deviceManager.getDeviceEnumerator().getWrapper() };
			return true;
		}
	};

	deviceManager.getDeviceEnumerator().updateDeviceStrings();

	paramParser.setRainmeter(rain);
	orchestrator.setLogger(logger);
}

void AudioParent::vReload() {
	const auto source = rain.read(L"Source").asIString();
	string id = { };

	using DataSource = DeviceManager::DataSource;
	DataSource sourceEnum;
	if (!source.empty()) {
		if (source == L"Output") {
			sourceEnum = DataSource::eDEFAULT_OUTPUT;
		} else if (source == L"Input") {
			sourceEnum = DataSource::eDEFAULT_INPUT;
		} else {
			logger.debug(L"Using '{}' as source audio device ID.", source);
			sourceEnum = DataSource::eID;
			id = source % csView();
		}
	} else {
		// legacy
		if (auto legacyID = this->rain.read(L"DeviceID").asString();
			!legacyID.empty()) {
			logger.debug(L"Using '{}' as source audio device ID.", legacyID);
			sourceEnum = DataSource::eID;
			id = legacyID;
		} else {
			const auto port = this->rain.read(L"Port").asIString(L"Output");
			if (port == L"Output") {
				sourceEnum = DataSource::eDEFAULT_OUTPUT;
			} else if (port == L"Input") {
				sourceEnum = DataSource::eDEFAULT_INPUT;
			} else {
				logger.error(L"Invalid Port '{}', must be one of: Output, Input. Set to Output.", port);
				sourceEnum = DataSource::eDEFAULT_OUTPUT;
			}
		}
	}

	const double computeTimeout = rain.read(L"computeTimeout").asFloat(-1.0);
	const double killTimeout = std::clamp(rain.read(L"killTimeout").asFloat(33.0), 1.0, 33.0);

	orchestrator.setComputeTimeout(computeTimeout);
	orchestrator.setKillTimeout(killTimeout);

	deviceManager.setOptions(sourceEnum, id);

	const bool anythingChanged = paramParser.parse();
	const index legacyNumber = paramParser.getLegacyNumber();
	switch (legacyNumber) {
	case 0:
	case 104:
		break;
	default:
		logger.error(L"Unknown magic number");
		setMeasureState(utils::MeasureState::eTEMP_BROKEN);
	}

	if (anythingChanged) {
		orchestrator.patch(paramParser.getParseResult(), legacyNumber, snapshot);
	}
}

double AudioParent::vUpdate() {
	const auto changes = notificationClient.getPointer()->takeChanges();

	const auto source = deviceManager.getRequesterSourceType();
	if (source == DeviceManager::DataSource::eDEFAULT_INPUT && changes.defaultCapture
		|| source == DeviceManager::DataSource::eDEFAULT_OUTPUT && changes.defaultRender) {
		deviceManager.forceReconnect();
	} else if (!changes.devices.empty()) {
		if (changes.devices.count(deviceManager.getDeviceInfo().id) > 0) {
			deviceManager.forceReconnect();
		}

		deviceManager.getDeviceEnumerator().updateDeviceStrings();
	}

	if (deviceManager.getState() != DeviceManager::State::eOK) {
		if (deviceManager.getState() == DeviceManager::State::eFATAL) {
			setMeasureState(utils::MeasureState::eBROKEN);
			logger.error(L"Unrecoverable error");
		}

		// todo
		// for (auto& [name, sa] : saMap) {
		// 	sa.resetValues();
		// }

		return deviceManager.getDeviceStatus();
	}

	bool any = false;
	deviceManager.getCaptureManager().capture([&](utils::array2d_view<float> channelsData) {
		channelMixer.saveChannelsData(channelsData, true);
		any = true;
	});

	if (any) {
		orchestrator.process(channelMixer);
		orchestrator.exchangeData(snapshot);
		channelMixer.reset();

		for (const auto& [procName, procInfo] : paramParser.getParseResult()) {
			auto& processingSnapshot = snapshot[procName];
			for (const auto& [handlerName, finisher] : procInfo.finishers) {
				for (auto& [channel, channelSnapshot] : processingSnapshot) {
					finisher(channelSnapshot[handlerName].handlerSpecificData);
				}
			}
		}
	}

	return deviceManager.getDeviceStatus();
}

void AudioParent::vCommand(isview bangArgs) {
	if (bangArgs == L"updateDevList") {
		deviceManager.getDeviceEnumerator().updateDeviceStringLegacy(deviceManager.getCurrentDeviceType());
		return;
	}

	logger.error(L"unknown command '{}'", bangArgs);
}

void AudioParent::vResolve(array_view<isview> args, string& resolveBufferString) {
	if (args.empty()) {
		logger.error(L"Invalid section variable: args needed");
		return;
	}

	const isview optionName = args[0];
	auto cl = logger.context(L"Invalid section variable '{}': ", optionName);

	if (optionName == L"current device") {
		if (args.size() < 2) {
			cl.error(L"need >= 2 argc, but only {} found", args.size());
			return;
		}

		const isview deviceProperty = args[1];

		if (deviceProperty == L"status") {
			resolveBufferString = deviceManager.getDeviceStatus() ? L"1" : L"0";
			return;
		}
		if (deviceProperty == L"status string") {
			resolveBufferString = deviceManager.getDeviceStatus() ? L"active" : L"down";
			return;
		}
		if (deviceProperty == L"type") {
			switch (deviceManager.getCurrentDeviceType()) {
			case utils::MediaDeviceType::eINPUT:
				resolveBufferString = L"input";
				return;
			case utils::MediaDeviceType::eOUTPUT:
				resolveBufferString = L"output";
				return;
			}
		}
		if (deviceProperty == L"name") {
			resolveBufferString = deviceManager.getDeviceInfo().fullFriendlyName;
			return;
		}
		if (deviceProperty == L"nameOnly") {
			resolveBufferString = deviceManager.getDeviceInfo().name;
			return;
		}
		if (deviceProperty == L"description") {
			resolveBufferString = deviceManager.getDeviceInfo().desc;
			return;
		}
		if (deviceProperty == L"id") {
			resolveBufferString = deviceManager.getDeviceInfo().id;
			return;
		}
		if (deviceProperty == L"format") {
			resolveBufferString = deviceManager.getCaptureManager().getFormatString();
			return;
		}

		return;
	}

	if (optionName == L"device list input") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListInput();
		return;
	}
	if (optionName == L"device list output") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListOutput();
		return;
	}

	if (optionName == L"value") {
		if (args.size() < 5) {
			cl.error(L"need >= 5 argc, but only {} found", args.size());
			return;
		}

		const auto procName = args[1];
		const auto channelName = args[2];
		const auto handlerName = args[3];
		const auto ind = utils::Option{ args[4] }.asInt(0);

		auto channelOpt = Channel::channelParser.find(channelName);
		if (!channelOpt.has_value()) {
			cl.error(L"channel '{}' is not recognized", channelName);
			return;
		}

		// auto procIter = saMap.find(procName);
		// if (procIter == saMap.end()) {
		// 	cl.error(L"processing '{}' is not found", procName);
		// 	return;
		// }
		//
		// const auto value = procIter->second.getAudioChildHelper().getValue(channelOpt.value(), handlerName, ind);

		const auto value = getValue(procName, handlerName, channelOpt.value(), ind);
		cl.printer.print(value);

		resolveBufferString = cl.printer.getBufferView();
		return;
	}

	if (optionName == L"handler info") {
		if (args.size() < 5) {
			cl.error(L"need >= 5 argc, but only {} found", args.size());
			return;
		}

		const auto procName = args[1];
		const auto channelName = args[2];
		const auto handlerName = args[3];
		const auto propName = args[4];

		auto channelOpt = Channel::channelParser.find(channelName);
		if (!channelOpt.has_value()) {
			cl.error(L"channel '{}' is not recognized", channelName);
			return;
		}

		// auto procIter = saMap.find(procName);
		// if (procIter == saMap.end()) {
		// 	cl.error(L"processing '{}' is not found", procName);
		// 	return;
		// }
		//
		// auto handler = procIter->second.getAudioChildHelper().findHandler(channelOpt.value(), handlerName);
		// if (handler == nullptr) {
		// 	cl.error(L"handler '{}:{}:{}' is not found", procName, channelName, handlerName);
		// 	return;
		// }
		//
		// const bool found = handler->vGetProp(propName, cl.printer);

		const bool found = orchestrator.getProp(procName, handlerName, channelOpt.value(), propName, cl.printer);
		if (!found) {
			cl.error(L"prop '{}:{}' is not found", handlerName, propName);
			return;
		}

		resolveBufferString = cl.printer.getBufferView();
		return;
	}


	legacy_resolve(args, resolveBufferString);
}

double AudioParent::getValue(isview proc, isview id, Channel channel, index ind) const {
	auto procIter = snapshot.find(proc);
	if (procIter == snapshot.end()) {
		return 0.0;
	}

	const auto& processingSnapshot = procIter->second;
	auto channelSnapshotIter = processingSnapshot.find(channel);
	if (channelSnapshotIter == processingSnapshot.end()) {
		return 0.0;

	}

	auto& channelSnapshot = channelSnapshotIter->second;
	auto handlerSnapshotIter = channelSnapshot.find(id);
	if (handlerSnapshotIter == channelSnapshot.end()) {
		return 0.0;

	}

	auto& values = handlerSnapshotIter->second.values;
	const auto layersCount = values.getBuffersCount();
	if (layersCount == 0) {
		return 0.0;
	}
	const auto valuesCount = values.getBufferSize();
	if (ind >= valuesCount) {
		return 0.0;
	}

	return values[0][ind];
}

string AudioParent::checkHandler(isview procName, Channel channel, isview handlerName) const {
	utils::BufferPrinter bp;

	const auto procDataIter = paramParser.getParseResult().find(procName);
	if (procDataIter == paramParser.getParseResult().end()) {
		bp.print(L"processing {} is not found", procName);
		return bp.getBufferPtr();
	}

	auto procData = procDataIter->second;
	auto& channels = procData.channels;
	if (channels.find(channel) == channels.end()) {
		bp.print(L"processing {} doesn't have channel {}", procName, channel.technicalName());
		return bp.getBufferPtr();
	}

	auto& handlerMap = procData.handlersInfo.patchers;
	if (handlerMap.find(handlerName) == handlerMap.end()) {
		bp.print(L"processing {} doesn't have handler {}", procName, handlerName);
		return bp.getBufferPtr();
	}

	return { };
}

isview AudioParent::legacy_findProcessingFor(isview handlerName) {
	for (auto& [name, pd] : paramParser.getParseResult()) {
		auto& patchers = pd.handlersInfo.patchers;
		if (patchers.find(handlerName) != patchers.end()) {
			return name;
		}
	}

	return { };
}

void AudioParent::legacy_resolve(array_view<isview> args, string& resolveBufferString) {
	const isview optionName = args[0];
	auto cl = logger.context(L"Invalid section variable '{}': ", optionName);

	if (optionName == L"device list") {
		resolveBufferString = deviceManager.getDeviceEnumerator().getDeviceListLegacy();
		return;
	}

	if (optionName == L"prop") {
		if (args.size() < 4) {
			cl.error(L"need >= 4 argc, but only {} found", args.size());
			return;
		}

		const auto channelName = args[1];
		const auto handlerName = args[2];
		const auto propName = args[3];

		auto channelOpt = Channel::channelParser.find(channelName);
		if (!channelOpt.has_value()) {
			cl.error(L"channel '{}' is not recognized", channelName);
			return;
		}

		auto procName = legacy_findProcessingFor(handlerName);
		if (procName.empty()) {
			cl.error(L"handler '{}' is not found", handlerName);
			return;
		}

		const bool found = orchestrator.getProp(propName, handlerName, channelOpt.value(), propName, cl.printer);
		if (!found) {
			cl.error(L"prop '{}:{}' is not found", handlerName, propName);
			return;
		}

		resolveBufferString = cl.printer.getBufferView();
		return;
	}

	cl.error(L"option is not supported");
}
