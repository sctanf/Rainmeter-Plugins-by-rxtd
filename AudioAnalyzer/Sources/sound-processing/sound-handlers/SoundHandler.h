/*
 * Copyright (C) 2019-2020 rxtd
 *
 * This Source Code Form is subject to the terms of the GNU General Public
 * License; either version 2 of the License, or (at your option) any later
 * version. If a copy of the GPL was not distributed with this file, You can
 * obtain one at <https://www.gnu.org/licenses/gpl-2.0.html>.
 */

/*
 Handler life cycle
 First of all, handler should parse its parameters. It happens in the method #parseParams(),
 which is called on an empty object.
 If params are invalid, then parseParams should return invalid object,
 so that handler with these params will not be created.
 Then the main life cycle happens:
 1. Function #setParams is called.
		This function should only save parameters, any external info in the moment of time when this function is called is considered invalid
 2. Function #vConfigure is called
		At this point handler should do all calculations required to work.
		Handler may call #vConfigure to get external info
		If something fails, handler should return invalid object, and the it is invalidated until next params change
 3. vProcess happens in the loop
 4. Data is accessed from other handlers and child measures.
		Before data is accessed vFinish method is called. // todo remove
 */

#pragma once
#include <any>

#include "array_view.h"
#include "BufferPrinter.h"
#include "RainmeterWrappers.h"
#include "Vector2D.h"
#include "option-parser/OptionMap.h"

namespace rxtd::audio_analyzer {
	class SoundHandler;

	class HandlerFinder {
	public:
		virtual ~HandlerFinder() = default;

		[[nodiscard]]
		virtual SoundHandler* getHandler(isview id) const = 0;
	};

	class SoundHandler {
	public:
		struct DataSize {
			index layersCount{ };
			index valuesCount{ };

			DataSize() = default;

			DataSize(index layersCount, index valuesCount) : layersCount(layersCount), valuesCount(valuesCount) {
			}

			[[nodiscard]]
			bool isEmpty() const {
				return layersCount == 0 || valuesCount == 0;
			}
		};

		class ParseResult {
			bool valid{ };
			std::any _params;

		public:
			ParseResult() {
				valid = false;
			}

			template <typename Params>
			ParseResult(Params params) {
				valid = true;
				_params = std::move(params);
			}

			[[nodiscard]]
			bool isValid() const {
				return valid;
			}

			[[nodiscard]]
			std::any takeParams() {
				return std::move(_params);
			}
		};

		struct DataChunk {
			index equivalentWaveSize{ };
			array_view<float> data;
		};

	protected:
		using OptionMap = utils::OptionMap;
		using Rainmeter = utils::Rainmeter;
		using Logger = utils::Rainmeter::Logger;

		struct Configuration {
			SoundHandler* sourcePtr = nullptr;
			index sampleRate{ };
			sview channelName{ };
		};

		struct ConfigurationResult {
			bool success = false;
			DataSize dataSize{ };

			ConfigurationResult() = default;

			ConfigurationResult(DataSize dataSize): success(true), dataSize(dataSize) {
			}

			ConfigurationResult(index layersCount, index valuesCount):
				ConfigurationResult(DataSize{ layersCount, valuesCount }) {
			}
		};

	public:
		template <typename _HandlerType>
		[[nodiscard]]
		static std::unique_ptr<SoundHandler> patchHandlerImpl(std::unique_ptr<SoundHandler> handlerPtr) {
			using HandlerType = _HandlerType;

			SoundHandler* ptr = dynamic_cast<HandlerType*>(handlerPtr.get());
			if (ptr == nullptr) {
				ptr = new HandlerType();
				handlerPtr = std::unique_ptr<SoundHandler>{ ptr };
			}

			return handlerPtr;
		}

	private:
		struct LayerCache {
			struct ChunkInfo {
				index offset{ };
				index equivalentWaveSize{ };
			};

			mutable std::vector<DataChunk> chunks;
			std::vector<ChunkInfo> meta;
		};

		DataSize _dataSize{ };
		mutable bool _layersAreValid = false;
		std::vector<float> _buffer;
		std::vector<LayerCache> _layers;
		utils::Vector2D<float> _lastResults;

		Configuration _configuration{ };

	public:
		virtual ~SoundHandler() = default;

