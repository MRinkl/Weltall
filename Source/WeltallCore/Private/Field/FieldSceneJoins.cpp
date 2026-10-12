#include "Field/FieldSceneJoins.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <map>
#include <set>
#include <utility>

namespace Weltall
{
	namespace
	{
		/** How far apart two edges may lie and still be joined, in the map's units: past a unit and its diagonal, short of two. */
		constexpr double Reach = 1.5;
		/** Two open edges run beside each other for this much at least, and for this share of the shorter one. */
		constexpr double LeastOverlap = 1.0;
		constexpr double OverlapShare = 1.0 / 3.0;
		/** Past this cosine between the ways into them, two polygons lie to the same side of their edges: one over the other. */
		constexpr double SameSide = 0.5;
		/** Vertices of different models this near stand in one place: where one is joined, the other follows. */
		constexpr double Coincident = 1.0 / 16.0;
		/** A join that moves its vertex less than this under one placement moves nothing. */
		constexpr double Unmoved = 1.0e-3;

		struct FPoint
		{
			double X = 0.0;
			double Y = 0.0;
			double Z = 0.0;

			FPoint operator+(const FPoint& Other) const { return {X + Other.X, Y + Other.Y, Z + Other.Z}; }
			FPoint operator-(const FPoint& Other) const { return {X - Other.X, Y - Other.Y, Z - Other.Z}; }
			FPoint operator*(double Scale) const { return {X * Scale, Y * Scale, Z * Scale}; }
			double Dot(const FPoint& Other) const { return X * Other.X + Y * Other.Y + Z * Other.Z; }
			double Length() const { return std::sqrt(Dot(*this)); }
		};

		/** A model taking part, by its distinct places: a model may keep one place under several vertex numbers. */
		struct FPiece
		{
			uint16 Object = 0;
			uint32 PolygonCount = 0;
			std::vector<uint16> PlaceOf;
			std::vector<FPoint> Local;
			std::vector<FPoint> World;
		};

		struct FOpenEdge
		{
			uint16 Piece = 0;
			uint16 A = 0;
			uint16 B = 0;
			FPoint Start;
			FPoint End;
			/** The unit vector from Start to End, and the one across the edge into its polygon. */
			FPoint Along;
			FPoint Inward;
			double Length = 0.0;
			FPoint Low;
			FPoint High;
		};

		enum class ETarget : uint8
		{
			None,
			Vertex,
			Edge,
		};

		/** Where a place is drawn instead: at a place of another piece, or Share of the way along an edge between two. */
		struct FTarget
		{
			ETarget Kind = ETarget::None;
			uint16 Piece = 0;
			uint16 A = 0;
			uint16 B = 0;
			double Share = 0.0;
		};

		struct FCandidate
		{
			uint16 Place = 0;
			FTarget Target;
			double Distance = 0.0;
		};

		FPoint Placed(const FGteMatrix& Placement, const FPoint& Local)
		{
			const auto Row = [&Placement, &Local](std::size_t Index)
			{
				const std::array<int16, 3>& Entries = Placement.M[Index];
				return (Entries[0] * Local.X + Entries[1] * Local.Y + Entries[2] * Local.Z) / FGteMatrix::One + Placement.T[Index];
			};
			return {Row(0), Row(1), Row(2)};
		}

		std::vector<FPiece> MakePieces(const FFieldScene& Scene)
		{
			std::vector<FPiece> Pieces;
			for (std::size_t Number = 0; Number < Scene.Objects.size(); ++Number)
			{
				const FFieldSceneObject& Object = Scene.Objects[Number];
				const FMdlPart& Part = Scene.PartOf(Object);
				if (Object.MatrixMode != EFieldMatrixMode::Placed || Object.PolygonCount == 0 || Part.Mime)
				{
					continue;
				}
				FPiece& Piece = Pieces.emplace_back();
				Piece.Object = static_cast<uint16>(Number);
				Piece.PolygonCount = Object.PolygonCount;
				std::map<FGteVector, uint16> Places;
				for (const FGteVector& Vertex : Part.Vertices)
				{
					const auto [Found, bNew] = Places.try_emplace(Vertex, static_cast<uint16>(Piece.Local.size()));
					if (bNew)
					{
						Piece.Local.push_back({static_cast<double>(Vertex[0]), static_cast<double>(Vertex[1]), static_cast<double>(Vertex[2])});
						Piece.World.push_back(Placed(Object.Placement, Piece.Local.back()));
					}
					Piece.PlaceOf.push_back(Found->second);
				}
			}
			return Pieces;
		}

