#pragma once

namespace Weltall
{
	/** The fixes of the world map's broken effects: each on unless the original's behavior is asked for. */
	struct FWorldMapOptions
	{
		/**
		 * The story's landmark (object 69 and those riding it) comes back after a battle while the story
		 * shows it, as the world map's entry shows it; the original's return hides it until the world map
		 * is next entered from the field.
		 */
		bool bStoryObjectBackAfterBattle = true;
		/**
		 * Scene 14's noise sweep (its effect 0Bh) plays on a pair of its own, heard with the tone; the original starts both
		 * on one pair, so the tone takes the sweep's channels at once and the sweep is never heard.
		 */
		bool bScene14SweepHeard = true;
		/**
		 * Scene 18's effects wait until Mahanon's task starts them, so the falls in view get the particles; the original's
		 * run from the scene's load at the world's origin, out of sight, and hold nearly half of them.
		 */
		bool bScene18EffectsWait = true;
		/**
		 * Trees that overlap are drawn the farther first; the original's ordering table sorts them sixteen units of depth
		 * at a time and within an entry by their place in their lists, so two overlapping trees change places back and
		 * forth as the camera's move takes one of them across an entry's edge.
		 */
		bool bTreesInDepthOrder = true;

		/** Every fix off: the original's behavior throughout. */
		static constexpr FWorldMapOptions Original()
		{
			FWorldMapOptions Options;
			Options.bStoryObjectBackAfterBattle = false;
			Options.bScene14SweepHeard = false;
			Options.bScene18EffectsWait = false;
			Options.bTreesInDepthOrder = false;
			return Options;
		}
	};
}
