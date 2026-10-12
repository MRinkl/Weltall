#include "ScenePolygonOrder.h"

#include "FieldDrawBuilder.h"

namespace
{
	void AddScenePolygonRun(TArray<FFieldDrawRun>& Runs, const FFieldDrawRun& Run, bool bJoinable)
	{
		if (bJoinable && !Runs.IsEmpty())
		{
			FFieldDrawRun& Last = Runs.Last();
			if (Last.Pass == Run.Pass && Last.Blend == Run.Blend && Last.Depth == Run.Depth && Last.bDepthWrite == Run.bDepthWrite && Last.Rule == Run.Rule
				&& Last.FirstVertex + Last.VertexCount == Run.FirstVertex)
			{
				Last.VertexCount += Run.VertexCount;
				return;
			}
		}
		Runs.Add(Run);
	}

	EFieldDepthRule RuleOf(const FScenePolygonItem& Item)
	{
		return Item.bModel ? EFieldDepthRule::Model : Item.bScenery ? EFieldDepthRule::Scenery : EFieldDepthRule::Entry;
	}

	bool HasOpaqueTexels(const FScenePolygonItem& Item)
	{
		return Item.bTextured || !Item.bSemiTransparent;
	}

	uint32 AddScenePolygonToOrder(FFieldSceneDraw& Draw, const FScenePolygonItem& Item, EFieldPass Pass, Weltall::EBlendMode Blend, EFieldRunDepth Depth,
		bool bDepthWrite, bool bJoinable, EFieldDepthRule Rule = EFieldDepthRule::Entry)
	{
		const uint32 First = static_cast<uint32>(Draw.Order.Num());
		for (uint32 Corner = 0; Corner < Item.CornerCount; ++Corner)
		{
			Draw.Order.Add(Item.FirstCorner + Corner);
		}
		AddScenePolygonRun(Draw.Runs, {First, Item.CornerCount, static_cast<uint8>(Pass), Blend, Depth, bDepthWrite, Rule}, bJoinable);
		return First;
	}

	/** As the GPU draws a polygon: opaque; a semi-transparent textured one's plain texels, then its blended ones; an untextured one blended. */
	void AddLayeredScenePolygon(FFieldSceneDraw& Draw, const FScenePolygonItem& Item, Weltall::EBlendMode UntexturedBlend, EFieldRunDepth Depth, bool bDepthWrite,
		EFieldDepthRule Rule = EFieldDepthRule::Entry)
	{
		if (!Item.bSemiTransparent)
		{
			AddScenePolygonToOrder(Draw, Item, EFieldPass::Opaque, Weltall::EBlendMode::Average, Depth, bDepthWrite, true, Rule);
		}
		else if (Item.bTextured)
		{
			const uint32 First = AddScenePolygonToOrder(Draw, Item, EFieldPass::PlainTexels, Weltall::EBlendMode::Average, Depth, bDepthWrite, false, Rule);
			AddScenePolygonRun(Draw.Runs, {First, Item.CornerCount, static_cast<uint8>(EFieldPass::SemiTransparentTexels), Item.PageBlend, Depth, false, Rule}, false);
		}
		else
		{
			AddScenePolygonToOrder(Draw, Item, EFieldPass::BlendAll, UntexturedBlend, Depth, false, true, Rule);
		}
	}
}

void AddScenePolygonsInPaintersOrder(FFieldSceneDraw& Draw, TConstArrayView<FScenePolygonItem> Items)
{
	// An untextured semi-transparent polygon blends as the last textured one drawn before it set the GPU.
	Weltall::EBlendMode Current = Weltall::EBlendMode::Average;
	for (const FScenePolygonItem& Item : Items)
	{
		if (Item.bTextured || Item.bSetsBlend)
		{
			Current = Item.PageBlend;
		}
		AddLayeredScenePolygon(Draw, Item, Current, EFieldRunDepth::None, false);
	}
}

void AddScenePolygonsUnderDepthRule(FFieldSceneDraw& Draw, TConstArrayView<FScenePolygonItem> Items)
{
	// The sprites' and the models' opaque texels mark their pixels; the scenery sorted by depth writes the depth of its own,
	// what is drawn over the models once they have marked theirs, beside them.
	for (const FScenePolygonItem& Item : Items)
	{
		const EFieldPass Plain = Item.bSemiTransparent ? EFieldPass::PlainTexels : EFieldPass::Opaque;
		if (Item.bSprite || ((Item.bModel || (Item.bScenery && !Item.bOverModels)) && HasOpaqueTexels(Item)))
		{
			AddScenePolygonToOrder(Draw, Item, Plain, Weltall::EBlendMode::Average, EFieldRunDepth::Mask, true, true, RuleOf(Item));
		}
	}
	for (const FScenePolygonItem& Item : Items)
	{
		if (Item.bScenery && Item.bOverModels && HasOpaqueTexels(Item))
		{
			const EFieldPass Plain = Item.bSemiTransparent ? EFieldPass::PlainTexels : EFieldPass::Opaque;
			AddScenePolygonToOrder(Draw, Item, Plain, Weltall::EBlendMode::Average, EFieldRunDepth::MaskBesideModels, true, true, EFieldDepthRule::Scenery);
		}
	}
	Weltall::EBlendMode Current = Weltall::EBlendMode::Average;
	for (const FScenePolygonItem& Item : Items)
	{
		if (Item.bTextured || Item.bSetsBlend)
		{
			Current = Item.PageBlend;
		}
		if (Item.bSprite)
		{
			const EFieldPass Plain = Item.bSemiTransparent ? EFieldPass::PlainTexels : EFieldPass::Opaque;
			const uint32 First = AddScenePolygonToOrder(Draw, Item, Plain, Weltall::EBlendMode::Average, EFieldRunDepth::InsideMask, false, false);
			if (Item.bSemiTransparent)
			{
				AddScenePolygonRun(Draw.Runs,
					{First, Item.CornerCount, static_cast<uint8>(EFieldPass::SemiTransparentTexels), Item.PageBlend, EFieldRunDepth::Masked, false}, false);
			}
			AddScenePolygonRun(Draw.Runs, {First, Item.CornerCount, static_cast<uint8>(Plain), Weltall::EBlendMode::Average, EFieldRunDepth::PushedBack, true},
				false);
		}
		else if (Item.bModel && HasOpaqueTexels(Item))
		{
			// A model's opaque texels against its prepass's depth, which within the margin lets the order decide among the models.
			const EFieldPass Plain = Item.bSemiTransparent ? EFieldPass::PlainTexels : EFieldPass::Opaque;
			const uint32 First = AddScenePolygonToOrder(Draw, Item, Plain, Weltall::EBlendMode::Average, EFieldRunDepth::InsideMask, false,
				!Item.bSemiTransparent, EFieldDepthRule::Model);
			if (Item.bSemiTransparent)
			{
				AddScenePolygonRun(Draw.Runs,
					{First, Item.CornerCount, static_cast<uint8>(EFieldPass::SemiTransparentTexels), Item.PageBlend, EFieldRunDepth::Masked, false,
						EFieldDepthRule::Model},
					false);
			}
		}
		else
		{
			const EFieldRunDepth Depth = Item.bShadow ? EFieldRunDepth::OutsideMask
				: Item.bBackground || Item.bInOrder	  ? EFieldRunDepth::None
				: Item.bOverModels					  ? EFieldRunDepth::OverModels
													  : EFieldRunDepth::Masked;
			AddLayeredScenePolygon(Draw, Item, Current, Depth, true, RuleOf(Item));
		}
	}
}