		/** The edges of a piece no other polygon of it shares, a polygon drawn twice over the same places counted once. */
		void AddOpenEdges(const FFieldScene& Scene, const std::vector<FPiece>& Pieces, uint16 PieceNumber, std::vector<FOpenEdge>& Edges)
		{
			struct FSide
			{
				uint32 Key = 0;
				uint16 A = 0;
				uint16 B = 0;
				FPoint Centre;
			};
			constexpr std::array<std::size_t, 4> AroundTriangle{0, 1, 2, 0};
			constexpr std::array<std::size_t, 4> AroundQuad{0, 1, 3, 2};
			const FPiece& Piece = Pieces[PieceNumber];
			const FFieldSceneObject& Object = Scene.Objects[Piece.Object];
			std::vector<FSide> Sides;
			std::set<std::array<uint16, 4>> Seen;
			for (uint32 Index = Object.FirstPolygon; Index < Object.FirstPolygon + Object.PolygonCount; ++Index)
			{
				const FMdlPrimitive& Primitive = Scene.PrimitiveOf(Scene.Polygons[Index]);
				const std::size_t Corners = static_cast<std::size_t>(Primitive.Traits().Corners);
				std::array<uint16, 4> Places{};
				FPoint Centre;
				for (std::size_t Corner = 0; Corner < Corners; ++Corner)
				{
					Places[Corner] = Piece.PlaceOf[Primitive.Vertices[Corner]];
					Centre = Centre + Piece.World[Places[Corner]] * (1.0 / static_cast<double>(Corners));
				}
				std::array<uint16, 4> Sorted = Places;
				if (Corners == 3)
				{
					Sorted[3] = 0xFFFF;
				}
				std::sort(Sorted.begin(), Sorted.end());
				if (!Seen.insert(Sorted).second)
				{
					continue;
				}
				const std::array<std::size_t, 4>& Around = Corners == 3 ? AroundTriangle : AroundQuad;
				for (std::size_t Side = 0; Side < Corners; ++Side)
				{
					const uint16 A = Places[Around[Side]];
					const uint16 B = Places[Around[(Side + 1) % Corners]];
					if (A != B)
					{
						Sides.push_back({static_cast<uint32>(std::min(A, B)) << 16 | std::max(A, B), A, B, Centre});
					}
				}
			}
			std::stable_sort(Sides.begin(), Sides.end(), [](const FSide& Left, const FSide& Right) { return Left.Key < Right.Key; });
			for (std::size_t Index = 0; Index < Sides.size(); ++Index)
			{
				const bool bShared = (Index > 0 && Sides[Index - 1].Key == Sides[Index].Key) || (Index + 1 < Sides.size() && Sides[Index + 1].Key == Sides[Index].Key);
				if (bShared)
				{
					continue;
				}
				const FSide& Side = Sides[Index];
				FOpenEdge Edge;
				Edge.Piece = PieceNumber;
				Edge.A = Side.A;
				Edge.B = Side.B;
				Edge.Start = Piece.World[Side.A];
				Edge.End = Piece.World[Side.B];
				Edge.Length = (Edge.End - Edge.Start).Length();
				if (Edge.Length <= 0.0)
				{
					continue;
				}
				Edge.Along = (Edge.End - Edge.Start) * (1.0 / Edge.Length);
				const FPoint ToCentre = Side.Centre - (Edge.Start + Edge.End) * 0.5;
				const FPoint Across = ToCentre - Edge.Along * ToCentre.Dot(Edge.Along);
				const double Reached = Across.Length();
				if (Reached <= 0.0)
				{
					continue;
				}
				Edge.Inward = Across * (1.0 / Reached);
				Edge.Low = {std::min(Edge.Start.X, Edge.End.X), std::min(Edge.Start.Y, Edge.End.Y), std::min(Edge.Start.Z, Edge.End.Z)};
				Edge.High = {std::max(Edge.Start.X, Edge.End.X), std::max(Edge.Start.Y, Edge.End.Y), std::max(Edge.Start.Z, Edge.End.Z)};
				Edges.push_back(Edge);
			}
		}

		/** Whether the first piece is the one fitted to the second: the one of fewer polygons, the later of two alike. */
		bool IsFittedTo(const FPiece& Fitted, const FPiece& Kept)
		{
			return Fitted.PolygonCount != Kept.PolygonCount ? Fitted.PolygonCount < Kept.PolygonCount : Fitted.Object > Kept.Object;
		}

