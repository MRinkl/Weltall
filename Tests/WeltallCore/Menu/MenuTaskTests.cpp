#include "Menu/MenuTask.h"

#include <catch_amalgamated.hpp>

#include <coroutine>
#include <string>
#include <vector>

using namespace Weltall;

namespace
{
	/** An owner of steps as the menu is: its steps wait for frames, which the test runs one at a time. */
	struct FStepOwner
	{
		FMenuTaskArena Arena{4096};
		std::coroutine_handle<> Waiting;
		std::vector<std::string> Log;

		FMenuTaskArena& TaskArena() { return Arena; }

		auto Frame()
		{
			struct FAwaiter
			{
				FStepOwner& Owner;

				bool await_ready() const noexcept { return false; }
				void await_suspend(std::coroutine_handle<> Step) noexcept { Owner.Waiting = Step; }
				void await_resume() const noexcept {}
			};
			return FAwaiter{*this};
		}

		TMenuTask<int> Count(int Frames)
		{
			for (int Frame = 0; Frame < Frames; ++Frame)
			{
				Log.push_back("count " + std::to_string(Frame));
				co_await this->Frame();
			}
			co_return Frames * 10;
		}

		TMenuTask<> Outer()
		{
			Log.push_back("outer");
			const int First = co_await Count(2);
			const int Second = co_await Count(1);
			Log.push_back("sum " + std::to_string(First + Second));
		}
	};
}

TEST_CASE("The menu's steps run nested, wait a frame at a time and give their memory back as they end", "[Menu]")
{
	FStepOwner Owner;
	{
		TMenuTask<> Root = Owner.Outer();
		CHECK(Root);
		CHECK_FALSE(Root.IsDone());
		// Nothing runs before the start; the start runs to the first wait.
		CHECK(Owner.Log.empty());
		Root.Start();
		CHECK(Owner.Log == std::vector<std::string>{"outer", "count 0"});
		const std::size_t Nested = Owner.Arena.Used();
		CHECK(Nested > 0);
		Owner.Waiting.resume();
		CHECK(Owner.Log.back() == "count 1");
		Owner.Waiting.resume();
		// The first count ended with its value; the second began in the same frame.
		CHECK(Owner.Log.back() == "count 0");
		CHECK(Owner.Arena.Used() == Nested);
		CHECK_FALSE(Root.IsDone());
		Owner.Waiting.resume();
		CHECK(Owner.Log.back() == "sum 30");
		CHECK(Root.IsDone());
	}
	CHECK(Owner.Arena.Used() == 0);
}

TEST_CASE("A menu step ended before it finishes gives its memory and its nested steps' back", "[Menu]")
{
	FStepOwner Owner;
	{
		TMenuTask<> Root = Owner.Outer();
		Root.Start();
		CHECK(Owner.Arena.Used() > 0);
	}
	CHECK(Owner.Arena.Used() == 0);
	TMenuTask<> Empty;
	CHECK_FALSE(Empty);
	CHECK_FALSE(Empty.IsDone());
}
