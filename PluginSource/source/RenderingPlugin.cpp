// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"

#include <assert.h>
#include <math.h>
#include <vector>

using namespace std;

static RenderAPI* s_CurrentAPI = NULL;

static int m_TextureCount = 0;
static vector<int> m_FreeTextureIndexs;
static int m_FreeIndexCount = 0;

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CopyTextures(void* textureHandle, int x, int y, int w, int h, void* textureHandle2, int x2, int y2)
{
	s_CurrentAPI->DoCopyTexture(textureHandle, x, y, w, h, textureHandle2, x2, y2);
}

extern "C" UNITY_INTERFACE_EXPORT int UNITY_INTERFACE_API CreateNativeTexture(int width, int height, int format)
{
	int textureIndex;
	if (m_FreeIndexCount != 0)
	{
		textureIndex = m_FreeTextureIndexs.back();
        m_FreeTextureIndexs.pop_back();
        m_FreeIndexCount--;
	}
	else
	{
		textureIndex = m_TextureCount;
	}

    if(s_CurrentAPI->CreateTexture(width, height, format, textureIndex) && s_CurrentAPI->GetTexturePointer(textureIndex) != nullptr)
    {
        m_TextureCount++;
        return textureIndex;
    }
    
    //Texture creation failed
    m_FreeTextureIndexs.push_back(textureIndex);
    m_FreeIndexCount++;
    return -1;
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API DestroyNativeTexture(int textureIndex)
{
	m_TextureCount--;
	m_FreeTextureIndexs.push_back(textureIndex);
	m_FreeIndexCount++;
	s_CurrentAPI->DestroyTexture(textureIndex);
}

extern "C" UNITY_INTERFACE_EXPORT void* UNITY_INTERFACE_API GetNativeTexturePointer(int textureIndex)
{
	return s_CurrentAPI->GetTexturePointer(textureIndex);
}

extern "C" UNITY_INTERFACE_EXPORT void UNITY_INTERFACE_API SetTextureColor(float red, float green, float blue, float alpha, void* targetTexture)
{
    return s_CurrentAPI->SetTextureColor(red, green, blue, alpha, targetTexture);
}




// --------------------------------------------------------------------------
// UnitySetInterfaces

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType);

static IUnityInterfaces* s_UnityInterfaces = NULL;
static IUnityGraphics* s_Graphics = NULL;

