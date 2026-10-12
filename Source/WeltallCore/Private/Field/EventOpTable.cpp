#include "EventOps.h"

namespace Weltall::EventOps
{
	namespace
	{
		void Unimplemented(FEventContext& Context)
		{
			Fail(Context, EEventFault::UnimplementedOpcode, CodeByte(Context, 0));
		}

		void UnimplementedExtended(FEventContext& Context)
		{
			Fail(Context, EEventFault::UnimplementedOpcode, 0xFE00u | CodeByte(Context, 0));
		}

		/**
		 * FE xx: the pc moves onto xx, and xx selects the handler. A handler that leaves the pc there
		 * lets xx run next as a base opcode.
		 */
		void RunExtended(FEventContext& Context)
		{
			Advance(Context, 1);
			const uint8 Opcode = CodeByte(Context, 0);
			const FExtendedTable& Handlers = ExtendedTable();
			if (Opcode >= Handlers.size())
			{
				UnimplementedExtended(Context);
				return;
			}
			Handlers[Opcode](Context);
		}

		struct FTables
		{
			FBaseTable Base;
			FExtendedTable Extended;

			FTables()
			{
				Base.fill(&Unimplemented);
				Extended.fill(&UnimplementedExtended);
				Base[0xFE] = &RunExtended;
				RegisterFlow(Base, Extended);
				RegisterThreads(Base, Extended);
				RegisterVariables(Base, Extended);
				RegisterMath(Base, Extended);
				RegisterActorFlags(Base, Extended);
				RegisterActors(Base, Extended);
				RegisterFacing(Base, Extended);
				RegisterItems(Base, Extended);
				RegisterCharacters(Base, Extended);
				RegisterPlacement(Base, Extended);
				RegisterSprites(Base, Extended);
				RegisterPartySprites(Base, Extended);
				RegisterParty(Base, Extended);
				RegisterCamera(Base, Extended);
				RegisterCameraControl(Base, Extended);
				RegisterCameraOrbits(Base, Extended);
				RegisterActorControl(Base, Extended);
				RegisterFieldState(Base, Extended);
				RegisterScreen(Base, Extended);
				RegisterModels(Base, Extended);
				RegisterCameraPath(Base, Extended);
				RegisterPlayerControl(Base, Extended);
				RegisterMessages(Base, Extended);
				RegisterWalks(Base, Extended);
				RegisterGlides(Base, Extended);
				RegisterScriptMoves(Base, Extended);
				RegisterMapChanges(Base, Extended);
				RegisterSoundEffects(Base, Extended);
				RegisterSoundBanks(Base, Extended);
				RegisterParticles(Base, Extended);
				RegisterMusicControls(Base, Extended);
				RegisterDoors(Base, Extended);
				RegisterScrolls(Base, Extended);
				RegisterPartyWalks(Base, Extended);
				RegisterJumps(Base, Extended);
				RegisterGears(Base, Extended);
				RegisterImages(Base, Extended);
				RegisterMarkers(Base, Extended);
				RegisterPrograms(Base, Extended);
				RegisterMenus(Base, Extended);
				RegisterMovies(Base, Extended);
			}
		};

		const FTables& Tables()
		{
			static const FTables Instance;
			return Instance;
		}
	}

	const FBaseTable& BaseTable()
	{
		return Tables().Base;
	}

	const FExtendedTable& ExtendedTable()
	{
		return Tables().Extended;
	}
}
