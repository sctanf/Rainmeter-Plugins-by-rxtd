/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "AudioChild.h"
#include "option-parser/OptionList.h"

#include "undef.h"

using namespace audio_analyzer;

AudioChild::AudioChild(utils::Rainmeter&& _rain) : TypeHolder(std::move(_rain)) {
	const auto parentName = rain.readString(L"Parent") % ciView();
	if (parentName == L"") {
		logger.error(L"Parent must be specified");
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
	parent = AudioParent::findInstance(rain.getSkin(), parentName);

	if (parent == nullptr) {
		logger.error(L"Parent '{}' is not found or broken", parentName);
		setMeasureState(utils::MeasureState::eBROKEN);
		return;
	}
}

void AudioChild::_reload() {
	const auto channelStr = rain.readString(L"Channel");

	if (channelStr == L"") {
		channel = Channel::eAUTO;
	} else {
		auto channelOpt = Channel::channelParser.find(channelStr);
		if (!channelOpt.has_value()) {
			logger.error(L"Invalid Channel '{}', set to Auto.", channelStr);
			channel = Channel::eAUTO;
		} else {
			channel = channelOpt.value();
		}
	}

	valueId = rain.readString(L"ValueId");

	const auto stringValueStr = rain.readString(L"StringValue") % ciView();
	if (stringValueStr == L"" || stringValueStr == L"Number") {
		stringValueType = StringValue::eNUMBER;
	} else if (stringValueStr == L"Info") {
		stringValueType = StringValue::eINFO;

		auto requestList = rain.read(L"InfoRequest").asList(L',').own();

		infoRequest.clear();
		for (auto view : requestList) {
			infoRequest.emplace_back(view.asString());
		}

		infoRequestC.clear();
		for (const auto& str : infoRequest) {
			infoRequestC.push_back(str.c_str());
		}
	} else {
		logger.error(L"Invalid StringValue '{}', set to Number.", stringValueStr);
		stringValueType = StringValue::eNUMBER;
	}

	auto signedIndex = rain.read(L"Index").asInt();
	if (signedIndex < 0) {
		logger.error(L"Invalid Index {}. Index should be > 0. Set to 0.", signedIndex);
		signedIndex = 0;
	}
	valueIndex = static_cast<decltype(valueIndex)>(signedIndex);
}

std::tuple<double, const wchar_t*> AudioChild::_update() {

	double result = parent->getValue(valueId, channel, valueIndex);

	const wchar_t *stringRes;
	switch (stringValueType) {
	case StringValue::eNUMBER:
		stringRes = nullptr;
		break;

	case StringValue::eINFO:
		stringValue = parent->resolve(index(infoRequestC.size()), infoRequestC.data());
		stringRes = stringValue.c_str();
		break;

	default:
		logger.error(L"Unexpected stringValueType: '{}'", stringValueType);
		stringRes = nullptr;
		break;
	}

	return std::make_tuple(result, stringRes);
}
