#include "Input/ButtonGlyphs.h"

#include <catch_amalgamated.hpp>

using namespace Weltall;

TEST_CASE("A device's icon set follows its name", "[Input]")
{
	CHECK(IconSetOfDevice(false, "KBM") == EButtonIconSet::Keyboard);
	CHECK(IconSetOfDevice(true, "DualSense Wireless Controller") == EButtonIconSet::PlayStation);
	CHECK(IconSetOfDevice(true, "PS4 Controller") == EButtonIconSet::PlayStation);
	CHECK(IconSetOfDevice(true, "Nintendo Switch Pro Controller") == EButtonIconSet::Nintendo);
	CHECK(IconSetOfDevice(true, "Steam Deck") == EButtonIconSet::SteamDeck);
	CHECK(IconSetOfDevice(true, "XboxOne") == EButtonIconSet::Xbox);
	CHECK(IconSetOfDevice(true, "Hid") == EButtonIconSet::Xbox);
}

TEST_CASE("A PlayStation button's glyph is its binding on the device the player uses", "[Input]")
{
	FControlBindings Bindings = FControlBindings::Defaults();
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::Xbox) == FButtonGlyph{EButtonIconSet::Xbox, "Gamepad_FaceButton_Bottom", "A"});
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::PlayStation).Icon == "Cross");
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::Nintendo).Icon == "B");
	CHECK(ButtonGlyph(EPadButton::Triangle, Bindings, EButtonIconSet::SteamDeck).Icon == "Y");
	CHECK(ButtonGlyph(EPadButton::Start, Bindings, EButtonIconSet::PlayStation).Icon == "Options");
	CHECK(ButtonGlyph(EPadButton::Up, Bindings, EButtonIconSet::Xbox).Icon == "DPad_Up");
	CHECK(ButtonGlyph(EPadButton::L2, Bindings, EButtonIconSet::Keyboard) == FButtonGlyph{EButtonIconSet::Keyboard, "One", "1"});
	CHECK(ButtonGlyph(EPadButton::Select, Bindings, EButtonIconSet::Keyboard).Icon == "Backspace");

	Bindings.Bind(EControlAction::Cross, 0, "Gamepad_FaceButton_Right");
	CHECK(ButtonGlyph(EPadButton::Cross, Bindings, EButtonIconSet::PlayStation).Icon == "Circle");
	const FButtonGlyph Unbound = ButtonGlyph(EPadButton::Circle, Bindings, EButtonIconSet::PlayStation);
	CHECK(Unbound.Key.empty());
	CHECK(Unbound.Icon.empty());
	CHECK(ButtonGlyph(EPadButton::L3, Bindings, EButtonIconSet::Xbox).Key.empty());
}

TEST_CASE("A face button's place among the four is the one its controller prints the icon at", "[Input]")
{
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "Cross") == EFaceButtonPlace::Bottom);
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "Circle") == EFaceButtonPlace::Right);
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "Square") == EFaceButtonPlace::Left);
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "Triangle") == EFaceButtonPlace::Top);
	CHECK(FaceButtonPlace(EButtonIconSet::Xbox, "A") == EFaceButtonPlace::Bottom);
	CHECK(FaceButtonPlace(EButtonIconSet::Xbox, "Y") == EFaceButtonPlace::Top);
	// Nintendo's letters sit across from Xbox's.
	CHECK(FaceButtonPlace(EButtonIconSet::Nintendo, "A") == EFaceButtonPlace::Right);
	CHECK(FaceButtonPlace(EButtonIconSet::Nintendo, "Y") == EFaceButtonPlace::Left);
	CHECK(FaceButtonPlace(EButtonIconSet::SteamDeck, "X") == EFaceButtonPlace::Left);
	CHECK(FaceButtonPlace(EButtonIconSet::Xbox, "LB") == EFaceButtonPlace::None);
	CHECK(FaceButtonPlace(EButtonIconSet::PlayStation, "A") == EFaceButtonPlace::None);
	CHECK(FaceButtonPlace(EButtonIconSet::Keyboard, "A") == EFaceButtonPlace::None);
}