		/** Whether two open edges run beside each other across a gap: near at both ends of where they overlap, their polygons not to one side. */
		bool RunBeside(const FOpenEdge& Fitted, const FOpenEdge& Kept)
		{
			if (Fitted.Inward.Dot(Kept.Inward) > SameSide)
			{
				return false;
			}
			const double From = (Kept.Start - Fitted.Start).Dot(Fitted.Along);
			const double To = (Kept.End - Fitted.Start).Dot(Fitted.Along);
			const double Low = std::max(0.0, std::min(From, To));
			const double High = std::min(Fitted.Length, std::max(From, To));
			if (High - Low < std::max(LeastOverlap, std::min(Fitted.Length, Kept.Length) * OverlapShare))
			{
				return false;
			}
			for (const double At : {Low, High})
			{
				const FPoint OnKept = Kept.Start + (Kept.End - Kept.Start) * ((At - From) / (To - From));
				const FPoint OnFitted = Fitted.Start + Fitted.Along * At;
				if ((OnKept - OnFitted).Length() > Reach)
				{
					return false;
				}
			}
			return true;
		}

		/** Where an end of a fitted edge may go on the edge it runs beside: its nearer end within reach, else the edge itself. */
		void AddCandidate(uint16 Place, const FPoint& At, const FOpenEdge& Kept, std::vector<FCandidate>& Candidates)
		{
			const double ToStart = (At - Kept.Start).Length();
			const double ToEnd = (At - Kept.End).Length();
			if (std::min(ToStart, ToEnd) <= Reach)
			{
				Candidates.push_back({Place, {ETarget::Vertex, Kept.Piece, ToStart <= ToEnd ? Kept.A : Kept.B, 0, 0.0}, std::min(ToStart, ToEnd)});
				return;
			}
			const double Share = (At - Kept.Start).Dot(Kept.Along) / Kept.Length;
			if (Share <= 0.0 || Share >= 1.0)
			{
				return;
			}
			const double Off = (At - (Kept.Start + (Kept.End - Kept.Start) * Share)).Length();
			if (Off <= Reach)
			{
				Candidates.push_back({Place, {ETarget::Edge, Kept.Piece, Kept.A, Kept.B, Share}, Off});
			}
		}

		void FindSeams(const std::vector<FPiece>& Pieces, std::vector<FOpenEdge>& Edges, std::vector<std::vector<FCandidate>>& Candidates)
		{
			std::sort(Edges.begin(), Edges.end(), [](const FOpenEdge& Left, const FOpenEdge& Right) { return Left.Low.X < Right.Low.X; });
			for (std::size_t First = 0; First < Edges.size(); ++First)
			{
				const FOpenEdge& One = Edges[First];
				for (std::size_t Second = First + 1; Second < Edges.size() && Edges[Second].Low.X <= One.High.X + Reach; ++Second)
				{
					const FOpenEdge& Other = Edges[Second];
					if (Other.Piece == One.Piece || Other.Low.Y > One.High.Y + Reach || One.Low.Y > Other.High.Y + Reach || Other.Low.Z > One.High.Z + Reach
						|| One.Low.Z > Other.High.Z + Reach)
					{
						continue;
					}
					const bool bOneFitted = IsFittedTo(Pieces[One.Piece], Pieces[Other.Piece]);
					const FOpenEdge& Fitted = bOneFitted ? One : Other;
					const FOpenEdge& Kept = bOneFitted ? Other : One;
					if (RunBeside(Fitted, Kept))
					{
						AddCandidate(Fitted.A, Fitted.Start, Kept, Candidates[Fitted.Piece]);
						AddCandidate(Fitted.B, Fitted.End, Kept, Candidates[Fitted.Piece]);
					}
				}
			}
		}

		/**
		 * Each place's target of those its seams offer: the ends before the edges and the nearest first, an end taken by
		 * one place of a piece only, so no polygon folds onto a point.
		 */
		std::vector<FTarget> Choose(const FPiece& Piece, const std::vector<FPiece>& Pieces, std::vector<FCandidate>& Candidates)
		{
			std::sort(Candidates.begin(), Candidates.end(), [&Pieces](const FCandidate& Left, const FCandidate& Right)
			{
				const auto Key = [&Pieces](const FCandidate& Candidate)
				{
					return std::tuple(Candidate.Target.Kind, Candidate.Distance, Candidate.Place, Pieces[Candidate.Target.Piece].Object, Candidate.Target.A,
						Candidate.Target.B);
				};
				return Key(Left) < Key(Right);
			});
			std::vector<FTarget> Chosen(Piece.Local.size());
			std::set<std::pair<uint16, uint16>> Taken;
			for (const FCandidate& Candidate : Candidates)
			{
				FTarget& Target = Chosen[Candidate.Place];
				if (Target.Kind != ETarget::None)
				{
					continue;
				}
				if (Candidate.Target.Kind == ETarget::Vertex && !Taken.insert({Candidate.Target.Piece, Candidate.Target.A}).second)
				{
					continue;
				}
				Target = Candidate.Target;
			}
			return Chosen;
		}

