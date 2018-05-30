#include "RenderAPI.h"
#include "PlatformBase.h"
#include "stdafx.h"
#include "ShadowMap.h"

// Direct3D 12 implementation of RenderAPI.


#if SUPPORT_D3D12

class RenderAPI_D3D12 : public RenderAPI
{
public:

	RenderAPI_D3D12();
	virtual ~RenderAPI_D3D12() { }

	virtual void ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces);
	virtual void SetRenderMethod(bool _useIndirect, bool _useBundle);
	virtual bool CheckDevice();

	virtual bool CreateResources();
	virtual void ReleaseResources();
	virtual void WaitGPU(int _frameIndex);

	virtual bool SetMeshData(void* _vertexBuffer, void* _indexBuffer, int _vertexCount, int _indexCount);
	virtual void SetTextureData(void* _texture);
	virtual bool SetShadowTextureData(void* _shadowTexture);
	virtual void WorkerThread();
	virtual void NotifyShadowThread(bool _multithread);
	virtual void InternalUpdate();
	virtual bool RenderShadows();
	virtual void SetObjectMatrix(int _index, XMMATRIX _matrix);
	virtual void SetObjTextureIndex(int _index, int _val);
	virtual void SetLightTransform(float *_lightPos, float *_lightDir, float _radius);
	virtual float *GetLightTransform();
	virtual double GetShadowTime();

private:
	void ToNextFrame();
	void ExecuteAndTiming();
	void ExecuteCmdList(ID3D12GraphicsCommandList *_cmdList);

	IUnityGraphicsD3D12v2* s_D3D12;

	// command for using in render thread
	ComPtr<ID3D12CommandAllocator> renderCmdAllocator[NumOfFrameResources];
	ComPtr<ID3D12GraphicsCommandList> renderCmdGraphicList[NumOfFrameResources];
	ComPtr<ID3D12CommandQueue> renderQueue;

	// fence
	HANDLE fenceEvent;
	ComPtr<ID3D12Fence> renderFence;
	UINT64 renderFenceValue[NumOfFrameResources];
	int frameIndex = 0;

	// shadow instance
	unique_ptr<ShadowMap> shadowMap;

	// thread HANDLE
	HANDLE beginShadowThread = nullptr;
	double shadowTime;

	// drawing flag
	bool useIndirect;
	bool useBundle;
};

RenderAPI* CreateRenderAPI_D3D12()
{
	return new RenderAPI_D3D12();
}

const UINT kNodeMask = 0;

RenderAPI_D3D12::RenderAPI_D3D12()
	: s_D3D12(NULL)
{

}

bool RenderAPI_D3D12::CreateResources()
{
	// ------------------------------------------------------- Create Frame Resource
	// ------------------------------------------------------- Any resource that would be updated per frame is recommended to use a circular list access.
	for (int i = 0; i < NumOfFrameResources; i++)
	{
		renderFenceValue[i] = 0;
	}
	frameIndex = 0;

	shadowMap = make_unique<ShadowMap>(s_D3D12->GetDevice());
	useIndirect = false;
	useBundle = false;

	// ------------------------------------------------------- Create Thread
	// a handle for waiting main thread signal
	beginShadowThread = CreateEvent(NULL, FALSE, FALSE, NULL);

	return true;
}

void RenderAPI_D3D12::ReleaseResources()
{
	SafeClose(beginShadowThread);

	for (int i = 0; i < NumOfFrameResources; i++)
	{
		WaitGPU(i);
		SafeReset(renderCmdGraphicList[i]);
		SafeReset(renderCmdAllocator[i]);
	}

	SafeClose(fenceEvent);
	SafeReset(shadowMap);
	SafeReset(renderFence);
	SafeReset(renderQueue);

	for (int i = 0; i < NumOfFrameResources; i++)
	{
		renderFenceValue[i] = 0;
	}
	frameIndex = 0;
}

void RenderAPI_D3D12::WaitGPU(int _frameIndex)
{
	if (renderQueue == nullptr ||
		renderFence == nullptr)
	{
		return;
	}

	if (FAILED(renderQueue->Signal(renderFence.Get(), renderFenceValue[_frameIndex])))
	{
		return;
	}

	fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
	if (FAILED(renderFence->SetEventOnCompletion(renderFenceValue[_frameIndex], fenceEvent)))
	{
		return;
	}
	WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);

	renderFenceValue[_frameIndex]++;
}

