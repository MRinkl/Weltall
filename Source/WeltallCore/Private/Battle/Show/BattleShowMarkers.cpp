#include "Battle/Show/BattleShowMarkers.h"

#include "Foundation/LittleEndian.h"
#include "Graphics/Gte.h"
#include "Media/PsxExecutable.h"

#include <algorithm>

namespace Weltall
{
	namespace
	{
		/** Where the kernel keeps the pyramid model the markers draw (both discs of the USA release). */
		constexpr uint32 MarkerModelAddress = 0x8001C76C;
		constexpr uint32 ModelHeaderSize = 0xC;
		constexpr uint32 ObjectSize = 0x1C;
		constexpr uint32 VertexSize = 8;
		/** A primitive's mode bits that make it textured, a quad or gouraud; its flag's bit that leaves it unlit. */
		constexpr uint8 ShapeBits = 0x1C;
		constexpr uint8 Gouraud = 0x10;
		constexpr uint8 Unlit = 1;
		/** How far over the combatant's head the pyramid stands, and how fast it turns. */
		constexpr int32 HeadRoom = 0x20;
		constexpr int16 TurnPerStep = 0x10;

		/** A pulse: the sprite's colour brightened and darkened by the sine of the step. */
		uint8 Pulse(int32 Wave)
		{
			return static_cast<uint8>(std::min(((Wave + 0x1000) >> 6) + 0x80, 0xFF));
		}
	}

