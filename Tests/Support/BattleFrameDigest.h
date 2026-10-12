#pragma once

#include "Foundation/Sha1.h"
#include "Foundation/Types.h"
#include "Support/OracleDigest.h"

#include <functional>
#include <string>
#include <vector>

namespace Weltall
{
	struct FBattleBackdropFrame;
	struct FBattleObjectLink;
	struct FBattleObjectsFrame;
	struct FBattleShowFade;
	struct FBattleShowFrame;
	struct FBattleShowLink;
	struct FBattleShowQuad;
	struct FBattleShowTriangle;
	struct FGte;
}

namespace Weltall::Testing
{
	/**
	 * What the stage or an object linked, as the objects' drawing oracle digests its packet
	 * in the original: what drew it (a part's slot, bone and polygon; an object's shadow; a
	 * cloth's triangle; a trail's quad), its entry, its code, its corners on the screen under Gte's projection, the
	 * colours the packet holds (none for the kinds the original lights once as it builds them), its texels, CLUT and
	 * page.
	 */
	void DigestBattleObjectLink(FOracleDigest& Digest, const FBattleObjectsFrame& Frame, const FGte& Gte, const FBattleObjectLink& Link);

	/**
	 * The stage's backdrop as the stage's oracle digests its packets, in
	 * the GPU's order: the dome's, then the second panorama's, then the first's; BeforePacket, when given, is called
	 * before each.
	 */
	void DigestBattleBackdrop(FOracleDigest& Digest, const FBattleBackdropFrame& Frame, const std::function<void()>& BeforePacket = {});

	/** A quad's packet as the original's routine for its drawer writes it, digested as the show's oracle digests an FT4. */
	void DigestBattleShowQuad(FOracleDigest& Digest, const FBattleShowFrame& Frame, const FBattleShowQuad& Quad);
	/** A marker's triangle as its packet holds it, digested as the show's oracle digests an F3 or a G3. */
	void DigestBattleShowTriangle(FOracleDigest& Digest, const FBattleShowFrame& Frame, const FBattleShowLink& Link, const FBattleShowTriangle& Triangle);
	/** The fade's two packets: its drawing mode, then the tile over the screen. */
	void DigestBattleShowFade(FOracleDigest& Digest, const FBattleShowLink& Link, const FBattleShowFade& Fade);

	/**
	 * A frame's whole drawing as the program's oracle digests the ordering table:
	 * its code, then the SHA-1 of every link in the GPU's order, each after its kind's mark: the HUD's draw as one mark
	 * (its packets are digested apart), the stage's and the objects' links, the backdrop's packets, the show's quads,
	 * triangles and fade. Gte holds the show's projection.
	 */
	void DigestBattleFrameDrawing(FOracleDigest& Digest, const FBattleShowFrame& Frame, const FGte& Gte);
	/**
	 * The SHA-1 of the frame's links alone, which the oracle prints for each frame (`--frames`); with Lines, each link
	 * as the oracle lists a frame's packets (`--table`): its kind, then its values in hexadecimal (the stage's, the
	 * objects' and the backdrop's as 32-bit words, the show's as bytes).
	 */
	FSha1::FDigest BattleFrameDrawing(const FBattleShowFrame& Frame, const FGte& Gte, std::vector<std::string>* Lines = nullptr);
}
