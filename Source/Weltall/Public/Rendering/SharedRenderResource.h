#pragma once

#include "CoreMinimal.h"
#include "RenderingThread.h"
#include "RenderResource.h"

/**
 * Starts initialising a resource made on the game thread and shares it between the screen and the
 * frames it queues: it is released on the rendering thread when the last reference goes, after any
 * frame drawing with it.
 */
template <typename TResource>
TSharedRef<TResource> ShareRenderResource(TResource* Resource)
{
	BeginInitResource(Resource);
	return TSharedRef<TResource>(Resource, [](TResource* Released)
	{
		ENQUEUE_RENDER_COMMAND(ReleaseSharedRenderResource)([Released](FRHICommandListImmediate&)
		{
			Released->ReleaseResource();
			delete Released;
		});
	});
}
