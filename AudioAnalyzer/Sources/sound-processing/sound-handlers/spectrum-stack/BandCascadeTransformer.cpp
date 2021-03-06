﻿/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

#include "BandCascadeTransformer.h"
#include "BandResampler.h"
#include "ResamplerProvider.h"

using namespace audio_analyzer;

SoundHandler::ParseResult BandCascadeTransformer::parseParams(
	const OptionMap& om, Logger& cl, const Rainmeter& rain,
	index legacyNumber
) const {
	ParseResult result{ true };
	auto& params = result.params.clear<Params>();

	const auto sourceId = om.get(L"source").asIString();
	if (sourceId.empty()) {
		cl.error(L"source not found");
		return { };
	}

	const double epsilon = std::numeric_limits<float>::epsilon();

	params.minWeight = om.get(L"minWeight").asFloat(0.1);
	params.minWeight = std::max(params.minWeight, epsilon);

	params.targetWeight = om.get(L"targetWeight").asFloat(2.5);
	params.targetWeight = std::max(params.targetWeight, params.minWeight);

	if (legacyNumber < 104) {
		auto zeroLevel = om.get(L"zeroLevelMultiplier").asFloat(1.0);
		zeroLevel = std::max(zeroLevel, 0.0);
		zeroLevel = zeroLevel * 0.66 * epsilon;

		params.zeroLevelHard = om.get(L"zeroLevelHardMultiplier").asFloat(0.01);
		params.zeroLevelHard = std::clamp(params.zeroLevelHard, 0.0, 1.0) * zeroLevel;

		// touch 'weightFallback' option to prevent log warning
		(void)om.get(L"weightFallback");
	} else {
		params.zeroLevelHard = om.get(L"zeroLevelMultiplier").asFloat(1.0);
		params.zeroLevelHard = std::max(params.zeroLevelHard, 0.0);
		params.zeroLevelHard *= epsilon;
	}

	if (const auto mixFunctionString = om.get(L"mixFunction").asIString(L"product");
		mixFunctionString == L"product") {
		params.mixFunction = MixFunction::PRODUCT;
	} else if (mixFunctionString == L"average") {
		params.mixFunction = MixFunction::AVERAGE;
	} else {
		cl.warning(L"mixFunction '{}' is not recognized, assume 'product'", mixFunctionString);
		params.mixFunction = MixFunction::PRODUCT;
	}

	result.sources.emplace_back(sourceId);
	return result;
}

SoundHandler::ConfigurationResult
BandCascadeTransformer::vConfigure(const ParamsContainer& _params, Logger& cl, ExternalData& externalData) {
	params = _params.cast<Params>();

	auto& config = getConfiguration();
	const auto provider = dynamic_cast<ResamplerProvider*>(config.sourcePtr);
	if (provider == nullptr) {
		cl.error(L"invalid source: BandResampler is not found in the handler chain");
		return { };
	}

	resamplerPtr = provider->getResampler();
	if (resamplerPtr == nullptr) {
		cl.error(L"BandResampler is not found in the source chain");
		return { };
	}

	const auto dataSize = config.sourcePtr->getDataSize();
	snapshot.clear();
	snapshot.resize(dataSize.layersCount);

	return { dataSize.valuesCount, { config.sourcePtr->getDataSize().eqWaveSizes[0] } };
}

void BandCascadeTransformer::vProcess(ProcessContext context, ExternalData& externalData) {
	auto& source = *getConfiguration().sourcePtr;

	const index layersCount = source.getDataSize().layersCount;

	for (index i = 0; i < layersCount; i++) {
		auto& meta = snapshot[i];
		meta.nextChunkIndex = 0;
		meta.data = source.getSavedData(i);
	}

	auto& eqWS = source.getDataSize().eqWaveSizes;

	for (auto chunk : source.getChunks(0)) {
		auto dest = pushLayer(0);

		if (clock::now() > context.killTime) {
			auto myChunks = getChunks(0);
			myChunks.remove_suffix(1);
			dest.copyFrom(myChunks.empty() ? getSavedData(0) : myChunks.back());
			continue;
		}

		for (index i = 0; i < layersCount; i++) {
			auto& meta = snapshot[i];
			meta.offset -= eqWS[0];

			auto layerChunks = source.getChunks(i);
			if (meta.nextChunkIndex >= layerChunks.size() || meta.offset >= 0) {
				continue;
			}

			meta.data = layerChunks[meta.nextChunkIndex];
			meta.nextChunkIndex++;
			meta.offset += eqWS[i];
			meta.maxValue = *std::max_element(meta.data.begin(), meta.data.end());
		}

		for (index band = 0; band < dest.size(); ++band) {
			dest[band] = computeForBand(band);
		}
	}
}

float BandCascadeTransformer::computeForBand(index band) const {
	const BandResampler& resampler = *resamplerPtr;

	float weight = 0.0f;
	float cascadesSummed = 0.0f;

	float valueProduct = 1.0f;
	float valueSum = 0.0f;

	const auto bandWeights = resampler.getBandWeights(band);

	for (index cascade = 0; cascade < index(snapshot.size()); cascade++) {
		const float bandWeight = bandWeights[cascade];
		const float magnitude = snapshot[cascade].data[band];

		if (snapshot[cascade].maxValue <= params.zeroLevelHard) {
			// this most often happens when there was a silence, and then some sound,
			// so cascade with index more than N haven't updated yet
			// so in that case we ignore values of all these cascades

			if (cascadesSummed == 0.0f) {
				// if cascadesSummed == 0.0f then bandWeight < params.minWeight for all previous cascades
				// let's use value of previous cascade to provide at least something
				return cascade == 0 ? 0.0f : snapshot[cascade - 1].data[band];
			}
			break;
		}

		if (bandWeight < params.minWeight) {
			continue;
		}

		cascadesSummed += 1.0f;

		valueProduct *= magnitude;
		valueSum += magnitude;
		weight += bandWeight;

		if (weight >= params.targetWeight) {
			break;
		}
	}

	if (cascadesSummed == 0.0f) {
		// bandWeight < params.minWeight for all cascades
		// let's use value of the last cascade with non zero weight and value
		for (index i = bandWeights.size() - 1; i >= 0; ++i) {
			if (bandWeights[i] > 0.0 && snapshot[i].data[band] > params.zeroLevelHard) {
				return snapshot[i].data[band];
			}
		}
		return 0.0;
	}

	return params.mixFunction == MixFunction::PRODUCT
		       ? std::pow(valueProduct, 1.0f / cascadesSummed)
		       : valueSum / cascadesSummed;
}