		/** Follows each place's target to the piece and point it ends at. */
		class FResolver
		{
		public:
			struct FEnd
			{
				bool bJoined = false;
				uint16 Piece = 0;
				FPoint Local;
			};

			FResolver(const FFieldScene& InScene, const std::vector<FPiece>& InPieces, const std::vector<std::vector<FTarget>>& InTargets)
				: Scene(InScene), Pieces(InPieces), Targets(InTargets), Ends(InPieces.size()), States(InPieces.size())
			{
				for (std::size_t Piece = 0; Piece < Pieces.size(); ++Piece)
				{
					Ends[Piece].resize(Pieces[Piece].Local.size());
					States[Piece].assign(Pieces[Piece].Local.size(), EState::New);
				}
			}

			FEnd EndOf(uint16 Piece, uint16 Place)
			{
				const FEnd Own{false, Piece, Pieces[Piece].Local[Place]};
				EState& State = States[Piece][Place];
				if (State == EState::Done)
				{
					return Ends[Piece][Place];
				}
				if (State == EState::Following)
				{
					// A ring of targets: it ends here.
					return Own;
				}
				State = EState::Following;
				const FTarget& Target = Targets[Piece][Place];
				FEnd End = Own;
				if (Target.Kind == ETarget::Vertex)
				{
					End = EndOf(Target.Piece, Target.A);
					End.bJoined = true;
				}
				else if (Target.Kind == ETarget::Edge)
				{
					const FEnd From = EndOf(Target.Piece, Target.A);
					const FEnd To = EndOf(Target.Piece, Target.B);
					// An edge whose ends are drawn by different pieces has no one space to lie in: the place keeps its own.
					if (From.Piece == To.Piece)
					{
						End = {true, From.Piece, From.Local + (To.Local - From.Local) * Target.Share};
					}
				}
				if (End.bJoined && IsNone(Piece, Own.Local, End))
				{
					End = Own;
				}
				Ends[Piece][Place] = End;
				States[Piece][Place] = EState::Done;
				return End;
			}

			/** Whether a join to End moves nothing for a place of Piece standing at Own: the same piece, or the same point under one placement. */
			bool IsNone(uint16 Piece, const FPoint& Own, const FEnd& End) const
			{
				return End.Piece == Piece
					|| (Scene.Objects[Pieces[End.Piece].Object].Placement == Scene.Objects[Pieces[Piece].Object].Placement && (End.Local - Own).Length() < Unmoved);
			}

		private:
			enum class EState : uint8
			{
				New,
				Following,
				Done,
			};

			const FFieldScene& Scene;
			const std::vector<FPiece>& Pieces;
			const std::vector<std::vector<FTarget>>& Targets;
			std::vector<std::vector<FEnd>> Ends;
			std::vector<std::vector<EState>> States;
		};