void RenderAPI_D3D12::ToNextFrame()
{
	// Schedule a Signal command in the queue.
	const UINT64 currentFenceValue = renderFenceValue[frameIndex];
	if (FAILED(renderQueue->Signal(renderFence.Get(), currentFenceValue)))
	{
		return;
	}

	// Cycle through the circular frame resource array.
	frameIndex = (frameIndex + 1) % NumOfFrameResources;

	// If the next frame is not ready to be rendered yet, wait until it is ready.
	if (renderFence->GetCompletedValue() < renderFenceValue[frameIndex])
	{
		fenceEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		if (FAILED(renderFence->SetEventOnCompletion(renderFenceValue[frameIndex], fenceEvent)))
		{
			return;
		}
		WaitForSingleObjectEx(fenceEvent, INFINITE, FALSE);
	}

	// Set the fence value for the next frame.
	renderFenceValue[frameIndex] = currentFenceValue + 1;
}

void RenderAPI_D3D12::ExecuteAndTiming()
{
	// debug timer
	LARGE_INTEGER frequency;        // ticks per second
	LARGE_INTEGER t1, t2;           // ticks

									// get ticks per second
	QueryPerformanceFrequency(&frequency);

	QueryPerformanceCounter(&t1);
	InternalUpdate();
	RenderShadows();
	QueryPerformanceCounter(&t2);

	shadowTime = (t2.QuadPart - t1.QuadPart) * 1000.0 / frequency.QuadPart;
}

void RenderAPI_D3D12::ExecuteCmdList(ID3D12GraphicsCommandList * _cmdList)
{
	ID3D12CommandList* ppCommandLists[] = { _cmdList };
	renderQueue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
}

void RenderAPI_D3D12::ProcessDeviceEvent(UnityGfxDeviceEventType type, IUnityInterfaces* interfaces)
{
	switch (type)
	{
	case kUnityGfxDeviceEventInitialize:
		s_D3D12 = interfaces->Get<IUnityGraphicsD3D12v2>();
		CreateResources();
		break;
	case kUnityGfxDeviceEventShutdown:
		ReleaseResources();
		break;
	}
}

void RenderAPI_D3D12::SetRenderMethod(bool _useIndirect, bool _useBundle)
{
	useIndirect = _useIndirect;
	useBundle = _useBundle;
}

bool RenderAPI_D3D12::CheckDevice()
{
	if (s_D3D12->GetDevice() == nullptr)
	{
		return false;
	}

	for (int i = 0; i < NumOfFrameResources; i++)
	{
		if (FAILED(s_D3D12->GetDevice()->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&renderCmdAllocator[i]))))
		{
			return false;
		}

		if (FAILED(s_D3D12->GetDevice()->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, renderCmdAllocator[i].Get(), nullptr, IID_PPV_ARGS(&renderCmdGraphicList[i]))))
		{
			return false;
		}

		if (FAILED(renderCmdGraphicList[i]->Close()))
		{
			return false;
		}

		D3D12_COMMAND_QUEUE_DESC queueDesc = {};
		queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
		queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
		if (FAILED(s_D3D12->GetDevice()->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&renderQueue))))
		{
			return false;
		}

		if (FAILED(s_D3D12->GetDevice()->CreateFence(renderFenceValue[i], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&renderFence))))
		{
			return false;
		}
		renderFenceValue[i]++;
	}

	return true;
}

bool RenderAPI_D3D12::SetMeshData(void * _vertexBuffer, void * _indexBuffer, int _vertexCount, int _indexCount)
{
	D3D12_RESOURCE_DESC desc;

	// copy vertex buffer
	ID3D12Resource *VB = (ID3D12Resource*)_vertexBuffer;
	if (VB == nullptr)
	{
		return false;
	}
	desc = VB->GetDesc();

	D3D12_VERTEX_BUFFER_VIEW vbv;
	vbv.BufferLocation = VB->GetGPUVirtualAddress();
	vbv.SizeInBytes = (UINT)desc.Width;							// width equals to size byte if it is a buffer resource
	vbv.StrideInBytes = (UINT)desc.Width / _vertexCount;

	// copy index buffer
	ID3D12Resource *IB = (ID3D12Resource*)_indexBuffer;
	if (IB == nullptr)
	{
		return false;
	}
	desc = IB->GetDesc();

	D3D12_INDEX_BUFFER_VIEW ibv;
	ibv.BufferLocation = IB->GetGPUVirtualAddress();
	ibv.SizeInBytes = (UINT)desc.Width;							// width equals to size byte if it is a buffer resource
	ibv.Format = DXGI_FORMAT_R32_UINT;

	shadowMap->AddMesh(vbv, ibv);

	return true;
}

void RenderAPI_D3D12::SetTextureData(void * _texture)
{
	shadowMap->AddCutoutTexture((ID3D12Resource*)_texture);
}

