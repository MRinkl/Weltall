#include "Input/ControlBindings.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("Every PlayStation button but the stick buttons has its action", "[Input]")
{
	int32 Actions = 0;
	for (int32 Bit = 0; Bit < 16; ++Bit)
	{
		const EPadButton Button = static_cast<EPadButton>(Bit);
		const std::optional<EControlAction> Action = ActionOf(Button);
		CHECK(Action.has_value() == (Button != EPadButton::L3 && Button != EPadButton::R3));
		if (Action)
		{
			CHECK(PadButtonOf(*Action) == Button);
			++Actions;
		}
	}
	CHECK(Actions == 14);
	CHECK_FALSE(PadButtonOf(EControlAction::ViewTurn).has_value());
	CHECK_FALSE(PadButtonOf(EControlAction::ViewReset).has_value());
}

TEST_CASE("A key's family and direction come from its name", "[Input]")
{
	CHECK(FamilyOfKey("Gamepad_FaceButton_Bottom") == EInputFamily::Gamepad);
	CHECK(FamilyOfKey("-Gamepad_RightY") == EInputFamily::Gamepad);
	CHECK(FamilyOfKey("K") == EInputFamily::KeyboardAndMouse);
	CHECK(FamilyOfKey("-MouseY") == EInputFamily::KeyboardAndMouse);
	CHECK(IsNegatedBinding("-MouseY"));
	CHECK_FALSE(IsNegatedBinding("MouseY"));
	CHECK(BoundKeyName("-MouseY") == "MouseY");
}

TEST_CASE("The default bindings put a key on one action and every button on both families", "[Input]")
{
	const FControlBindings Bindings = FControlBindings::Defaults();
	// The tilt is a setting: nothing tilts the view unless the player binds it.
	CHECK(Bindings.Keys(EControlAction::ViewTilt).empty());
	for (int32 Action = 0; Action < ControlActionCount; ++Action)
	{
		const EControlAction Each = static_cast<EControlAction>(Action);
		if (Each == EControlAction::ViewTilt)
		{
			continue;
		}
		CHECK_FALSE(Bindings.FirstKey(Each, EInputFamily::Gamepad).empty());
		CHECK_FALSE(Bindings.FirstKey(Each, EInputFamily::KeyboardAndMouse).empty());
		for (const std::string& Key : Bindings.Keys(Each))
		{
			for (int32 Other = Action + 1; Other < ControlActionCount; ++Other)
			{
				for (const std::string& OtherKey : Bindings.Keys(static_cast<EControlAction>(Other)))
				{
					CHECK(BoundKeyName(Key) != BoundKeyName(OtherKey));
				}
			}
		}
	}
	CHECK(Bindings.FirstKey(EControlAction::Cross, EInputFamily::Gamepad) == "Gamepad_FaceButton_Bottom");
	CHECK(Bindings.FirstKey(EControlAction::Cross, EInputFamily::KeyboardAndMouse) == "E");
	CHECK(Bindings.FirstKey(EControlAction::Circle, EInputFamily::KeyboardAndMouse) == "Q");
	CHECK(Bindings.FirstKey(EControlAction::Square, EInputFamily::KeyboardAndMouse) == "F");
	CHECK(Bindings.FirstKey(EControlAction::Triangle, EInputFamily::KeyboardAndMouse) == "SpaceBar");
	CHECK(Bindings.FirstKey(EControlAction::L1, EInputFamily::KeyboardAndMouse) == "Tab");
	CHECK(Bindings.FirstKey(EControlAction::R1, EInputFamily::KeyboardAndMouse) == "R");
	CHECK(Bindings.FirstKey(EControlAction::L2, EInputFamily::KeyboardAndMouse) == "One");
	CHECK(Bindings.FirstKey(EControlAction::R2, EInputFamily::KeyboardAndMouse) == "Four");
}

TEST_CASE("Binding a key replaces its family's slot and takes it off other actions", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	Bindings.Bind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	CHECK(Bindings.Keys(EControlAction::Cross) == std::vector<std::string>{"E", "Enter", "Gamepad_FaceButton_Right"});
	CHECK(Bindings.Keys(EControlAction::Circle) == std::vector<std::string>{"Q", "Escape"});
	CHECK(Bindings.FirstKey(EControlAction::Circle, EInputFamily::Gamepad).empty());

	Bindings.Bind(EControlAction::Up, 1, "SpaceBar");
	CHECK(Bindings.Keys(EControlAction::Up) == std::vector<std::string>{"W", "SpaceBar", "Gamepad_DPad_Up", "Gamepad_LeftStick_Up"});
	CHECK(Bindings.Keys(EControlAction::Triangle) == std::vector<std::string>{"Gamepad_FaceButton_Top"});

	Bindings.Bind(EControlAction::Start, 5, "O");
	CHECK(Bindings.Keys(EControlAction::Start) == std::vector<std::string>{"P", "Gamepad_Special_Right", "O"});

	// An axis bound the other way round is the same key.
	Bindings.Bind(EControlAction::ViewTurn, 0, "-MouseY");
	CHECK(Bindings.Keys(EControlAction::ViewTurn) == std::vector<std::string>{"-MouseY", "Gamepad_RightX"});
	CHECK(Bindings.Keys(EControlAction::ViewTilt).empty());

	Bindings.Unbind("Gamepad_RightX");
	CHECK(Bindings.Keys(EControlAction::ViewTurn) == std::vector<std::string>{"-MouseY"});
	Bindings.Bind(EControlAction::ViewZoom, 0, "");
	CHECK(Bindings.Keys(EControlAction::ViewZoom) == FControlBindings::Defaults().Keys(EControlAction::ViewZoom));
}

