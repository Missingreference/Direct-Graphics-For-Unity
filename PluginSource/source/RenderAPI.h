#pragma once

#include "Unity/IUnityGraphics.h"

#include <stddef.h>

struct IUnityInterfaces;


// Super-simple "graphics abstraction". This is nothing like how a proper platform abstraction layer would look like;
// all this does is a base interface for whatever our plugin sample needs. Which is only "draw some triangles"
// and "modify a texture" at this point.
//
// There are implementations of this base class for D3D9, D3D11, OpenGL etc.; see individual RenderAPI_* files.
class RenderAPI
{
public:
	virtual ~RenderAPI() { }


	// Process general event like initialization, shutdown, device loss/reset etc.
	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) = 0;

	// Is the API using "reversed" (1.0 at near plane, 0.0 at far plane) depth buffer?
	// Reversed Z is used on modern platforms, and improves depth buffer precision.
	virtual bool GetUsesReverseZ() = 0;

	// Begin modifying texture data. You need to pass texture width/height too, since some graphics APIs
	// (e.g. OpenGL ES) do not have a good way to query that from the texture itself...
	//
	// Returns pointer into the data buffer to write into (or NULL on failure), and pitch in bytes of a single texture row.
	virtual void* BeginModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int* outRowPitch) = 0;
	// End modifying texture data.
	virtual void EndModifyTexture(void* textureHandle, int textureWidth, int textureHeight, int rowPitch, void* dataPtr) = 0;

	virtual void DoCopyTexture(void *sourceTexture, int sourceX, int sourceY, int sourceWidth, int sourceHeight, void *destinationTexture, int destinationX, int destinationY) = 0;
    virtual bool CreateTexture(int width, int height, int format, int textureIndex) = 0;
	virtual void DestroyTexture(int textureIndex) = 0;
	virtual void* GetTexturePointer(int textureIndex) = 0;
    virtual void SetTextureColor(float red, float green, float blue, float alpha, void* targetTexture) = 0;
};


// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);