	protected:
		template <typename Params>
		static bool compareParamsEquals(Params p1, const std::any& p2) {
			return p1 == *std::any_cast<Params>(&p2);
		}

	public:
		// should return true when params are the same
		[[nodiscard]]
		virtual bool checkSameParams(const std::any& p) const = 0;
		virtual void setParams(const std::any& p) = 0;

		[[nodiscard]]
		virtual ParseResult
		parseParams(const OptionMap& om, Logger& cl, const Rainmeter& rain, index legacyNumber) const = 0;

		// returns true on success, false on invalid handler
		[[nodiscard]]
		bool patch(
			const std::any& params,
			sview channelName, index sampleRate,
			HandlerFinder& hf,
			Logger& cl
		);

	protected:
		[[nodiscard]]
		virtual isview vGetSourceName() const = 0;

		[[nodiscard]]
		virtual ConfigurationResult vConfigure(Logger& cl) = 0;

		[[nodiscard]]
		array_span<float> pushLayer(index layer, index equivalentWaveSize) {
			const index offset = index(_buffer.size());
			_buffer.resize(offset + _dataSize.valuesCount);
			_layersAreValid = false;

			_layers[layer].meta.push_back({ offset, equivalentWaveSize });

			return { _buffer.data() + offset, _dataSize.valuesCount };
		}

	private:
		void inflateLayers() const {
			if (_layersAreValid) {
				return;
			}

			for (auto& data : _layers) {
				data.chunks.resize(data.meta.size());
				for (index i = 0; i < index(data.meta.size()); i++) {
					data.chunks[i].equivalentWaveSize = data.meta[i].equivalentWaveSize;
					data.chunks[i].data = { _buffer.data() + data.meta[i].offset, _dataSize.valuesCount };
				}
			}

			_layersAreValid = true;
		}

	public:
		[[nodiscard]]
		DataSize getDataSize() const {
			return _dataSize;
		}

		[[nodiscard]]
		array_view<DataChunk> getChunks(index layer) const {
			if (layer >= _dataSize.layersCount) {
				return { };
			}

			inflateLayers();
			return _layers[layer].chunks;
		}

		[[nodiscard]]
		array_view<float> getLastData(index layer) const {
			if (layer >= _dataSize.layersCount) {
				return { };
			}

			inflateLayers();
			auto chunks = _layers[layer].chunks;
			if (!chunks.empty()) {
				return chunks.back().data;
			}

			return _lastResults[layer];
		}

		[[nodiscard]]
		array_view<float> getSavedData(index layer) const {
			if (layer >= _dataSize.layersCount) {
				return { };
			}

			return _lastResults[layer];
		}

		void process(array_view<float> wave) {
			vProcess(wave);
		}

		// returns true on success, false on failure
		bool finish() {
			vFinish();
			return true; // todo
		}

		void purgeCache() {
			for (index layer = 0; layer < _dataSize.layersCount; layer++) {
				auto chunks = _layers[layer].chunks;
				if (chunks.empty()) {
					continue;
				}

				auto chunkData = chunks.back().data;
				std::copy(chunkData.begin(), chunkData.end(), _lastResults[layer].begin());
			}

			for (auto& data : _layers) {
				data.meta.clear();
			}

			_layersAreValid = false;

			_buffer.clear();
		}

		void reset() {
			// todo do we even need reset at all?
			purgeCache();
			vReset();
		}

		[[nodiscard]]
		virtual index getStartingLayer() const {
			return 0;
		}

		// return true if such prop exists, false otherwise
		[[nodiscard]]
		virtual bool vGetProp(const isview& prop, utils::BufferPrinter& printer) const {
			return false;
		}

		[[nodiscard]]
		virtual bool vIsStandalone() {
			return false;
		}

	protected:
		[[nodiscard]]
		const Configuration& getConfiguration() const {
			return _configuration;
		}

		virtual void vProcess(array_view<float> wave) = 0;

		// Method can be called several times in a row, handler should check for changes for optimal performance
		// returns true on success, false on failure
		virtual void vFinish() {
		}

		virtual void vReset() {
		}

		static index legacy_parseIndexProp(const isview& request, const isview& propName, index endBound) {
			return legacy_parseIndexProp(request, propName, 0, endBound);
		}

		static index legacy_parseIndexProp(
			const isview& request,
			const isview& propName,
			index minBound, index endBound
		);
	};
}
