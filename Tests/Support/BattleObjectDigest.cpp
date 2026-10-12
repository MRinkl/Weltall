#include "Support/BattleObjectDigest.h"

#include "Battle/Show/BattleShowCamera.h"
#include "Battle/Show/BattleShowObjects.h"
#include "Game/PsxRandom.h"
#include "Support/BattleObjectServices.h"

#include <algorithm>
#include <array>
#include <string>
#include <vector>

namespace Weltall::Testing
{
	namespace
	{
		constexpr int32 NoPack = 0xFE;
		constexpr int32 StreamKinds = 3;

		/** The digest's values under the oracle's labels (its dump's, which name what each group holds). */
		class FSink
		{
		public:
			explicit FSink(FOracleDigest& InDigest) : Digest(InDigest) {}

			void Put(const std::string&, const std::vector<int32>& Values)
			{
				for (const int32 Value : Values)
				{
					Digest.I32(Value);
				}
			}

		private:
			FOracleDigest& Digest;
		};

		void Add(std::vector<int32>& Values, const std::array<int16, 3>& Triple)
		{
			Values.insert(Values.end(), Triple.begin(), Triple.end());
		}

		void AddPlace(std::vector<int32>& Values, const FGearCodeRef& Ref)
		{
			const std::array<int32, 2> At = PackPlace(Ref);
			Values.insert(Values.end(), At.begin(), At.end());
		}

		void AddPlace(std::vector<int32>& Values, const FGearMotionPack* Pack, uint32 Offset)
		{
			Values.push_back(Pack != nullptr ? Pack->Slot : NoPack);
			Values.push_back(Pack != nullptr ? static_cast<int32>(Offset) : 0);
		}

		void AddMatrix(std::vector<int32>& Values, const FGteMatrix& Matrix)
		{
			for (const std::array<int16, 3>& Row : Matrix.M)
			{
				Add(Values, Row);
			}
			Values.insert(Values.end(), Matrix.T.begin(), Matrix.T.end());
		}

		void PutTrails(FSink& Sink, const FBattleObject& Object)
		{
			for (std::size_t Index = 0; Index < Object.Trails.size(); ++Index)
			{
				const FBattleTrailEmitter& Emitter = Object.Trails[Index];
				std::vector<int32> Values = {Emitter.Bone, Emitter.Kind, Emitter.Blend, Emitter.Segment};
				Add(Values, Emitter.Points[0]);
				Add(Values, Emitter.Points[1]);
				Values.insert(Values.end(), Emitter.Samples.begin(), Emitter.Samples.end());
				Values.insert(Values.end(), {Emitter.Ring, Emitter.Counter, Emitter.SegmentSamples, Emitter.Life});
				Add(Values, Emitter.Colour);
				Add(Values, Emitter.Fade);
				Sink.Put("trail " + std::to_string(Index), Values);
			}
		}

		void PutBuffer(FSink& Sink, const std::vector<uint16>& Colours)
		{
			std::vector<int32> Values = {static_cast<int32>(Colours.size())};
			Values.insert(Values.end(), Colours.begin(), Colours.end());
			Sink.Put("clut buffer", Values);
		}

		void PutClutAnimations(FSink& Sink, const FBattleObject& Object)
		{
			for (std::size_t Index = 0; Index < Object.ClutAnimations.size(); ++Index)
			{
				const FBattleClutAnimation& Animation = Object.ClutAnimations[Index];
				const std::string Label = "clut " + std::to_string(Index);
				if (!Animation.bActive)
				{
					Sink.Put(Label, {0});
					continue;
				}
				Sink.Put(Label, {1, Animation.Type, Animation.bFed ? 1 : 0, Animation.Feeds, static_cast<uint16>(Animation.Count), Animation.Phase, Animation.Speed,
									Animation.Level, 1, Animation.Divisor, Animation.Base, static_cast<int32>(Animation.Curve), Animation.bColourMatrix ? 1 : 0,
									Animation.Rect.X, Animation.Rect.Y, Animation.Rect.Width, Animation.Rect.Height});
				PutBuffer(Sink, Animation.Source);
				PutBuffer(Sink, Animation.Second);
				PutBuffer(Sink, Animation.Result);
			}
		}

		void PutCloths(FSink& Sink, const FBattleObject& Object)
		{
			for (std::size_t Index = 0; Index < Object.Cloths.size(); ++Index)
			{
				const FGearCloth& Cloth = Object.Cloths[Index];
				std::vector<int32> Values = {static_cast<int32>(Cloth.Points.size())};
				for (const FGearClothPoint& Point : Cloth.Points)
				{
					Values.insert(Values.end(), {Point.Position[0], Point.Position[1], Point.Position[2], Point.Normal[0], Point.Normal[1], Point.Normal[2]});
				}
				Sink.Put("cloth " + std::to_string(Index), Values);
			}
		}

