// Example low level rendering Unity plugin

#include "PlatformBase.h"
#include "RenderAPI.h"
#include "stdafx.h"

static RenderAPI* s_CurrentAPI = NULL;
static HANDLE shadowThread = nullptr;
struct threadwrapper
{
	static unsigned int WINAPI thunk(LPVOID lpParameter)
	{
		s_CurrentAPI->WorkerThread();
		return 0;
	}
};

// check d3d device
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CheckDevice()
{
	return s_CurrentAPI->CheckDevice();
}

// create resource
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API CreateResources()
{
	bool result = s_CurrentAPI->CreateResources();
	shadowThread = reinterpret_cast<HANDLE>(_beginthreadex(
		nullptr,
		0,
		threadwrapper::thunk,
		nullptr,
		0,
		nullptr));

	return result;
}

// release resource
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API ReleaseResources()
{
	SafeClose(shadowThread);

	s_CurrentAPI->ReleaseResources();
}

// get mesh data from unity
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SendMeshData(void* _vertexBuffer, void* _indexBuffer, int _vertexCount, int _indexCount)
{
	return s_CurrentAPI->SetMeshData(_vertexBuffer, _indexBuffer, _vertexCount, _indexCount);
}

// get render texture data from Unity
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SendTextureData(void* _texture)
{
	return s_CurrentAPI->SetTextureData(_texture);
}

// get render texture data from Unity
extern "C" bool UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SendShadowTextureData(void* _shadowTexture)
{
	return s_CurrentAPI->SetShadowTextureData(_shadowTexture);
}

// render shadows
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RenderShadows(bool _multithread)
{
	s_CurrentAPI->NotifyShadowThread(_multithread);
}

// set matrix
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetObjectTransform(int _index, float *_pos, float *_scale, float *_rot)
{
	XMMATRIX m = XMMatrixScaling(_scale[0], _scale[1], _scale[2])
		* XMMatrixRotationQuaternion(XMVectorSet(_rot[0], _rot[1], _rot[2], _rot[3]))
		* XMMatrixTranslation(_pos[0], _pos[1], _pos[2]);

	s_CurrentAPI->SetObjectMatrix(_index, m);
}

// set texture index
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetObjTextureIndex(int _index, int _val)
{
	s_CurrentAPI->SetObjTextureIndex(_index, _val);
}

// set light transform
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetLightTransform(float *_lightPos, float *_lightDir, float _radius)
{
	s_CurrentAPI->SetLightTransform(_lightPos, _lightDir, _radius);
}

// get shadow transform
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetLightTransform(float *_shadow)
{
	float *m = s_CurrentAPI->GetLightTransform();
	for (int i = 0; i < 16; i++)
	{
		_shadow[i] = m[i];
	}
}

// get shadow render time
extern "C" double UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetShadowRenderTime()
{
	return s_CurrentAPI->GetShadowTime();
}

// set indirect drawing
extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API SetRenderMethod(bool _useIndirect, bool _useBundle)
{
	s_CurrentAPI->SetRenderMethod(_useIndirect, _useBundle);
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
	
	// Run OnGraphicsDeviceEvent(initialize) manually on plugin load
	OnGraphicsDeviceEvent(kUnityGfxDeviceEventInitialize);
}

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API UnityPluginUnload()
{
	s_Graphics->UnregisterDeviceEventCallback(OnGraphicsDeviceEvent);
}

#if UNITY_WEBGL
typedef void	(UNITY_INTERFACE_API * PluginLoadFunc)(IUnityInterfaces* unityInterfaces);
typedef void	(UNITY_INTERFACE_API * PluginUnloadFunc)();

extern "C" void	UnityRegisterRenderingPlugin(PluginLoadFunc loadPlugin, PluginUnloadFunc unloadPlugin);

extern "C" void UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API RegisterPlugin()
{
	UnityRegisterRenderingPlugin(UnityPluginLoad, UnityPluginUnload);
}
#endif

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
	}
}

static void UNITY_INTERFACE_API OnRenderEvent(int eventID)
{
	// Unknown / unsupported graphics device type? Do nothing
	if (s_CurrentAPI == NULL)
		return;
}

// --------------------------------------------------------------------------
// GetRenderEventFunc, an example function we export which is used to get a rendering event callback function.

extern "C" UnityRenderingEvent UNITY_INTERFACE_EXPORT UNITY_INTERFACE_API GetRenderEventFunc()
{
	return OnRenderEvent;
}

