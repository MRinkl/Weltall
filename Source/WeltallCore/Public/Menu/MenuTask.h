#pragma once

#include <concepts>
#include <coroutine>
#include <cstddef>
#include <exception>
#include <optional>
#include <type_traits>
#include <utility>
#include <vector>

namespace Weltall
{
	/**
	 * Memory for the menu's running steps (its coroutine frames). A step started inside another ends
	 * before it, so the blocks come back in the order opposite to their taking, and the memory is one
	 * stack reserved once: starting and ending a step allocates nothing.
	 */
	class FMenuTaskArena
	{
	public:
		explicit FMenuTaskArena(std::size_t Bytes) : Memory(Bytes) {}

		/** A block of Size bytes on top of the stack, or null when the stack is full. */
		void* Take(std::size_t Size)
		{
			const std::size_t Rounded = RoundUp(Size);
			if (Top + Rounded > Memory.size())
			{
				return nullptr;
			}
			void* Block = Memory.data() + Top;
			Top += Rounded;
			return Block;
		}

		/** Gives back the block on top of the stack, of the size it was taken with. */
		void Give(std::size_t Size) { Top -= RoundUp(Size); }

		/** The bytes the steps running hold. */
		std::size_t Used() const { return Top; }

	private:
		/** As operator new aligns its blocks: coroutine frames may count on it. */
		static constexpr std::size_t Alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;

		static constexpr std::size_t RoundUp(std::size_t Size) { return (Size + Alignment - 1) / Alignment * Alignment; }

		std::vector<std::byte> Memory;
		std::size_t Top = 0;
	};

	/** What owns the menu's steps: every TMenuTask is a member function of one, and runs in its arena. */
	template <typename TOwner>
	concept CMenuTaskOwner = requires(TOwner& Owner) {
		{ Owner.TaskArena() } -> std::same_as<FMenuTaskArena&>;
	};

	namespace MenuTaskMemory
	{
		/**
		 * Each block starts with the arena it came from, so it can be given back without its owner; the
		 * header takes a whole alignment unit, so the frame after it keeps the block's alignment.
		 */
		constexpr std::size_t HeaderSize = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
		static_assert(HeaderSize >= sizeof(FMenuTaskArena*));

		inline void* Take(FMenuTaskArena& Arena, std::size_t Size)
		{
			std::byte* Block = static_cast<std::byte*>(Arena.Take(HeaderSize + Size));
			if (Block == nullptr)
			{
				// The steps nest no deeper than the menus' screens: an arena too small is a programming error.
				std::terminate();
			}
			*reinterpret_cast<FMenuTaskArena**>(Block) = &Arena;
			return Block + HeaderSize;
		}

		inline void Give(void* Frame, std::size_t Size)
		{
			std::byte* Block = static_cast<std::byte*>(Frame) - HeaderSize;
			(*reinterpret_cast<FMenuTaskArena**>(Block))->Give(HeaderSize + Size);
		}
	}

	/**
	 * A step of the menu's code that may run over several frames: a coroutine that starts when it is
	 * awaited and gives its result, and control, back to its awaiter when it ends. The menu's code keeps
	 * the original's shape: every loop that waits for the next frame awaits one.
	 */
	template <typename T = void>
	class [[nodiscard]] TMenuTask
	{
		struct FPromiseBase
		{
			std::coroutine_handle<> Continuation = std::noop_coroutine();

			std::suspend_always initial_suspend() noexcept { return {}; }

			auto final_suspend() noexcept
			{
				struct FToContinuation
				{
					std::coroutine_handle<> Next;

					bool await_ready() noexcept { return false; }
					std::coroutine_handle<> await_suspend(std::coroutine_handle<>) noexcept { return Next; }
					void await_resume() noexcept {}
				};
				return FToContinuation{Continuation};
			}

			void unhandled_exception() noexcept { std::terminate(); }

			template <CMenuTaskOwner TOwner, typename... TArgs>
			static void* operator new(std::size_t Size, TOwner& Owner, TArgs&&...)
			{
				return MenuTaskMemory::Take(Owner.TaskArena(), Size);
			}

			static void operator delete(void* Frame, std::size_t Size) { MenuTaskMemory::Give(Frame, Size); }
		};

		struct FValuePromise : FPromiseBase
		{
			std::optional<std::conditional_t<std::is_void_v<T>, int, T>> Value;

			template <typename TResult>
			void return_value(TResult&& Result)
			{
				Value = std::forward<TResult>(Result);
			}
		};

		struct FVoidPromise : FPromiseBase
		{
			void return_void() {}
		};

	public:
		struct promise_type : std::conditional_t<std::is_void_v<T>, FVoidPromise, FValuePromise>
		{
			TMenuTask get_return_object() { return TMenuTask(std::coroutine_handle<promise_type>::from_promise(*this)); }
		};

		/** No step: one to be given a step's later. */
		TMenuTask() = default;
		TMenuTask(TMenuTask&& Other) noexcept : Handle(std::exchange(Other.Handle, nullptr)) {}
		TMenuTask& operator=(TMenuTask&& Other) noexcept
		{
			if (this != &Other)
			{
				Destroy();
				Handle = std::exchange(Other.Handle, nullptr);
			}
			return *this;
		}
		TMenuTask(const TMenuTask&) = delete;
		TMenuTask& operator=(const TMenuTask&) = delete;
		~TMenuTask() { Destroy(); }

		bool await_ready() const noexcept { return false; }

		std::coroutine_handle<> await_suspend(std::coroutine_handle<> Awaiter) noexcept
		{
			Handle.promise().Continuation = Awaiter;
			return Handle;
		}

		T await_resume()
		{
			if constexpr (!std::is_void_v<T>)
			{
				return std::move(*Handle.promise().Value);
			}
		}

		/** For the program running the outermost step: starts it; it comes back at its first wait. */
		void Start() { Handle.resume(); }

		bool IsDone() const { return Handle && Handle.done(); }

		/** Whether it holds a step. */
		explicit operator bool() const { return static_cast<bool>(Handle); }

	private:
		explicit TMenuTask(std::coroutine_handle<promise_type> InHandle) : Handle(InHandle) {}

		void Destroy()
		{
			if (Handle)
			{
				Handle.destroy();
				Handle = nullptr;
			}
		}

		std::coroutine_handle<promise_type> Handle{};
	};
}