	TResult<FBattleMarkerModel> FBattleMarkerModel::Read(const FPsxExecutable& Executable)
	{
		const auto Head = Executable.Read(MarkerModelAddress, ModelHeaderSize + ObjectSize);
		if (Head.empty())
		{
			return MakeError("the executable holds no marker model");
		}
		const auto Word = [&Head](uint32 At) { return LittleEndian::ReadU32(Head, At); };
		const uint32 VertexTop = Word(ModelHeaderSize);
		const uint32 VertexCount = Word(ModelHeaderSize + 4);
		const uint32 PrimitiveTop = Word(ModelHeaderSize + 0x10);
		const uint32 PrimitiveCount = Word(ModelHeaderSize + 0x14);
		const uint32 ObjectAddress = MarkerModelAddress + ModelHeaderSize;
		const auto Vertices = Executable.Read(ObjectAddress + VertexTop, VertexCount * VertexSize);
		if (Vertices.empty())
		{
			return MakeError("the marker model's vertices lie past the executable");
		}
		FBattleMarkerModel Model;
		for (uint32 Vertex = 0; Vertex < VertexCount; ++Vertex)
		{
			const std::size_t At = Vertex * VertexSize;
			Model.Vertices.push_back({LittleEndian::ReadS16(Vertices, At), LittleEndian::ReadS16(Vertices, At + 2), LittleEndian::ReadS16(Vertices, At + 4)});
		}
		uint32 Primitive = ObjectAddress + PrimitiveTop;
		for (uint32 Index = 0; Index < PrimitiveCount; ++Index)
		{
			const auto Header = Executable.Read(Primitive, 4);
			if (Header.empty())
			{
				return MakeError("the marker model's primitives lie past the executable");
			}
			const uint8 Words = Header[1];
			const uint8 Flag = Header[2];
			const uint8 Mode = Header[3];
			const uint8 Shape = Mode & ShapeBits;
			if ((Shape != 0 && Shape != Gouraud) || (Flag & Unlit) == 0)
			{
				return MakeError("the marker model holds a primitive other than an unlit triangle");
			}
			// A flat triangle's colour then its corners; a gouraud one's three colours then its corners.
			const uint32 Colors = Shape == Gouraud ? 3 : 1;
			const auto Body = Executable.Read(Primitive + 4, Colors * 4 + 8);
			if (Body.empty())
			{
				return MakeError("the marker model's primitives lie past the executable");
			}
			FBattleMarkerModel::FTriangle Triangle;
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				const std::size_t At = (Colors == 3 ? Corner : 0) * 4;
				Triangle.Colors[Corner] = {Body[At], Body[At + 1], Body[At + 2]};
			}
			Triangle.Code = Mode;
			for (std::size_t Corner = 0; Corner < 3; ++Corner)
			{
				Triangle.Vertices[Corner] = LittleEndian::ReadU16(Body, Colors * 4 + Corner * 2);
				if (Triangle.Vertices[Corner] >= VertexCount)
				{
					return MakeError("a marker model's triangle names a vertex past its vertices");
				}
			}
			Model.Triangles.push_back(Triangle);
			Primitive += (Words + 1u) * 4u;
		}
		return Model;
	}

	void FBattleMarker::Update()
	{
		// The marker's step: over the combatant's head, its colour pulsing, turning.
		Position = {Target->Position[0] >> 16, (Target->Position[1] >> 16) - Height - HeadRoom, Target->Position[2] >> 16};
		++Count;
		const uint8 Level = Pulse(GteSin(Count << 6));
		Target->Red = Level;
		Target->Green = Level;
		Target->Blue = Level;
		Owner->Sprites->ApplyColorBlend(*Target);
		Rotation[1] = static_cast<int16>(Rotation[1] + TurnPerStep);
	}

	void FBattleMarker::OnKilled()
	{
		Owner->End(*this);
	}

	void FBattleActorGlow::Update()
	{
		const int32 Angle = Count << 6;
		Sprite->Red = Pulse(GteSin(Angle));
		Sprite->Green = Pulse(GteCos(Angle));
		Sprite->Blue = Sprite->Green;
		Owner->Sprites->ApplyColorBlend(*Sprite);
		++Count;
	}

	void FBattleActorGlow::OnKilled()
	{
		Owner->StopGlow();
	}

	void FBattleMarkers::Reset()
	{
		for (FBattleMarker& Marker : Markers)
		{
			Marker = FBattleMarker{};
			Marker.Owner = this;
		}
		Glow = FBattleActorGlow{};
		Glow.Owner = this;
		Mask = 0;
		Made = 0;
	}

	void FBattleMarkers::Set(uint16 Slots, uint8 Actor, const std::array<FSprite*, SlotCount>& SlotSprites)
	{
		Mask = Slots;
		if (Slots == 0)
		{
			StopGlow();
		}
		else
		{
			StartGlow(Actor, SlotSprites);
		}
		for (int32 Slot = 0; Slot < SlotCount; ++Slot)
		{
			FSprite* Sprite = SlotSprites[static_cast<std::size_t>(Slot)];
			FBattleMarker& Marker = Markers[static_cast<std::size_t>(Slot)];
			if ((Slots >> Slot & 1) == 0)
			{
				if (Sprite != nullptr && Marker.bLive)
				{
					Sprites->UnlinkTask(Marker);
					End(Marker);
				}
			}
			else if (Sprite != nullptr && !Marker.bLive)
			{
				Create(Slot, *Sprite);
			}
		}
	}

	const FBattleMarker* FBattleMarkers::Find(const IKernelTask* Task) const
	{
		const auto Found = std::find_if(Markers.begin(), Markers.end(), [Task](const FBattleMarker& Marker) { return &Marker == Task; });
		return Found != Markers.end() && Found->bLive ? &*Found : nullptr;
	}

	void FBattleMarkers::Create(int32 Slot, FSprite& Target)
	{
		// The marker is a task under the combatant's, not among those the playback counts.
		FBattleMarker& Marker = Markers[static_cast<std::size_t>(Slot)];
		Marker = FBattleMarker{};
		Marker.Owner = this;
		Marker.bLive = true;
		Marker.Serial = ++Made;
		Marker.Target = &Target;
		Marker.Height = static_cast<uint16>(Target.FrameTop);
		Sprites->LinkTask(Marker, &Target);
		Sprites->Uncount(Marker);
		Target.Code &= static_cast<uint8>(~1u);
		Sprites->ApplyColorBlend(Target);
		Marker.Update();
	}

	void FBattleMarkers::End(FBattleMarker& Marker)
	{
		// The marker's end: the combatant's colour no longer applied.
		Marker.bLive = false;
		if (Marker.Target != nullptr)
		{
			Marker.Target->Code |= 1;
			Sprites->ApplyColorBlend(*Marker.Target);
		}
	}

	void FBattleMarkers::StartGlow(uint8 Actor, const std::array<FSprite*, SlotCount>& SlotSprites)
	{
		if (Glow.bLive)
		{
			if (Glow.Actor == Actor)
			{
				return;
			}
			StopGlow();
		}
		FSprite* Sprite = Actor < SlotCount ? SlotSprites[Actor] : nullptr;
		if (Sprite == nullptr)
		{
			return;
		}
		Glow = FBattleActorGlow{};
		Glow.Owner = this;
		Glow.bLive = true;
		Glow.Sprite = Sprite;
		Glow.Actor = Actor;
		Sprites->LinkTask(Glow, Sprite);
		Sprites->Uncount(Glow);
		Sprite->Code &= static_cast<uint8>(~1u);
	}

	void FBattleMarkers::StopGlow()
	{
		// The glow's end: the sprite's colour no longer applied (from its next build).
		if (!Glow.bLive)
		{
			return;
		}
		Glow.bLive = false;
		Glow.Sprite->Code |= 1;
		if (Sprites->IsLinked(Glow))
		{
			Sprites->UnlinkTask(Glow);
		}
	}
}
