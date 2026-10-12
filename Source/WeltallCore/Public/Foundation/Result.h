#pragma once

#include <string>
#include <utility>
#include <variant>

namespace Weltall
{
	/** Why an operation failed, in words meant for a log or an error screen. */
	struct FError
	{
		std::string Message;
	};

	/**
	 * The value of an operation that can fail, or the reason it failed.
	 * Shaped like std::expected so it can be swapped for it once every toolchain provides C++23.
	 */
	template <typename T>
	class [[nodiscard]] TResult
	{
	public:
		TResult(T InValue) : Storage(std::in_place_index<0>, std::move(InValue)) {}
		TResult(FError InError) : Storage(std::in_place_index<1>, std::move(InError)) {}

		bool HasValue() const { return Storage.index() == 0; }
		explicit operator bool() const { return HasValue(); }

		T& Value() & { return std::get<0>(Storage); }
		const T& Value() const& { return std::get<0>(Storage); }
		T&& Value() && { return std::get<0>(std::move(Storage)); }

		const FError& Error() const { return std::get<1>(Storage); }

		T* operator->() { return &Value(); }
		const T* operator->() const { return &Value(); }
		T& operator*() & { return Value(); }
		const T& operator*() const& { return Value(); }

	private:
		std::variant<T, FError> Storage;
	};

	inline FError MakeError(std::string Message)
	{
		return FError{std::move(Message)};
	}
}