		void PutBones(FSink& Sink, const FBattleObject& Object)
		{
			for (std::size_t Index = 0; Index < Object.Bones.size(); ++Index)
			{
				const FGearBone& Bone = Object.Bones[Index];
				std::vector<int32> Values = {Bone.Parent, Bone.bWorldDirty, Bone.bRotationDirty, Bone.RotationOrder, Bone.bShown, Bone.Part, Bone.Index};
				AddMatrix(Values, Bone.Local);
				AddMatrix(Values, Bone.World);
				Add(Values, Bone.Scale);
				Values.push_back(Bone.Billboard);
				Add(Values, Bone.Angles);
				Values.insert(Values.end(), Bone.Position.begin(), Bone.Position.end());
				Values.insert(Values.end(), {Bone.Rotation, Bone.Translation, Bone.Scaling});
				Sink.Put("bone " + std::to_string(Index), Values);
			}
		}

		void PutObject(FSink& Sink, int32 Slot, const FBattleObject& Object)
		{
			std::vector<int32> Values = {1, Object.Owner, Object.bFromFiles ? 1 : 0, Object.bExtensionPending ? 1 : 0, Object.Scale, Object.FloorTriangle,
				Object.Height, Object.RadiusX, Object.RadiusZ, Object.Stance, Object.Queue};
			Values.insert(Values.end(), Object.QueuedOwners.begin(), Object.QueuedOwners.end());
			Values.insert(Values.end(), Object.QueuedSequences.begin(), Object.QueuedSequences.end());
			Values.insert(Values.end(), {Object.bVisible, Object.Signal, Object.NoFloorSnap, Object.ScaledBones, Object.Playing, Object.Loudness, Object.Mark,
											Object.WatchTag, Object.WindY, Object.WaitTicks, Object.Loops, Object.Timer, Object.TimerEnd, Object.NearDistance,
											Object.Flags});
			Sink.Put("slot " + std::to_string(Slot), Values);
			Values.clear();
			AddPlace(Values, Object.Pc);
			AddPlace(Values, Object.OnNear);
			AddPlace(Values, Object.OnTimer);
			AddPlace(Values, Object.OnLanding);
			AddPlace(Values, Object.Motion, Object.Motion != nullptr ? Object.Motion->Scripts + 8 : 0);
			AddPlace(Values, Object.bHasSoundTable ? Object.Motion : nullptr, Object.Motion != nullptr ? Object.Motion->Bank : 0);
			AddPlace(Values, Object.Extension != nullptr ? &Object.Extension->Pack : nullptr, 0);
			Values.insert(Values.end(), {Object.bExtensionInstalled ? 1 : 0, Object.bOwnsMotion ? 1 : 0});
			Sink.Put("code", Values);
			Values = {Object.Who, Object.WhoBone, Object.Host, Object.HostTurns, Object.HostBone, Object.FloorY, Object.bSoundEffectsRegistered ? 1 : 0,
				Object.bExtensionSoundEffectsRegistered ? 1 : 0};
			for (const std::array<int16, 3>* Vector : {&Object.WhoOffset, &Object.AttachOffset, &Object.Spin, &Object.SpinAcceleration, &Object.Velocity,
					 &Object.Acceleration, &Object.Target})
			{
				Add(Values, *Vector);
			}
			Values.insert(Values.end(), {Object.Speed, Object.TextureX, Object.TextureY, Object.ClutX, Object.ClutY});
			Sink.Put("motion", Values);
			Values = {Object.KeyFrame, Object.KeyLoop, Object.KeyIndex, Object.KeyCount};
			AddPlace(Values, Object.KeyEvents);
			AddPlace(Values, Object.KeyEventsLoop);
			Values.insert(Values.end(), {Object.Targets, static_cast<int32>(Object.Trails.size()), static_cast<int32>(Object.Cloths.size()),
											static_cast<int32>(Object.ClutAnimations.size())});
			Sink.Put("keys", Values);
			PutTrails(Sink, Object);
			PutClutAnimations(Sink, Object);
			PutCloths(Sink, Object);
			PutBones(Sink, Object);
		}

		void PutTracks(FSink& Sink, const FBattleObjects& Objects)
		{
			const FGearChannelPool& Pool = Objects.Channels;
			Sink.Put("pool", {Pool.LowestFree(), Pool.Capacity()});
			const std::array<int16, FBattleModelCamera::TrackCount>& CameraTracks = Objects.ModelCamera.Tracks;
			for (int32 Index = 0; Index < Pool.Capacity(); ++Index)
			{
				const FGearChannel& Channel = Pool[Index];
				const std::string Label = "track " + std::to_string(Index);
				if (!Channel.bUsed)
				{
					Sink.Put(Label, {0});
					continue;
				}
				std::vector<int32> Values = {1, Channel.Loop, Channel.Kind, Channel.Tag};
				const bool bCamera = std::find(CameraTracks.begin(), CameraTracks.end(), Index) != CameraTracks.end();
				if ((Channel.Kind & GearChannelKind::Mask) < StreamKinds && !bCamera)
				{
					AddPlace(Values, Channel.StreamStart);
					AddPlace(Values, Channel.StreamRead);
				}
				else
				{
					Values.insert(Values.end(), Channel.Words.begin(), Channel.Words.end());
				}
				Values.insert(Values.end(), {Channel.Tick, Channel.Length});
				Sink.Put(Label, Values);
			}
		}

