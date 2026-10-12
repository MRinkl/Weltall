#include "Field/FieldLight.h"

#include "Field/EventContext.h"
#include "Field/FieldSceneFrame.h"
#include "Gears/GearModels.h"

namespace Weltall
{
	namespace
	{
		/** Bits of an actor's state: the axis a script turns its model about as it is drawn. */
		constexpr uint32 ExtraRotationAxis = 0x3;
		constexpr uint8 RelitMode = 1;
		/** The lights are turned by a model's matrix at half its scale. */
		constexpr int32 LightScale = 0x800;

		FGteMatrix Placement(const std::array<std::array<int16, 3>, 3>& Rotation, const std::array<int32, 3>& Translation)
		{
			FGteMatrix Matrix;
			Matrix.M = Rotation;
			Matrix.T = Translation;
			return Matrix;
		}
	}

	void PlaceFieldSceneObject(const FEventContext& Context, FFieldSceneObject& Object, bool bDriftDrawsAll)
	{
		const FFieldActor& Record = Context.Actors[Object.Actor];
		Object.MatrixMode = (Record.Flags & FieldActorFlag::MatrixMode) == 1 ? EFieldMatrixMode::FacingScreen : EFieldMatrixMode::Placed;
		Object.bWithoutWorldRotation = false;
		Object.bNeverCulled = bDriftDrawsAll;
		Object.Placement = Placement(Record.Matrix, Record.Translation);
		if (Object.Actor >= Context.States.size())
		{
			return;
		}
		const uint32 Axis = Context.States[Object.Actor].Status & ExtraRotationAxis;
		if (Axis != 0)
		{
			// A model a script turns as it is drawn: about x, y or z by the state's extra angle.
			FGteVector Angles{};
			Angles[Axis == 1 ? 0 : Axis == 2 ? 1 : 2] = Context.States[Object.Actor].ExtraAngle;
			Object.Placement.M = MulMatrix(Placement(Record.Matrix, {}), RotationXyz(Angles)).M;
			Object.bNeverCulled = false;
		}
		else if (Record.bCarried)
		{
			// Under its parent as the drawing last placed that one, or under its gear's joint, beneath the view but not the world's turn.
			Object.Attached = Object.Placement;
			Object.Placement = Record.Carrier;
			Object.bWithoutWorldRotation = true;
			Object.bNeverCulled = false;
		}
	}

	std::optional<FGteMatrix> FieldModelsLight(const FEventContext& Context, const FMdlLights& Lights)
	{
		// The drawing loads the lights for a relit model before it looks whether the model is hidden or on screen.
		for (std::size_t Actor = Context.Actors.size(); Actor-- > 0;)
		{
			const FFieldActor& Record = Context.Actors[Actor];
			if ((Record.Flags & FieldActorFlag::NoModel) != 0 || Record.DrawMode != RelitMode)
			{
				continue;
			}
			FFieldSceneObject Object;
			Object.Actor = static_cast<uint16>(Actor);
			PlaceFieldSceneObject(Context, Object, false);
			return MulMatrix(Lights.Directions, ScaleColumns(FieldObjectMatrix(Object, Context.Camera.View), {LightScale, LightScale, LightScale}));
		}
		return std::nullopt;
	}

	std::optional<FGteMatrix> FieldGearsLight(const FGearModels& Models, const FFieldGearLights& Lights)
	{
		FGteMatrix Light;
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Light.M[Row][Column] = Lights.Words[3 * Row + Column];
			}
		}
		const FGearFrame& Frame = Models.Frame();
		for (std::size_t Slot = Frame.Models.size(); Slot-- > 0;)
		{
			const FGearFrameModel& Drawn = Frame.Models[Slot];
			if (!Drawn.bPresent || !Drawn.bShown)
			{
				continue;
			}
			for (std::size_t Part = Drawn.Parts.size(); Part-- > 0;)
			{
				if (Drawn.Parts[Part].bShown)
				{
					return MulMatrix(MulMatrix(Light, Drawn.RootWorld), Drawn.Parts[Part].World);
				}
			}
		}
		return std::nullopt;
	}

	void LoadFieldGearColors(const FFieldGearLights& Lights, FGte& Gte)
	{
		for (std::size_t Row = 0; Row < 3; ++Row)
		{
			for (std::size_t Column = 0; Column < 3; ++Column)
			{
				Gte.ColorMatrix.M[Row][Column] = Lights.Words[FFieldGearLights::MatrixWords + 3 * Row + Column];
			}
		}
		for (std::size_t Channel = 0; Channel < 3; ++Channel)
		{
			Gte.BackColor[Channel] = Lights.Ambient[Channel] * 16;
		}
	}
}
