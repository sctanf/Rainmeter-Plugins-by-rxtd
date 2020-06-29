#pragma once
#include "array_view.h"

namespace rxtd::utils {

	class IAudioCaptureClientWrapper;

	class AudioBuffer {
		friend IAudioCaptureClientWrapper;

		IAudioCaptureClientWrapper& parent;
		const index id;
		const bool silent{ };
		const array_view<std::byte> buffer;

		AudioBuffer(IAudioCaptureClientWrapper& parent, index id, bool silent, const std::byte* buffer, uint32_t size);

	public:
		// I want lifetime of this object to be very limited
		// Ideally it should be limited to one block where it was created
		// So no move constructors are allowed
		AudioBuffer(const AudioBuffer& other) = delete;
		AudioBuffer(AudioBuffer&& other) noexcept = delete;
		AudioBuffer& operator=(const AudioBuffer& other) = delete;
		AudioBuffer& operator=(AudioBuffer&& other) noexcept = delete;

		~AudioBuffer();

		bool isSilent() const {
			return silent;
		}

		array_view<std::byte> getBuffer() const {
			return buffer;
		}
	};
}