		void PutCamera(FSink& Sink, const FBattleObjects& Objects, const FBattleCamera& Camera)
		{
			FBattleModelCamera Model = Objects.ModelCamera;
			std::vector<int32> Values = {Model.bOn ? 1 : 0, Model.Watched, Model.Events, Model.bFollow ? 1 : 0};
			// The words as they lie in memory: the eye's yaw, pitch, distance and height, then the look-at's yaw, distance and height.
			for (const int32 Channel : {0, 2, 3, 6, 1, 4, 5})
			{
				Values.push_back(Model.Channel(Channel));
			}
			Values.insert(Values.end(), Model.Tracks.begin(), Model.Tracks.end());
			Add(Values, Camera.Eye);
			Add(Values, Camera.Target);
			Values.insert(Values.end(), {Camera.Mode, Model.FloorTriangle, Model.FloorHeight, Model.FloorFrame});
			Sink.Put("camera", Values);
		}

		void PutTrailPool(FSink& Sink, const FBattleObjects& Objects)
		{
			Sink.Put("trail pool", {Objects.TrailLowest, static_cast<int32>(Objects.TrailSegments.size()) - 1});
			for (std::size_t Index = 0; Index < Objects.TrailSegments.size(); ++Index)
			{
				const FBattleTrailSegment& Segment = Objects.TrailSegments[Index];
				std::vector<int32> Values = {Segment.Age, Segment.Kind};
				for (const FGteVector& Corner : Segment.Corners)
				{
					Add(Values, Corner);
				}
				Values.push_back(Segment.Life);
				Add(Values, Segment.Colour);
				Add(Values, Segment.Fade);
				Values.push_back(Segment.bSemiTransparent ? 1 : 0);
				Sink.Put("segment " + std::to_string(Index), Values);
			}
		}

		void PutEvents(FSink& Sink, FRecordingObjectServices& Services)
		{
			for (const FRecordingObjectServices::FEvent& Event : Services.Events)
			{
				std::vector<int32> Values = {static_cast<int32>(Event.Values.size() + 1)};
				for (std::size_t Letter = 0; Letter < 4; ++Letter)
				{
					Values.push_back(static_cast<uint8>(Event.Name[Letter]));
				}
				Values.insert(Values.end(), Event.Values.begin(), Event.Values.end());
				Sink.Put("event " + Event.Name, Values);
			}
			Services.Events.clear();
		}
	}

	void DigestBattleObjects(FOracleDigest& Digest, const FBattleObjects& Objects, const FBattleCamera& Camera, const FBattleObjectWords& Words)
	{
		FSink Sink(Digest);
		for (int32 Slot = 0; Slot < FBattleObjects::SlotCount; ++Slot)
		{
			if (const FBattleObject* Object = Objects.Object(Slot))
			{
				PutObject(Sink, Slot, *Object);
			}
			else
			{
				Sink.Put("slot " + std::to_string(Slot), {0});
			}
		}
		PutTracks(Sink, Objects);
		Sink.Put("module", {Objects.Accumulator, Objects.WindPhase, Objects.ClothWind, Objects.PulsePhase, Objects.PulseLevel, Objects.bObjectsVisible ? 1 : 0,
							   Objects.bShadows ? 1 : 0, Objects.bSignalled ? 1 : 0, Objects.Performer, Objects.PlayTargets});
		PutCamera(Sink, Objects, Camera);
		for (std::size_t Index = 0; Index < Objects.PointLights.size(); ++Index)
		{
			const FBattlePointLight& Light = Objects.PointLights[Index];
			// The light's record keeps a word after its offset that nothing writes.
			Sink.Put("light " + std::to_string(Index), {Light.Position[0], Light.Position[1], Light.Position[2], Light.Strength, Light.Offset[0], Light.Offset[1],
														   Light.Offset[2], 0, Light.Slot, Light.Bone});
		}
		std::vector<int32> Matrices;
		Add(Matrices, Objects.LightMatrix.M[1]);
		Add(Matrices, Objects.LightMatrix.M[2]);
		for (const std::array<int16, 3>& Row : Objects.ColourMatrix.M)
		{
			Add(Matrices, Row);
		}
		Sink.Put("matrices", Matrices);
		Sink.Put("words", {Words.RequestedEffect, static_cast<int32>(Words.Seed), static_cast<int32>(Words.TasksMade), static_cast<int32>(Words.TasksCounted),
							  Words.Record});
		PutTrailPool(Sink, Objects);
	}

	void DigestBattleObjectsFrame(FOracleDigest& Digest, const FBattleObjects& Objects, const FBattleCamera& Camera, FRecordingObjectServices& Services,
		const FPsxRandom& Random)
	{
		FBattleObjectWords Words;
		Words.RequestedEffect = Objects.RequestedEffect;
		Words.Seed = Random.Seed();
		Words.TasksMade = Services.TasksMade;
		Words.TasksCounted = Services.TasksCounted;
		Words.Record = Services.Record;
		DigestBattleObjects(Digest, Objects, Camera, Words);
		FSink Sink(Digest);
		PutEvents(Sink, Services);
	}
}