		/**
		 * A place that is not joined, standing where a joined place of another piece stands, is drawn where that one is:
		 * the two met before and still do. Not a place another's join leans on, which is where the two already meet.
		 */
		void FollowJoined(const std::vector<FPiece>& Pieces, const std::vector<std::vector<FTarget>>& Targets, const FResolver& Resolver,
			std::vector<std::vector<FResolver::FEnd>>& Ends)
		{
			struct FPlace
			{
				uint16 Piece = 0;
				uint16 Place = 0;
			};
			using FCell = std::array<int64, 3>;
			const auto CellOf = [](const FPoint& At) { return FCell{static_cast<int64>(std::floor(At.X)), static_cast<int64>(std::floor(At.Y)), static_cast<int64>(std::floor(At.Z))}; };
			std::map<FCell, std::vector<FPlace>> Joined;
			std::vector<std::vector<bool>> LeanedOn(Pieces.size());
			for (std::size_t Piece = 0; Piece < Pieces.size(); ++Piece)
			{
				LeanedOn[Piece].assign(Pieces[Piece].Local.size(), false);
			}
			for (std::size_t Piece = 0; Piece < Pieces.size(); ++Piece)
			{
				for (std::size_t Place = 0; Place < Ends[Piece].size(); ++Place)
				{
					if (Ends[Piece][Place].bJoined)
					{
						Joined[CellOf(Pieces[Piece].World[Place])].push_back({static_cast<uint16>(Piece), static_cast<uint16>(Place)});
					}
					const FTarget& Target = Targets[Piece][Place];
					if (Target.Kind != ETarget::None)
					{
						LeanedOn[Target.Piece][Target.A] = true;
						if (Target.Kind == ETarget::Edge)
						{
							LeanedOn[Target.Piece][Target.B] = true;
						}
					}
				}
			}
			if (Joined.empty())
			{
				return;
			}
			std::vector<std::pair<FPlace, FResolver::FEnd>> Followers;
			for (std::size_t Piece = 0; Piece < Pieces.size(); ++Piece)
			{
				for (std::size_t Place = 0; Place < Ends[Piece].size(); ++Place)
				{
					if (Ends[Piece][Place].bJoined || LeanedOn[Piece][Place])
					{
						continue;
					}
					const FPoint& At = Pieces[Piece].World[Place];
					const FCell Cell = CellOf(At);
					const FResolver::FEnd* Nearest = nullptr;
					double NearestDistance = Coincident;
					for (int64 X = -1; X <= 1; ++X)
					{
						for (int64 Y = -1; Y <= 1; ++Y)
						{
							for (int64 Z = -1; Z <= 1; ++Z)
							{
								const auto Found = Joined.find({Cell[0] + X, Cell[1] + Y, Cell[2] + Z});
								if (Found == Joined.end())
								{
									continue;
								}
								for (const FPlace& Other : Found->second)
								{
									const FResolver::FEnd& End = Ends[Other.Piece][Other.Place];
									const double Distance = (Pieces[Other.Piece].World[Other.Place] - At).Length();
									if (Other.Piece != Piece && Distance < NearestDistance && !Resolver.IsNone(static_cast<uint16>(Piece), Pieces[Piece].Local[Place], End))
									{
										Nearest = &End;
										NearestDistance = Distance;
									}
								}
							}
						}
					}
					if (Nearest != nullptr)
					{
						Followers.push_back({{static_cast<uint16>(Piece), static_cast<uint16>(Place)}, *Nearest});
					}
				}
			}
			for (const auto& [Follower, End] : Followers)
			{
				Ends[Follower.Piece][Follower.Place] = End;
			}
		}
	}

	std::vector<FFieldSceneJoin> BuildFieldSceneJoins(const FFieldScene& Scene)
	{
		const std::vector<FPiece> Pieces = MakePieces(Scene);
		std::vector<FFieldSceneJoin> Joins;
		if (Pieces.size() < 2)
		{
			return Joins;
		}
		std::vector<FOpenEdge> Edges;
		for (std::size_t Piece = 0; Piece < Pieces.size(); ++Piece)
		{
			AddOpenEdges(Scene, Pieces, static_cast<uint16>(Piece), Edges);
		}
		std::vector<std::vector<FCandidate>> Candidates(Pieces.size());
		FindSeams(Pieces, Edges, Candidates);
		std::vector<std::vector<FTarget>> Targets;
		for (std::size_t Piece = 0; Piece < Pieces.size(); ++Piece)
		{
			Targets.push_back(Choose(Pieces[Piece], Pieces, Candidates[Piece]));
		}
		FResolver Resolver(Scene, Pieces, Targets);
		std::vector<std::vector<FResolver::FEnd>> Ends(Pieces.size());
		for (std::size_t Piece = 0; Piece < Pieces.size(); ++Piece)
		{
			for (std::size_t Place = 0; Place < Pieces[Piece].Local.size(); ++Place)
			{
				Ends[Piece].push_back(Resolver.EndOf(static_cast<uint16>(Piece), static_cast<uint16>(Place)));
			}
		}
		FollowJoined(Pieces, Targets, Resolver, Ends);
		for (std::size_t Piece = 0; Piece < Pieces.size(); ++Piece)
		{
			const FPiece& Fitted = Pieces[Piece];
			for (std::size_t Vertex = 0; Vertex < Fitted.PlaceOf.size(); ++Vertex)
			{
				const FResolver::FEnd& End = Ends[Piece][Fitted.PlaceOf[Vertex]];
				if (End.bJoined)
				{
					Joins.push_back({Fitted.Object, static_cast<uint16>(Vertex), Pieces[End.Piece].Object,
						{static_cast<float>(End.Local.X), static_cast<float>(End.Local.Y), static_cast<float>(End.Local.Z)}});
				}
			}
		}
		return Joins;
	}
}
