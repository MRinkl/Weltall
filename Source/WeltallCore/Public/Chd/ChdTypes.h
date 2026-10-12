#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <variant>

/**
 * A reader of MAME's compressed disc images (CHD), written against the C++ standard library alone so that the same
 * files build in any project. A project fits it to its own file access through IInput.
 */
namespace Chd
{
	using int8 = std::int8_t;
	using int16 = std::int16_t;
	using int32 = std::int32_t;
	using int64 = std::int64_t;
	using uint8 = std::uint8_t;
	using uint16 = std::uint16_t;
	using uint32 = std::uint32_t;
	using uint64 = std::uint64_t;

	/** Random-access, read-only bytes: the CHD file. Read may be called from several threads at once. */
	class IInput
	{
	public:
		virtual ~IInput() = default;

		virtual uint64 Size() const = 0;

		/** Fills Destination with the bytes at Offset; false if they are not all available. */
		virtual bool Read(uint64 Offset, std::span<uint8> Destination) const = 0;
	};

	/** Why a CHD cannot be read. */
	enum class EError : uint8
	{
		/** The file could not be read. */
		CannotRead,
		/** The file does not begin as a CHD does. */
		NotChd,
		/** A CHD of a version other than 5. */
		UnsupportedVersion,
		/** Packed by a codec this reader does not know. */
		UnknownCodec,
		/** Holds only what differs from another CHD, its parent. */
		NeedsParent,
		/** The file ends before parts its header or map place in it: still being copied, or cut short. */
		CutShort,
		/** Contradicts itself. */
		Corrupt,
	};

	/** The value of an operation that can fail, or the reason it failed. */
	template <typename T>
	class [[nodiscard]] TResult
	{
	public:
		TResult(T InValue) : Storage(std::in_place_index<0>, std::move(InValue)) {}
		TResult(EError InError) : Storage(std::in_place_index<1>, InError) {}

		bool HasValue() const { return Storage.index() == 0; }
		explicit operator bool() const { return HasValue(); }

		T& Value() & { return std::get<0>(Storage); }
		const T& Value() const& { return std::get<0>(Storage); }
		T&& Value() && { return std::get<0>(std::move(Storage)); }

		EError Error() const { return std::get<1>(Storage); }

		T* operator->() { return &Value(); }
		const T* operator->() const { return &Value(); }
		T& operator*() & { return Value(); }
		const T& operator*() const& { return Value(); }

	private:
		std::variant<T, EError> Storage;
	};
}
