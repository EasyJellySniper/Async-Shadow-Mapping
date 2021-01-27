#pragma once

#include "Unity/IUnityGraphics.h"

#include <stddef.h>
#include <DirectXMath.h>
using namespace DirectX;

struct IUnityInterfaces;

class RenderAPI
{
public:
	virtual ~RenderAPI() { }


	// Process general event like initialization, shutdown, device loss/reset etc.
	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces) = 0;
	virtual void SetRenderMethod(bool _useIndirect, bool _useBundle) = 0;

	virtual bool CreateResources() = 0;
	virtual void ReleaseResources() = 0;
	virtual void WaitGPU(int _frameIndex) = 0;

	virtual bool CheckDevice() = 0;
	virtual bool SetMeshData(void* _vertexBuffer, void* _indexBuffer, int _vertexCount, int _indexCount) = 0;
	virtual void SetTextureData(void* _texture) = 0;
	virtual bool SetShadowTextureData(void* _shadowTexture) = 0;
	virtual void WorkerThread() = 0;
	virtual void NotifyShadowThread(bool _multithread, float _fakeDelay) = 0;
	virtual void InternalUpdate() = 0;
	virtual bool RenderShadows() = 0;
	virtual void SetObjectMatrix(int _index, XMMATRIX _matrix) = 0;
	virtual void SetObjTextureIndex(int _index, int _val) = 0;
	virtual void SetLightTransform(float *_lightPos, float *_lightDir, float _radius) = 0;
	virtual float *GetLightTransform() = 0;
	virtual double GetShadowTime() = 0;
};


// Create a graphics API implementation instance for the given API type.
RenderAPI* CreateRenderAPI(UnityGfxRenderer apiType);

