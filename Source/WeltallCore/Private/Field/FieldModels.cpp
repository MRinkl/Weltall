#include "Field/FieldModels.h"

#include "Graphics/GteVectors.h"
#include "Graphics/MdlMime.h"

#include <algorithm>
#include <span>
#include <string>

namespace Weltall
{
	namespace
	{
		/** Bits of the script state's Status: the axis a script turns the model about as it is drawn, 0 none. */
		constexpr uint32 ExtraRotationAxis = 0x3;
		constexpr uint16 NoGearJoint = 0xFFFF;
		constexpr uint8 NoParent = 0xFF;
		/** The drift's modes: the scenery that is not a script actor's, or every model not attached to another. */
		constexpr uint8 SceneryDrifts = 0;
		constexpr uint8 EveryModelDrifts = 1;

		FGteMatrix Placement(const std::array<std::array<int16, 3>, 3>& Rotation, const std::array<int32, 3>& Translation)
		{
			FGteMatrix Matrix;
			Matrix.M = Rotation;
			Matrix.T = Translation;
			return Matrix;
		}

		int32 Wrapped(int32 A, int32 B)
		{
			return static_cast<int32>(static_cast<uint32>(A) + static_cast<uint32>(B));
		}

		FError Unsupported(std::size_t Actor, const char* What)
		{
			return MakeError("field actor " + std::to_string(Actor) + ": " + What);
		}

		/** The part an actor's model is drawn from; null for an actor without a model. */
		const FMdlPart* ModelPart(const FEventContext& Context, const FFieldActor& Record)
		{
			if ((Record.Flags & FieldActorFlag::NoModel) != 0 || Record.Model >= Context.MapModels.size() || Context.MapModels[Record.Model].Parts.empty())
			{
				return nullptr;
			}
			return &Context.MapModels[Record.Model].Parts.front();
		}

		/** The shared vertices a MIMe moves for an actor's model; null when they never move. */
		std::vector<FGteVector>* MovingVertices(FEventContext& Context, const FFieldActor& Record)
		{
			if (ModelPart(Context, Record) == nullptr || Record.Model >= Context.MimeVertices.size() || Context.MimeVertices[Record.Model].empty())
			{
				return nullptr;
			}
			return &Context.MimeVertices[Record.Model];
		}

		/** A shown model's MIMe update, then its vertices as this actor draws them. */
		void AnimateModel(FEventContext& Context, std::size_t Index)
		{
			FFieldActor& Record = Context.Actors[Index];
			std::vector<FGteVector>* Vertices = MovingVertices(Context, Record);
			if (Vertices == nullptr || (Record.Flags & FieldActorFlag::Hidden) != 0)
			{
				return;
			}
			const FMdlPart& Part = *ModelPart(Context, Record);
			if ((Record.Flags & FieldActorFlag::VertexAnimation) != 0 && Part.Mime)
			{
				std::span<const int32> Weights;
				if (Index < Context.States.size())
				{
					Weights = std::span<const int32>(Context.States[Index].PartValues).first(Part.Mime->Keys.size());
				}
				AnimateMdlMime(Part, Weights, *Vertices);
			}
			std::copy(Vertices->begin(), Vertices->end(), Record.DrawnVertices.begin());
		}
	}

	std::optional<FError> SetUpFieldMime(FEventContext& Context)
	{
		Context.MimeVertices.assign(Context.MapModels.size(), {});
		for (std::size_t Index = 0; Index < Context.Actors.size(); ++Index)
		{
			const FFieldActor& Record = Context.Actors[Index];
			const FMdlPart* Part = ModelPart(Context, Record);
			if (Part == nullptr || (Record.Flags & FieldActorFlag::VertexAnimation) == 0 || !Part->Mime)
			{
				continue;
			}
			if (Part->bMimeMovesNormals)
			{
				return Unsupported(Index, "its model's MIMe animation moves its normals, which no map's does");
			}
			if (Index < Context.States.size() && Part->Mime->Keys.size() > static_cast<std::size_t>(FEventActorState::PartValueCount))
			{
				return Unsupported(Index, "its model's MIMe animation has more keys than the actor has part values");
			}
			Context.MimeVertices[Record.Model] = Part->Vertices;
		}
		for (FFieldActor& Record : Context.Actors)
		{
			const std::vector<FGteVector>* Vertices = MovingVertices(Context, Record);
			Record.DrawnVertices = Vertices != nullptr ? *Vertices : std::vector<FGteVector>{};
		}
		return std::nullopt;
	}

	std::optional<FError> UpdateFieldModels(FEventContext& Context)
	{
		FFieldRenderState& Render = Context.Render;
		for (std::size_t Axis = 0; Axis < 3; ++Axis)
		{
			Render.DriftTotal[Axis] = Wrapped(Render.DriftTotal[Axis], Render.DriftStep[Axis]);
		}
		const uint8 DriftMode = Render.SceneryDrift & FieldSceneryDrift::ModeMask;
		for (std::size_t Index = 0; Index < Context.Actors.size(); ++Index)
		{
			FFieldActor& Record = Context.Actors[Index];
			Record.WorldMatrix = Record.Matrix;
			Record.Position = Record.Translation;
			Record.bCarried = false;
			if ((Record.Flags & FieldActorFlag::NoModel) != 0)
			{
				continue;
			}
			bool bDrifts = DriftMode == SceneryDrifts || DriftMode == EveryModelDrifts;
			if (Index < Context.States.size())
			{
				const FEventActorState& State = Context.States[Index];
				const bool bTurnedAlone = (State.Status & ExtraRotationAxis) != 0;
				const bool bOnGear = !bTurnedAlone && State.GearJoint != NoGearJoint;
				if (bOnGear)
				{
					// Riding a gear's joint: the joint's matrix times the model's own, or the model's own twice where the slot is empty.
					if (!Context.Gears.Models || (State.GearJoint >> 12) >= FGearModels::Slots)
					{
						return Unsupported(Index, "a model rides the joint of a gear slot the module does not have, or on a map whose load loaded no gears");
					}
					const FGteMatrix Own = Placement(Record.Matrix, Record.Translation);
					const std::optional<FGteMatrix> Joint = FieldGearJoint(Context, State.GearJoint >> 12, State.GearJoint & 0xFFF);
					if (Context.Gears.Models->Fault)
					{
						return Context.Gears.Models->Fault;
					}
					Record.Carrier = Joint ? *Joint : Own;
					Record.bCarried = true;
					const FGteMatrix World = CompMatrix(Record.Carrier, Own);
					Record.WorldMatrix = World.M;
					Record.Position = World.T;
				}
				else if (!bTurnedAlone && State.Parent != NoParent)
				{
					if (State.Parent >= Context.Actors.size())
					{
						return Unsupported(Index, "a model is attached to an actor the map does not have");
					}
					const FFieldActor& Parent = Context.Actors[State.Parent];
					Record.Carrier = Placement(Parent.WorldMatrix, Parent.Position);
					Record.bCarried = true;
					const FGteMatrix World = CompMatrix(Record.Carrier, Placement(Record.Matrix, Record.Translation));
					Record.WorldMatrix = World.M;
					Record.Position = World.T;
				}
				bDrifts = !bTurnedAlone && !bOnGear && State.Parent == NoParent && DriftMode == EveryModelDrifts;
			}
			if (bDrifts)
			{
				for (std::size_t Axis = 0; Axis < 3; ++Axis)
				{
					Record.Translation[Axis] = Wrapped(Record.Translation[Axis], Render.DriftStep[Axis]);
				}
			}
			AnimateModel(Context, Index);
		}
		return std::nullopt;
	}
}
