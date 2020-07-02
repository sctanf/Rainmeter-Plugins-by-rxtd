/*
 * Copyright (C) 2019 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "TypeHolder.h"

#include "undef.h"

using namespace utils;

std::map<Rainmeter::Skin, std::map<istring, ParentBase*, std::less<>>> ParentBase::skinMap { };

TypeHolder::TypeHolder(Rainmeter&& rain) : rain(std::move(rain)), logger(this->rain.getLogger()) {

}

void TypeHolder::_command(isview  bangArgs) {
	logger.warning(L"Measure does not have commands");
}
void TypeHolder::_resolve(int argc, const wchar_t* argv[], string& resolveBufferString) {
}

void TypeHolder::setMeasureState(MeasureState brokenState) {
	this->measureState = brokenState;
}

void TypeHolder::setUseResultString(bool value) {
	useResultString = value;
}

double TypeHolder::update() {
	if (measureState != MeasureState::eWORKING) {
		return 0.0;
	}

	resultDouble = _update();
	if (useResultString) {
		resultString = { };
		_updateString(resultString);
	}
	return resultDouble;
}

void TypeHolder::reload() {
	if (measureState == MeasureState::eBROKEN) { // skip reload only if the measure is unrecoverable
		return;
	}

	measureState = MeasureState::eWORKING;
	_reload();
}

void TypeHolder::command(const wchar_t *bangArgs) {
	if (measureState != MeasureState::eWORKING) {
		logger.warning(L"Skipping bang on a broken measure");
		return;
	}

	_command(bangArgs);
}

const wchar_t* TypeHolder::resolve(int argc, const wchar_t* argv[]) {
	if (measureState != MeasureState::eWORKING) {
		logger.warning(L"Skipping resolve on a broken measure");
		return L"";
	}

	resolveString = { };
	_resolve(argc, argv, resolveString);

	return resolveString.c_str();
}

const wchar_t* TypeHolder::getString() const {
	if (measureState != MeasureState::eWORKING) {
		return L"broken";
	}

	if (useResultString) {
		return resultString.c_str();
	} else {
		return nullptr;
	}
}

MeasureState TypeHolder::getState() const {
	return measureState;
}

ParentBase::ParentBase(Rainmeter&& rain): TypeHolder(std::move(rain)) {
	skinMap[this->rain.getSkin()][this->rain.getMeasureName() % ciView() % own()] = this;
}

ParentBase::~ParentBase() {
	const auto skinIter = skinMap.find(this->rain.getSkin());
	if (skinIter == skinMap.end()) {
		std::terminate();
	}
	auto& measuresMap = skinIter->second;

	const auto measureIter = measuresMap.find(this->rain.getMeasureName() % ciView());
	if (measureIter == measuresMap.end()) {
		std::terminate();
	}
	measuresMap.erase(measureIter);

	if (measuresMap.empty()) {
		skinMap.erase(skinIter);
	}
}

ParentBase* ParentBase::findParent(Rainmeter::Skin skin, isview measureName) {
	const auto skinIter = skinMap.find(skin);
	if (skinIter == skinMap.end()) {
		return nullptr;
	}
	const auto& measuresMap = skinIter->second;

	const auto measureIter = measuresMap.find(measureName);
	if (measureIter == measuresMap.end()) {
		return nullptr;
	}

	auto result = measureIter->second;
	if (result->getState() == MeasureState::eBROKEN) {
		return nullptr;
	}
	return result;
}