extern "C" void	UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginLoad(IUnityInterfaces* unityInterfaces)
{
	s_UnityInterfaces = unityInterfaces;
	s_Graphics = s_UnityInterfaces->Get<IUnityGraphics>();
	s_Graphics->RegisterDeviceEventCallback(OnGraphicsDeviceEvent);
	
#if SUPPORT_VULKAN
	if (s_Graphics->GetRenderer() == kUnityGfxRendererNull)
	{
		extern void RenderAPI_Vulkan_OnPluginLoad(IUnityInterfaces*);
		RenderAPI_Vulkan_OnPluginLoad(unityInterfaces);
	}
#endif // SUPPORT_VULKAN

	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

// --------------------------------------------------------------------------
// GraphicsDeviceEvent


static UnityGfxRenderer s_DeviceType = kUnityGfxRendererNull;

static void UNITY_INTERFACE_API OnGraphicsDeviceEvent(UnityGfxDeviceEventType eventType)
{
	// Create graphics API implementation upon initialization
	if (eventType == kUnityGfxDeviceEventInitialize)
	{
		assert(s_CurrentAPI == NULL);
		s_DeviceType = s_Graphics->GetRenderer();
		s_CurrentAPI = CreateRenderAPI(s_DeviceType);
	}

	// Let the implementation process the device related events
	if (s_CurrentAPI)
	{
		s_CurrentAPI->ProcessDeviceEvent(eventType, s_UnityInterfaces);
	}

	// Cleanup graphics API implementation upon shutdown
	if (eventType == kUnityGfxDeviceEventShutdown)
	{
		delete s_CurrentAPI;
		s_CurrentAPI = NULL;
		s_DeviceType = kUnityGfxRendererNull;

		m_TextureCount = 0;
		m_FreeIndexCount = 0;
	}
}



// --------------------------------------------------------------------------
// OnRenderEvent
// This will be called for GL.IssuePluginEvent script calls; eventID will
// be the integer passed to IssuePluginEvent. In this example, we just ignore
// that value.


static void DrawColoredTriangle()
{
	// Draw a colored triangle. Note that colors will come out differently
	// in D3D and OpenGL, for example, since they expect color bytes
	// in different ordering.
	struct MyVertex
	{
		float x, y, z;
		unsigned int color;
	};
	MyVertex verts[3] =
	{
		{ -0.5f, -0.25f,  0, 0xFFff0000 },
		{ 0.5f, -0.25f,  0, 0xFF00ff00 },
		{ 0,     0.5f ,  0, 0xFF0000ff },
	};

	// Transformation matrix: rotate around Z axis based on time.
	float phi = 0.0f; // time set externally from Unity script
	float cosPhi = cosf(phi);
	float sinPhi = sinf(phi);
	float depth = 0.7f;
	float finalDepth = s_CurrentAPI->GetUsesReverseZ() ? 1.0f - depth : depth;
	float worldMatrix[16] = {
		cosPhi,-sinPhi,0,0,
		sinPhi,cosPhi,0,0,
		0,0,1,0,
		0,0,finalDepth,1,
	};

	//s_CurrentAPI->DrawSimpleTriangles(worldMatrix, 1, verts);
}


static void ModifyTexturePixels()
{
	/*
	void* textureHandle = g_TextureHandle;
	int width = g_TextureWidth;
	int height = g_TextureHeight;
	void* textureHandle2 = g_TextureHandle2;
	int width2 = g_TextureWidth2;
	int height2 = g_TextureHeight2;
	if (!textureHandle || !textureHandle2)
		return;

	int textureRowPitch;
	void* textureDataPtr = s_CurrentAPI->BeginModifyTexture(textureHandle, width, height, &textureRowPitch);
	if (!textureDataPtr)
		return;
	*/


	/*
	const float t = g_Time * 4.0f;

	unsigned char* dst = (unsigned char*)textureDataPtr;
	for (int y = 0; y < height; ++y)
	{
		unsigned char* ptr = dst;
		for (int x = 0; x < width; ++x)
		{
			// Simple "plasma effect": several combined sine waves
			int vv = int(
				(127.0f + (127.0f * sinf(x / 7.0f + t))) +
				(127.0f + (127.0f * sinf(y / 5.0f - t))) +
				(127.0f + (127.0f * sinf((x + y) / 6.0f - t))) +
				(127.0f + (127.0f * sinf(sqrtf(float(x*x + y*y)) / 4.0f - t)))
				) / 4;

			// Write the texture pixel
			ptr[0] = vv;
			ptr[1] = vv;
			ptr[2] = vv;
			ptr[3] = vv;

			// To next pixel (our pixels are 4 bpp)
			ptr += 4;
		}

		// To next image row
		dst += textureRowPitch;
	}
	*/


	//s_CurrentAPI->EndModifyTexture(textureHandle, width, height, textureRowPitch, textureDataPtr);

    /*
	for(size_t i = 0; i < blitTextureInputs.size(); i++)
	{
		Texture2DPair texturePair = blitTextureInputs[i];
		s_CurrentAPI->DoCopyTexture(texturePair.texturePointer, texturePair.width, texturePair.heigh, texturePair.texturePointer2, texturePair.width2, texturePair.height2);
	}
	blitTextureInputs.clear();

	status = 1.0f;
     */
}


static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL)
		return;

	DrawColoredTriangle();
	ModifyTexturePixels();
	//ModifyVertexBuffer();
}



// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}

