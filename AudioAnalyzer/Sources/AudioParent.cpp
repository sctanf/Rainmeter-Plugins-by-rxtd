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
		callAllSA([=](SoundAnalyzer& sa) {
			sa.setSourceRate(format.samplesPerSec);
			sa.setLayout(format.channelLayout);
		});
		currentFormat = std::move(format);
	}) {
	setUseResultString(false);

	if (deviceManager.getState() == DeviceManager::State::eFATAL) {
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}

	notificationClient = {
		[=](auto ptr) {
			*ptr = new utils::CMMNotificationClient{
				[=](sview id) {
					notificationCallback(id);
				},
				deviceManager.getDeviceEnumerator().getWrapper()
			};
			return true;
		}
	};

	deviceManager.getDeviceEnumerator().updateDeviceStrings();

	paramParser.setRainmeter(rain);
}

void AudioParent::_reload() {
	const auto source = rain.read(L"Source").asIString();
	string id = { };
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

	computeTimeout = rain.read(L"computeTimeout").asFloat(4.0);
	finishTimeout = rain.read(L"finishTimeout").asFloat(2.0);

	deviceManager.setOptions(sourceEnum, id);

	paramParser.parse();

	patchSA(paramParser.getParseResult());
}

double AudioParent::_update() {
	const bool eventHappened = notificationCheck();
	if (eventHappened) {
		deviceManager.checkAndRepair();

		if (deviceManager.getState() == DeviceManager::State::eOK) {
			deviceManager.getDeviceEnumerator().updateDeviceStrings();
		}
	}

	if (deviceManager.getState() != DeviceManager::State::eOK) {
		if (deviceManager.getState() == DeviceManager::State::eFATAL) {
			setMeasureState(utils::MeasureState::eBROKEN);
			logger.error(L"Unrecoverable error");
		}

		callAllSA([=](SoundAnalyzer& sa) {
			sa.resetValues();
		});
	} else {
		using clock = std::chrono::high_resolution_clock;
		static_assert(clock::is_steady);

		deviceManager.getCaptureManager().capture([&](bool silent, array_view<std::byte> buffer) {
			if (!silent) {
				channelMixer.decomposeFramesIntoChannels(buffer, true);
			} else {
				channelMixer.writeSilence(buffer.size(), true);
			}
		});

		const std::chrono::duration<float, std::milli> duration{ computeTimeout };
		const auto maxTime = clock::now() + std::chrono::duration_cast<std::chrono::milliseconds>(duration);

		const std::chrono::duration<float, std::milli> dur{ 10.0 };
		clock::time_point killTime = maxTime + std::chrono::duration_cast<std::chrono::milliseconds>(dur);

		callAllSA([=](SoundAnalyzer& sa) {
			sa.process(channelMixer);
		});
		channelMixer.reset();

		const auto now = clock::now();

		// if (now >= killTime) {
		// 	const std::chrono::duration<float, std::milli> overheadTime = now - maxTime;
		// 	logger.warning(L"killed: {} ms overhead over specified {} ms", overheadTime.count(), duration);
		// 	return;
		// }

		if (now >= maxTime) {
			const std::chrono::duration<float, std::milli> overheadTime = now - maxTime;
			logger.debug(L"compute timeout: {} ms overhead over specified {} ms", overheadTime.count(), computeTimeout);
		}

		const auto maxFinishTime = clock::now() + std::chrono::duration_cast<std::chrono::milliseconds>(
			std::chrono::duration<float, std::milli>{ finishTimeout }
		);

		callAllSA([=](SoundAnalyzer& sa) {
			sa.finishStandalone();
		});

		const auto finishTime = clock::now();
		if (finishTime > maxFinishTime) {
			const std::chrono::duration<float, std::milli> overheadTime = finishTime - maxFinishTime;
			logger.notice(L"finish time overhead {} ms over specified {} ms", overheadTime.count(), finishTimeout);
		}
	}

	return deviceManager.getDeviceStatus();
}

void AudioParent::_command(isview bangArgs) {
	if (bangArgs == L"updateDevList") {
		deviceManager.getDeviceEnumerator().updateDeviceStringLegacy(deviceManager.getCurrentDeviceType());
		return;
	}

	logger.error(L"unknown command '{}'", bangArgs);
}

void AudioParent::_resolve(array_view<isview> args, string& resolveBufferString) {
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
			default: ;
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

		auto procIter = saMap.find(procName);
		if (procIter == saMap.end()) {
			cl.error(L"processing '{}' is not found", procName);
			return;
		}

		const auto value = procIter->second.getAudioChildHelper().getValue(channelOpt.value(), handlerName, ind);
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

		auto procIter = saMap.find(procName);
		if (procIter == saMap.end()) {
			cl.error(L"processing '{}' is not found", procName);
			return;
		}

		auto handler = procIter->second.getAudioChildHelper().findHandler(channelOpt.value(), handlerName);
		if (handler == nullptr) {
			cl.error(L"handler '{}:{}:{}' is not found", procName, channelName, handlerName);
			return;
		}

		const bool found = handler->getProp(propName, cl.printer);
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
	auto procIter = saMap.find(proc);
	if (procIter == saMap.end()) {
		return 0.0;
	}
	return procIter->second.getAudioChildHelper().getValue(channel, id, ind);
}

double AudioParent::legacy_getValue(isview id, Channel channel, index ind) const {
	auto [handler, helper] = findHandlerByName(id, channel);
	if (handler == nullptr) {
		return 0.0;
	}
	return helper.getValueFrom(handler, channel, ind);
}

void AudioParent::patchSA(ParamParser::ProcessingsInfoMap procs) {
	std::set<istring> toDelete;
	for (auto& [name, ptr] : saMap) {
		if (procs.find(name) == procs.end()) {
			toDelete.insert(name);
		}
	}
	for (auto& name : toDelete) {
		saMap.erase(name);
	}

	for (auto& [name, data] : procs) {
		auto& sa = saMap[name];
		sa.getCPH().setTargetRate(data.targetRate);
		sa.getCPH().setFCC(std::move(data.fcc));
		sa.setHandlers(data.channels, data.handlersInfo);
		sa.setSourceRate(currentFormat.samplesPerSec);
		sa.setLayout(currentFormat.channelLayout);
		sa.setGranularity(data.granularity);
	}
}

std::pair<SoundHandler*, AudioChildHelper>
AudioParent::findHandlerByName(isview name, Channel channel) const {
	for (auto& [_, analyzer] : saMap) {
		auto handler = analyzer.getAudioChildHelper().findHandler(channel, name);
		if (handler != nullptr) {
			return { handler, analyzer.getAudioChildHelper() };
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

		auto [handler, helper] = findHandlerByName(handlerName, channelOpt.value());
		if (handler == nullptr) {
			cl.error(L"handler '{}:{}' is not found", channelName, handlerName);
			return;
		}

		const bool found = handler->getProp(propName, cl.printer);
		if (!found) {
			cl.error(L"prop '{}:{}' is not found", handlerName, propName);
			return;
		}

		resolveBufferString = cl.printer.getBufferView();
		return;
	}

	cl.error(L"option is not supported");
}
