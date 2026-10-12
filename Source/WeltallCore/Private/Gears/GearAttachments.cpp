#include "GearMachine.h"

#include "Graphics/GteVectors.h"

#include <string>

namespace Weltall::GearMachine
{
	void FollowAttachment(FGearModels& Models, FGearModel& Model)
	{
		const FGearModel* Host = Models.Model(Model.HostSlot);
		if (Host == nullptr)
		{
			Model.HostSlot = FGearModel::NoHost;
			return;
		}
		if ((Model.Flags & GearModelFlags::KeepsShownWhenAttached) == 0)
		{
			Model.bShown = Host->bShown;
		}
		if (Model.bShown == 0)
		{
			return;
		}
		const int32 HostBone = static_cast<int16>(Model.HostBone);
		if (HostBone < 0 || static_cast<std::size_t>(HostBone) >= Host->Bones.size())
		{
			Models.Fail("a gear is attached to bone " + std::to_string(HostBone) + ", which its host does not have");
			return;
		}
		const FGearBone& HostRoot = Host->Bones[0];
		FGearBone& Root = Model.Bones[0];
		if (Model.HostTurns != 0)
		{
			const FGteMatrix Turn = HostBone == 0 ? HostRoot.World : MulMatrix(HostRoot.World, Host->Bones[static_cast<std::size_t>(HostBone)].World);
			Root.Local.M = MulMatrix(Turn, Root.Local).M;
			Root.World.M = MulMatrix(Turn, Root.World).M;
		}
		const FGteMatrix Place = HostBone == 0 ? HostRoot.Local : CompMatrix(HostRoot.Local, Host->Bones[static_cast<std::size_t>(HostBone)].World);
		const FGteLongVector Where = RotTrans(Place, {Model.AttachOffset[0], Model.AttachOffset[1], Model.AttachOffset[2]});
		Root.Local.T = Where;
		Root.Position = Where;
	}

	int16 ProjectOnAxis(FGearEngine& Models, const std::array<int32, 3>& Offset, const FGteLongVector& First, const FGteLongVector& Second, int32 Scale)
	{
		const FGteLongVector Axis = OuterProduct12(First, Second);
		const int32 Length = SquareRoot0(static_cast<int32>(static_cast<uint32>(WrapMul(Axis[0], Axis[0])) + static_cast<uint32>(WrapMul(Axis[1], Axis[1]))
			+ static_cast<uint32>(WrapMul(Axis[2], Axis[2]))));
		const int32 Dot = WrapAdd(WrapAdd(WrapMul(Axis[0], Offset[0]), WrapMul(Axis[1], Offset[1])), WrapMul(Axis[2], Offset[2]));
		const int32 Along = WrapMul(Divide(Models, WrapMul(Dot, 0x10), Length + 1), 0x100);
		return static_cast<int16>(Divide(Models, Along, Scale));
	}
}
