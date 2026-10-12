#include "Field/FieldActor.h"

namespace Weltall
{
	std::vector<FFieldActor> CreateFieldActors(std::span<const FFieldActorDefinition> Definitions)
	{
		std::vector<FFieldActor> Actors(Definitions.size());
		for (std::size_t Index = 0; Index < Definitions.size(); ++Index)
		{
			const FFieldActorDefinition& Definition = Definitions[Index];
			FFieldActor& Actor = Actors[Index];
			Actor.Flags = Definition.Flags;
			Actor.Rotation = Definition.Rotation;
			Actor.Model = Definition.Model;
			for (std::size_t Axis = 0; Axis < 3; ++Axis)
			{
				Actor.Translation[Axis] = static_cast<uint16>(Definition.Position[Axis]);
				Actor.Position[Axis] = Actor.Translation[Axis];
			}
			if (!Definition.HasModel())
			{
				Actor.Flags |= FieldActorFlag::Hidden;
				Actor.Rotation = {};
			}
		}
		return Actors;
	}

	void SetFieldModelDrawModes(std::span<FFieldActor> Actors, bool bFog)
	{
		for (FFieldActor& Actor : Actors)
		{
			if ((Actor.Flags & FieldActorFlag::NoModel) == 0)
			{
				Actor.DrawMode = static_cast<uint8>(FieldModelDrawMode(Actor.Flags, bFog));
			}
		}
	}
}