bool RenderAPI_D3D12::SetShadowTextureData(void * _shadowTexture)
{
	ID3D12Resource *st = (ID3D12Resource*)_shadowTexture;
	if (st == nullptr)
	{
		return false;
	}

	if (!shadowMap->CreateShadowDsv(st))
	{
		return false;
	}

	if (!shadowMap->CreateSrv())
	{
		return false;
	}

	if (!shadowMap->CreateRootSignature())
	{
		return false;
	}

	if (!shadowMap->CreatePSOs())
	{
		return false;
	}

	if (!shadowMap->CreateConstantBuffers())
	{
		return false;
	}

	if (!shadowMap->CreateIndirectBuffer(renderCmdGraphicList, renderCmdAllocator))
	{
		return false;
	}
	
	// submit init job
	for (int i = 0; i < NumOfFrameResources; i++)
	{
		ExecuteCmdList(renderCmdGraphicList[i].Get());
		WaitGPU(i);
	}

	if (!shadowMap->CreateShadowBundle())
	{
		return false;
	}

	return true;
}

void RenderAPI_D3D12::WorkerThread()
{
	while (true)
	{
		WaitForSingleObject(beginShadowThread, INFINITE);
		ExecuteAndTiming();
	}
}

void RenderAPI_D3D12::NotifyShadowThread(bool _multithread)
{
	if (_multithread)
	{
		SetEvent(beginShadowThread);
	}
	else
	{
		ExecuteAndTiming();
	}
}

void RenderAPI_D3D12::InternalUpdate()
{
	shadowMap->UpdateConstantBuffer(frameIndex);
}

bool RenderAPI_D3D12::RenderShadows()
{
	auto cmdAlloc = renderCmdAllocator[frameIndex];
	auto cmdList = renderCmdGraphicList[frameIndex];

	// reset command list
	if (FAILED(cmdAlloc->Reset())
		|| FAILED(cmdList->Reset(cmdAlloc.Get(), nullptr)))
	{
		return false;
	}

	shadowMap->RenderShadow(cmdList.Get(), frameIndex, useIndirect, useBundle);

	if (HRESULT hr = FAILED(cmdList->Close()))
	{
		return false;
	}

	// Execute the rendering work.
	ExecuteCmdList(cmdList.Get());

	ToNextFrame();

	return true;
}

void RenderAPI_D3D12::SetObjectMatrix(int _index, XMMATRIX _matrix)
{
	shadowMap->SetObjectTransform(_index, XMMatrixTranspose(_matrix));
}

void RenderAPI_D3D12::SetObjTextureIndex(int _index, int _val)
{
	shadowMap->SetObjTextureIndex(_index, _val);
}

void RenderAPI_D3D12::SetLightTransform(float *_lightPos, float *_lightDir, float _radius)
{
	// calculate light transform
	XMVECTOR lightPos = XMVectorSet(_lightPos[0], _lightPos[1], _lightPos[2], 0.0f);
	XMVECTOR targetPos = XMVectorSet(_lightPos[0] + _lightDir[0], _lightPos[1] + _lightDir[1], _lightPos[2] + _lightDir[2], 0.0f);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(XMLoadFloat3(&XMFLOAT3(0.0f, 0.0f, 0.0f)), lightView));

	float l = sphereCenterLS.x - _radius;
	float b = sphereCenterLS.y - _radius;
	float n = sphereCenterLS.z - _radius;
	float r = sphereCenterLS.x + _radius;
	float t = sphereCenterLS.y + _radius;
	float f = sphereCenterLS.z + _radius;

	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	XMMATRIX viewProj = lightView * lightProj;
	viewProj = XMMatrixTranspose(viewProj);

	shadowMap->SetShadowTransform(viewProj);
}

float * RenderAPI_D3D12::GetLightTransform()
{
	float *m = new float[16];

	XMFLOAT4X4 shadowTransform = shadowMap->GetShadowTransform();

	m[0] = shadowTransform._11;
	m[1] = shadowTransform._12;
	m[2] = shadowTransform._13;
	m[3] = shadowTransform._14;

	m[4] = shadowTransform._21;
	m[5] = shadowTransform._22;
	m[6] = shadowTransform._23;
	m[7] = shadowTransform._24;

	m[8] = shadowTransform._31;
	m[9] = shadowTransform._32;
	m[10] = shadowTransform._33;
	m[11] = shadowTransform._34;

	m[12] = shadowTransform._41;
	m[13] = shadowTransform._42;
	m[14] = shadowTransform._43;
	m[15] = shadowTransform._44;

	return m;
}

double RenderAPI_D3D12::GetShadowTime()
{
	return shadowTime;
}

#endif // #if SUPPORT_D3D12
