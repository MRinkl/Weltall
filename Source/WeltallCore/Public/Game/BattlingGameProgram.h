#pragma once

#include "Battling/BattlingData.h"
#include "Battling/BattlingDrawing.h"
#include "Foundation/Result.h"
#include "Game/GameProgram.h"
#include "Graphics/Vram.h"
#include "Text/MesWindow.h"

#include <array>
#include <memory>
#include <optional>
#include <span>

namespace Weltall
{
	class FBattlingFight;
	class FBattlingGameSound;
	struct FBattlingTickInput;
	class FGameSession;
	class FGameState;

	/** What the Battling program shows. */
	enum class EBattlingGameStage : uint8
	{
		/** The arena's menus: the top menu, bonus battling's, the gear choice, over the fire. */
		Menus,
		/** The two gears' portraits over the fire, while the console would load their files: until a button is pressed, two seconds at most. */
		Versus,
		/** The bout: the fight, its replay and screens, the tutorial. */
		Bout,
	};

	/**
	 * The arena's settings a bout starts from when the field asks for the arena's menus or the scripted bout: read from the
	 * game state's word, which is written with the defaults when it was never set; a state that has won the last gear
	 * forgets the gears beaten.
	 */
	struct FBattlingSavedSettings
	{
		std::array<bool, 2> Vibration{};
		uint8 WinsPerSet = 2;
		uint8 Level = 0;

		/** The settings a word holds, the defaults when it was never set. */
		WELTALLCORE_API static FBattlingSavedSettings FromWord(uint32 Word);
		WELTALLCORE_API static FBattlingSavedSettings Load(FGameState& Game);
		/** Writes them into the game state's word as leaving the menus does, its upper half (Argento's unlock) kept. */
		WELTALLCORE_API void Save(FGameState& Game) const;
	};

	/**
	 * The Battling program as the kernel runs it: the bout the field asked for with its six bytes (a bout against the
	 * computer, practice, the tutorial, or the story's scripted bout), or the arena's menus and the sets they start, run pass
	 * by pass with the session's pad, random numbers and sound, the field's song playing on, until it leaves; then the
	 * result the field reads, the gears beaten (and the menus' settings) kept in the game state, and the field next.
	 */
	class FBattlingGameProgram final : public IGameProgram
	{
	public:
		/** The program started as the kernel runs it, up to its first wait. The session must outlive it. */
		WELTALLCORE_API static TResult<std::unique_ptr<FBattlingGameProgram>> Start(FGameSession& Session);
		WELTALLCORE_API ~FBattlingGameProgram() override;
		FBattlingGameProgram(const FBattlingGameProgram&) = delete;
		FBattlingGameProgram& operator=(const FBattlingGameProgram&) = delete;

		int32 VblanksPerFrame() const override { return NextBlanks; }
		[[nodiscard]] WELTALLCORE_API std::optional<FError> Frame(std::span<const FPadState> HeldPerVblank) override;
		EGameProgramState State() const override { return ProgramState; }

		EBattlingGameStage Stage() const { return GameStage; }
		/** Whether the second controller plays: a bout of two players or its gear choice, or practice against the second controller. */
		WELTALLCORE_API bool ReadsSecondPad() const;
		/** The bout, whose fire the versus screen shows too. */
		const FBattlingFight& Fight() const { return *Bout; }
		/** The two sides' gears, by the arena's numbers. */
		const std::array<uint8, 2>& Gears() const { return GearNumbers; }

		/** VRAM as the program's uploads leave it: the arena's pictures and both gears' textures. */
		const FVram& Vram() const { return Screen; }
		/** The last frame of the bout drawn, none before the first; and the one before it, when it was drawn the pass before. */
		const FBattlingFrame* LatestFrame() const { return Latest < 0 ? nullptr : &Frames[static_cast<std::size_t>(Latest)]; }
		const FBattlingFrame* PreviousFrame() const { return bPreviousFollows ? &Frames[static_cast<std::size_t>(1 - Latest)] : nullptr; }
		/**
		 * The picture a pause captured: the frame on the screen as it opened, which a paused frame shows blurred under its 2D
		 * layer (FBattlingFrame::PictureBlurs, counted from this picture's own).
		 */
		const FBattlingFrame* PausedPicture() const { return Picture ? &*Picture : nullptr; }

	private:
		explicit FBattlingGameProgram(FGameSession& InSession);

		/**
		 * The bout's gears (GearNumbers) read and loaded, as the program does once their files are in: their textures and HUD
		 * pictures uploaded and their parts built.
		 */
		[[nodiscard]] std::optional<FError> LoadGears();
		/** The first round begun once the gears are loaded: the fade in through the fire, the marks, the main loop's counts. */
		void BeginBout();
		/**
		 * A pass of the arena's menus, then what it chose: EXIT, a bout whose gears are loaded, or the demonstration, whose
		 * gears are loaded and whose first pass runs with the same pads.
		 */
		[[nodiscard]] std::optional<FError> RunMenus();
		[[nodiscard]] std::optional<FError> AfterMenusPass(const FBattlingTickInput& Input);
		/** A pass after the menus chose a bout: while its gears are read, then the steps of their load. */
		void RunMenusLoad();
		/**
		 * What a pass leaves: the windows' typing, the pictures it needs, its frame (bLoading: a step of the gears' load), the
		 * game state's bytes.
		 */
		void PublishPass(bool bLoading);
		/** The versus screen's frame of a pass: while the gears are read, or a step of their load. */
		void DrawVersus(bool bLoading);
		/** The vibrations the pass started, handed to the kernel's motors. */
		void StartVibrations();
		/** Battling's exit: the field next, with the bout's result. */
		void Leave();

		FGameSession& Session;
		FMesContext Text;
		std::array<uint8, 2> GearNumbers{};
		std::array<FBattlingGearFile, 2> GearFiles;
		std::unique_ptr<FBattlingGameSound> Sound;
		std::unique_ptr<FBattlingFight> Bout;
		FVram Screen;
		std::unique_ptr<FBattlingDrawing> Drawing;
		/** The lines the latest pass's message windows typed, uploaded into Screen. */
		std::vector<FVramImage> TypedLines;
		std::array<FBattlingFrame, 2> Frames;
		int32 Latest = -1;
		std::optional<FBattlingFrame> Picture;
		/** The blurs the captured picture already had: a pause opened while the resumed pass still showed the last one's. */
		int32 PictureBlursBefore = 0;
		bool bPreviousFollows = false;
		EBattlingGameStage GameStage = EBattlingGameStage::Bout;
		/** The versus screen: the passes it has been held for, whether its wait is over, and the load's passes left after it. */
		int32 HeldPasses = 0;
		bool bGearsRead = false;
		int32 LoadPasses = 0;
		bool bVersusShown = false;
		bool bBoutBegun = false;
		/** The field asked for the arena's menus: the program runs them, and each set they start ends back in them. */
		bool bFromMenus = false;
		/** Every gear's portrait, which the gear choice uploads as it first opens after the menus start. */
		std::vector<FVramImage> MenuPortraits;
		int32 NextBlanks = 2;
		EGameProgramState ProgramState = EGameProgramState::Running;
	};
}
