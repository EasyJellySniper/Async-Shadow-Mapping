//MIT License
//
//Copyright(c) 2018 Chieh Hung Liu(Squall Liu)
//
//Permission is hereby granted, free of charge, to any person obtaining a copy
//of this software and associated documentation files(the "Software"), to deal
//in the Software without restriction, including without limitation the rights
//to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
//copies of the Software, and to permit persons to whom the Software is
//furnished to do so, subject to the following conditions :
//
//The above copyright notice and this permission notice shall be included in all
//copies or substantial portions of the Software.
//
//THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
//IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
//FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
//AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
//LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
//OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
//SOFTWARE.

#pragma once
#include "stdafx.h"
#include "UploadBuffer.h"
#include "DefaultBuffer.h"

struct ObjectConstants
{
	XMFLOAT4X4 World = Identity4x4;
	UINT texIndex = -1;
	float padding[47];		// padding to 256 bytes
};

struct LightConstants
{
	XMFLOAT4X4 ViewProj = Identity4x4;
	float padding[48];		// padding to 256 bytes
};

const int MaxTexture = 16;

class ShadowMap
{
public:
	ShadowMap(ID3D12Device *_device);
	~ShadowMap();

	void AddMesh(D3D12_VERTEX_BUFFER_VIEW _vbv, D3D12_INDEX_BUFFER_VIEW _ibv);
	void AddCutoutTexture(ID3D12Resource *_texture);
	void SetShadowTransform(XMMATRIX _m);
	XMFLOAT4X4 GetShadowTransform();
	void SetObjectTransform(int _index, XMMATRIX _m);
	void SetObjTextureIndex(int _index, int _val);

	void UpdateConstantBuffer(int _frameIndex);
	void RenderShadow(ID3D12GraphicsCommandList *_cmdList, int _frameIndex, bool _indirect, bool _useBundle);
	bool CreateShadowDsv(ID3D12Resource *_unityResource);
	bool CreateRootSignature();
	bool CreatePSOs();
	bool CreateConstantBuffers();
	bool CreateSrv();
	bool CreateIndirectBuffer(ComPtr<ID3D12GraphicsCommandList> *_cmdLists, ComPtr<ID3D12CommandAllocator> *_cmdAllocs);
	bool CreateShadowBundle();

private:
	void RenderShadowObjects(ID3D12GraphicsCommandList *_cmdList, int _frameIndex);
	void RenderShadowIndirect(ID3D12GraphicsCommandList * _cmdList, int _frameIndex);

	// device cache
	ID3D12Device *device = nullptr;

	// mesh
	vector<D3D12_VERTEX_BUFFER_VIEW> vertexBufferView;
	vector<D3D12_INDEX_BUFFER_VIEW> indexBufferView;

	// shadow resources
	ID3D12Resource *unityShadowResource;
	D3D12_CLEAR_VALUE shadowClearValue;
	DXGI_FORMAT shadowFormat = DXGI_FORMAT_D16_UNORM;

	// descriptors
	ComPtr<ID3D12DescriptorHeap> shadowDsvHeap = nullptr;
	UINT dsvDescriptorSize;

	// view port
	D3D12_VIEWPORT shadowViewport;
	D3D12_RECT shadowScissorRect;

	// root signature
	ComPtr<ID3D12RootSignature> shadowRS = nullptr;

	// pipeline state object
	ComPtr<ID3D12PipelineState> shadowPSO = nullptr;
	ComPtr<ID3DBlob> shadowVS = nullptr;
	ComPtr<ID3DBlob> shadowPS = nullptr;

	// object transform
	unique_ptr<UploadBuffer<ObjectConstants>> shadowObjectCB[NumOfFrameResources];
	vector<XMFLOAT4X4> shadowObjectMatrix;
	vector<int> shadowObjTextureIndex;

	// shadow transform
	unique_ptr<UploadBuffer<LightConstants>> shadowLightCB[NumOfFrameResources];
	XMFLOAT4X4 shadowTransform = Identity4x4;

	// indirect drawing
	struct ShadowIndirect
	{
		D3D12_GPU_VIRTUAL_ADDRESS objectCbv;
		D3D12_GPU_VIRTUAL_ADDRESS lightCbv;
		D3D12_VERTEX_BUFFER_VIEW vbv;
		D3D12_INDEX_BUFFER_VIEW ibv;
		D3D12_DRAW_INDEXED_ARGUMENTS drawIndexArgus;
	};

	ComPtr<ID3D12CommandSignature> shadowCmdSignature = nullptr;
	unique_ptr<DefaultBuffer<ShadowIndirect>> shadowIndirectBuffer[NumOfFrameResources];
	unique_ptr<UploadBuffer<ShadowIndirect>> shadowIndirectUploader[NumOfFrameResources];
	vector<ShadowIndirect> shadowCommands[NumOfFrameResources];

	// texture resource (for cutout)
	vector<ID3D12Resource*> cutoutMaps;
	ComPtr<ID3D12DescriptorHeap> cutoutSrvHeap = nullptr;
	UINT srvDescriptorSize;

	// rendering bundles
	ComPtr<ID3D12CommandAllocator> bundleCmdAlloc[NumOfFrameResources];
	ComPtr<ID3D12GraphicsCommandList> bundleCmdList[NumOfFrameResources];
};