TEST_CASE("Rebinding a key gives the action it leaves the key it replaced, or takes it off", "[Input]")
{
	using Keys = std::vector<std::string>;
	FControlBindings Bindings = FControlBindings::Defaults();
	// Q, Circle's, in Cross's first slot: Circle takes E in its place.
	Bindings.Rebind(EControlAction::Cross, 0, "Q");
	CHECK(Bindings.Keys(EControlAction::Cross) == Keys{"Q", "Enter", "Gamepad_FaceButton_Bottom"});
	CHECK(Bindings.Keys(EControlAction::Circle) == Keys{"E", "Escape", "Gamepad_FaceButton_Right"});

	// Into a free slot: the key just leaves its action, from wherever it was among that action's keys.
	Bindings.Rebind(EControlAction::Cross, 1, "Gamepad_DPad_Up");
	CHECK(Bindings.Keys(EControlAction::Cross) == Keys{"Q", "Enter", "Gamepad_FaceButton_Bottom", "Gamepad_DPad_Up"});
	CHECK(Bindings.Keys(EControlAction::Up) == Keys{"W", "Up", "Gamepad_LeftStick_Up"});
	CHECK(Bindings.KeyAt(EControlAction::Up, EInputFamily::Gamepad, 0) == "Gamepad_LeftStick_Up");

	// An action's own key in its other slot: the two change places, the sign the new binding's.
	Bindings.Rebind(EControlAction::Up, 0, "Up");
	CHECK(Bindings.Keys(EControlAction::Up) == Keys{"Up", "W", "Gamepad_LeftStick_Up"});
	Bindings.Rebind(EControlAction::ViewTilt, 0, "MouseY");
	CHECK(Bindings.Keys(EControlAction::ViewTilt) == Keys{"MouseY"});
	// Its own key asked for a free slot after it: it stays where it is.
	Bindings.Rebind(EControlAction::ViewTilt, 1, "-MouseY");
	CHECK(Bindings.Keys(EControlAction::ViewTilt) == Keys{"-MouseY"});

	// A free key replaces the slot's, which is then nobody's.
	Bindings.Rebind(EControlAction::Start, 0, "O");
	CHECK(Bindings.Keys(EControlAction::Start) == Keys{"O", "Gamepad_Special_Right"});
	CHECK_FALSE(Bindings.Find("P").has_value());
	Bindings.Rebind(EControlAction::Start, 0, "");
	CHECK(Bindings.Keys(EControlAction::Start) == Keys{"O", "Gamepad_Special_Right"});
}

TEST_CASE("A binding's slots are counted within its family, and a family is put back alone", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::KeyboardAndMouse, 1) == "Down");
	CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::Gamepad, 1) == "Gamepad_LeftStick_Down");
	CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::Gamepad, 2).empty());
	CHECK(Bindings.Count(EControlAction::Square, EInputFamily::Gamepad) == 1);
	CHECK(Bindings.Count(EControlAction::ViewTilt, EInputFamily::Gamepad) == 0);
	CHECK(Bindings.Find("Gamepad_LeftStick_Down") == FBoundPlace{EControlAction::Down, 1});
	CHECK(Bindings.Find("-Gamepad_RightY") == FBoundPlace{EControlAction::ViewZoom, 0});
	CHECK_FALSE(Bindings.Find("F12").has_value());

	Bindings.Clear(EControlAction::Down, EInputFamily::KeyboardAndMouse, 0);
	Bindings.Clear(EControlAction::Down, EInputFamily::KeyboardAndMouse, 5);
	CHECK(Bindings.Keys(EControlAction::Down) == std::vector<std::string>{"Down", "Gamepad_DPad_Down", "Gamepad_LeftStick_Down"});
	Bindings.Rebind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	Bindings.Rebind(EControlAction::Select, 1, "Insert");

	const auto AsDefaults = [&Bindings](EInputFamily Family)
	{
		const FControlBindings Defaults = FControlBindings::Defaults();
		for (int32 Action = 0; Action < ControlActionCount; ++Action)
		{
			const EControlAction Each = static_cast<EControlAction>(Action);
			for (int32 Slot = 0; Slot < 3; ++Slot)
			{
				if (Bindings.KeyAt(Each, Family, Slot) != Defaults.KeyAt(Each, Family, Slot))
				{
					return false;
				}
			}
		}
		return true;
	};

	// The controller's buttons back as they start; the keyboard's changes stay.
	Bindings.ResetFamily(EInputFamily::Gamepad);
	CHECK(AsDefaults(EInputFamily::Gamepad));
	CHECK(Bindings.KeyAt(EControlAction::Down, EInputFamily::KeyboardAndMouse, 0) == "Down");
	CHECK(Bindings.KeyAt(EControlAction::Select, EInputFamily::KeyboardAndMouse, 1) == "Insert");
	Bindings.ResetFamily(EInputFamily::KeyboardAndMouse);
	CHECK(AsDefaults(EInputFamily::KeyboardAndMouse));
	CHECK(AsDefaults(EInputFamily::Gamepad));
}
