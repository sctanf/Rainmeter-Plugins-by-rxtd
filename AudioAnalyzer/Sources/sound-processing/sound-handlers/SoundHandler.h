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
 First of all, handler should parse its parameters. It happens in the static method #parseParams().
 Then the main life cycle happens, which consists of several phases.
 1. Function #patchMe function is called.
		Handler should assume that:
			- only its params are known and valid
			- any external resources that it could potentially have links to don't exist anymore
			- sample rate and channel are invalid

 2. Function #vFinishLinking is called
		At this point:
			- Channel and sample rate are known
			- If handler requested a source via returned value of #getSource function, the source exists and valid
		Handler must recalculate data that depend on values above.
		If something fails, handler is invalidated until next params change
 3. vProcess happens in the loop
 4. Data is accessed from other handlers and child measures.
		Before data is accessed vFinish method is called. // todo remove
 */

#pragma once
#include <atomic>

#include "array2d_view.h"
#include "array_view.h"
#include "BufferPrinter.h"
#include "RainmeterWrappers.h"
#include "Vector2D.h"
#include "../Channel.h"

namespace rxtd::audio_analyzer {
	class SoundHandler;

	class HandlerFinder {
	public:
		virtual ~HandlerFinder() = default;

		[[nodiscard]]
		virtual SoundHandler* getHandler(isview id) const = 0;
	};

	class HandlerPatcher {
		friend SoundHandler;

	public:
		virtual ~HandlerPatcher() = default;

	protected:
		// takes old pointer and returns new
		//	- if returned pointer is different from the old, then new object was created,
		//		and the caller must release resources associated with old pointer
		//
		//	- if returned pointer is the same, then the old object was reused
		virtual SoundHandler* patch(SoundHandler* handlerPtr) const = 0;
	};

	class LayerDataId {
		using idType = uint32_t;

		static std::atomic<idType> sourceIdCounter;

		idType sourceId{ };
		idType dataId{ };

		LayerDataId(idType handlerId, idType dataId) : sourceId(handlerId), dataId(dataId) {
		}

	public:
		LayerDataId() = default;

		static LayerDataId createNext() {
			const auto nextId = sourceIdCounter.fetch_add(1);
			return { nextId + 1, 0 };
		}

		void advance() {
			dataId++;
		}

		void assign(LayerDataId other) {
			dataId = other.dataId;
		}

		// autogenerated
		friend bool operator==(const LayerDataId& lhs, const LayerDataId& rhs) {
			return lhs.sourceId == rhs.sourceId
				&& lhs.dataId == rhs.dataId;
		}

		friend bool operator!=(const LayerDataId& lhs, const LayerDataId& rhs) {
			return !(lhs == rhs);
		}

		// LayerDataId(const LayerDataId& other) = default;
		// LayerDataId& operator=(const LayerDataId& other) = default;
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

		struct LayeredData2 {
			utils::array2d_view<float> values;
			array_view<LayerDataId> ids;
		};

	protected:
		using OptionMap = utils::OptionMap;
		using Rainmeter = utils::Rainmeter;
		using Logger = utils::Rainmeter::Logger;

		struct LinkingResult {
			bool success = false;
			DataSize dataSize{ };

			LinkingResult() = default;

			LinkingResult(DataSize dataSize): success(true), dataSize(dataSize) {
			}

			LinkingResult(index layersCount, index valuesCount): LinkingResult(DataSize{ layersCount, valuesCount }) {
			}
		};

	public:
		template <typename HandlerType>
		class HandlerPatcherImpl : public HandlerPatcher {
			typename HandlerType::Params params{ };
			bool valid = false;

		public:
			HandlerPatcherImpl(const OptionMap& optionMap, Logger& cl, const Rainmeter& rain, index legacyNumber) {
				HandlerType temp1{ };
				SoundHandler& temp2 = temp1;
				valid = temp2.parseParams(optionMap, cl, rain, &params, legacyNumber);
			}

			[[nodiscard]]
			bool isValid() const {
				return valid;
			}

			virtual ~HandlerPatcherImpl() = default;

		private:
			[[nodiscard]]
			SoundHandler* patch(SoundHandler* handlerPtr) const override {
				auto ptr = dynamic_cast<HandlerType*>(handlerPtr);
				if (ptr == nullptr) {
					ptr = new HandlerType();
				}

				if (ptr->getParams() != params) {
					ptr->setParams(params);
				}

				return ptr;
			}
		};

	private:
		LayerDataId _generatorId;
		DataSize _dataSize{ };
		utils::Vector2D<float> _values;
		std::vector<LayerDataId> _idsRef;
		std::vector<LayerDataId> _ids;

		SoundHandler* _sourceHandlerPtr = nullptr;
		index _sampleRate{ };
		Channel _channel{ };

	public:
		SoundHandler() {
			_generatorId = LayerDataId::createNext();
		}

		virtual ~SoundHandler() = default;

	private:
		// All derived classes should have methods with following signatures

		/*
		const Params& getParams() const;
		void patchMe(const Params& _params);
		 */

		// I can't declare these as virtual functions
		// because Params structs are defined in derived classes,
		// and C++ doesn't support template virtual functions

		template <typename>
		friend class HandlerPatcherImpl;

		// must return true if all options are valid, false otherwise
		virtual bool parseParams(
			const OptionMap& om, Logger& cl, const Rainmeter& rain, void* paramsPtr, index legacyNumber
		) const = 0;

	public:
		[[nodiscard]]
		static SoundHandler* patch(
			SoundHandler* old, HandlerPatcher& patcher,
			Channel channel, index sampleRate,
			HandlerFinder& hf,
			Logger& cl
		);

	protected:
		[[nodiscard]]
		virtual isview vGetSourceName() const = 0;

		// method should return false if check failed, true otherwise
		[[nodiscard]]
		virtual LinkingResult vFinishLinking(Logger& cl) = 0;

		// push new data
		// implies that handler generates data on it's own, and the data doesn't have any particular source
		// maybe source is complex, maybe handler resamples data
		[[nodiscard]]
		array_span<float> generateLayerData(index layer) {
			_generatorId.advance();
			_ids[layer] = _generatorId;
			return _values[layer];
		}

		// push new data
		// implies that handler takes data from the source and transforms it somehow
		[[nodiscard]]
		array_span<float> updateLayerData(index layer, LayerDataId sourceId) {
			_idsRef[layer] = sourceId;
			_ids[layer].assign(sourceId);
			return _values[layer];
		}

		[[nodiscard]]
		array_view<LayerDataId> getRefIds() const {
			return _idsRef;
		}

	public:
		[[nodiscard]]
		DataSize getDataSize() const {
			return _dataSize;
		}

		[[nodiscard]]
		LayeredData2 getData() const {
			return { _values, _ids };
		}

		void process(array_view<float> wave) {
			vProcess(wave);
		}

		// returns true on success, false on failure
		bool finish() {
			vFinish();
			return true; // todo
		}

		void reset() {
			// todo do we even need reset at all?
			_values.init(0.0f);
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
		SoundHandler* getSource() const {
			return _sourceHandlerPtr;
		}

		[[nodiscard]]
		index getSampleRate() const {
			return _sampleRate;
		}

		[[nodiscard]]
		Channel getChannel() const {
			return _channel;
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
