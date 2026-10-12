#include "Menu/MenuProgram.h"

#include "Game/GameState.h"
#include "Input/KernelPad.h"
#include "Menu/MenuGearShop.h"
#include "Menu/MenuMain.h"
#include "Menu/MenuMember.h"
#include "Menu/MenuName.h"
#include "Menu/MenuShop.h"
#include "Timing/PlayClock.h"

namespace Weltall
{
	namespace
	{
		std::unique_ptr<FMenuModule> MakeModule(const FMenuServices& Services, const FMenuRequest& Request)
		{
			switch (Request.Mode)
			{
			case EMenuMode::MemberChange:
				return std::make_unique<FMenuMember>(Services, Request);
			case EMenuMode::EnterName:
				return std::make_unique<FMenuName>(Services, Request);
			case EMenuMode::Shop:
				return std::make_unique<FMenuShop>(Services, Request);
			case EMenuMode::GearShop:
				return std::make_unique<FMenuGearShop>(Services, Request);
			default:
				return std::make_unique<FMenuMain>(Services, Request);
			}
		}
	}

	FMenuProgram::FMenuProgram(const FMenuServices& InServices, const FMenuRequest& Request)
		: Services(InServices), Module(MakeModule(InServices, Request))
	{
	}

	FMenuProgram::~FMenuProgram()
	{
		// The running steps' frames live in the module's arena: they go first.
		Root = {};
	}

	TResult<bool> FMenuProgram::Step(IVramUploads* Uploads)
	{
		if (bEnded)
		{
			return false;
		}
		if (Module->Failure())
		{
			return *Module->Failure();
		}
		Module->SetUploadObserver(Uploads);
		if (!Root)
		{
			Root = Module->Run();
			Root.Start();
		}
		else if (!Module->StepPadWait())
		{
			Module->WaitingStep().resume();
		}
		Module->SetUploadObserver(nullptr);
		if (Module->Failure())
		{
			return *Module->Failure();
		}
		if (Module->IsSoftReset() || Root.IsDone())
		{
			bEnded = true;
			return false;
		}
		return true;
	}

	TResult<bool> FMenuProgram::Frame(FPadState Held, IVramUploads* Uploads)
	{
		TResult<bool> Drawn = Step(Uploads);
		if (!Drawn || !*Drawn)
		{
			return Drawn;
		}
		// The vertical blank after the frame: the console counts it before it reads the pad, whose repeat keeps to the count.
		FGameState& Game = *Services.Game;
		Game.SetPlayTime(Game.PlayTime() + 1);
		Services.Pad->Vblank(Held, Game.PlayTime());
		Services.Clock->Vblank();
		return true;
	}

	bool FMenuProgram::EndedBySoftReset() const
	{
		return Module->IsSoftReset();
	}

	const FMenuFrame& FMenuProgram::Snapshot() const
	{
		return Module->DrawnFrame();
	}

	const FVram& FMenuProgram::Vram() const
	{
		return Module->Vram();
	}

	const FGearShopModel* FMenuProgram::GearModel() const
	{
		return Module->GearModel();
	}

	const FMenuResources& FMenuProgram::Resources() const
	{
		return *Services.Resources;
	}
}
