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

#include "ShadowMap.h"

ShadowMap::ShadowMap(ID3D12Device * _device)
{
	device = _device;
}

ShadowMap::~ShadowMap()
{
	vertexBufferView.clear();
	indexBufferView.clear();
	shadowObjectMatrix.clear();
	cutoutMaps.clear();
	shadowObjTextureIndex.clear();

	for (int i = 0; i < NumOfFrameResources; i++)
	{
		shadowCommands[i].clear();
		SafeReset(shadowObjectCB[i]);
		SafeReset(shadowLightCB[i]);
		SafeReset(shadowIndirectBuffer[i]);
		SafeReset(shadowIndirectUploader[i]);
		SafeReset(bundleCmdAlloc[i]);
		SafeReset(bundleCmdList[i]);
	}

	SafeReset(shadowDsvHeap);
	SafeReset(cutoutSrvHeap);
	SafeReset(shadowRS);
	SafeReset(shadowPSO);
	SafeReset(shadowVS);
	SafeReset(shadowPS);
	SafeReset(shadowCmdSignature);
}

void ShadowMap::AddMesh(D3D12_VERTEX_BUFFER_VIEW _vbv, D3D12_INDEX_BUFFER_VIEW _ibv)
{
	vertexBufferView.push_back(_vbv);
	indexBufferView.push_back(_ibv);
}

void ShadowMap::AddCutoutTexture(ID3D12Resource * _texture)
{
	cutoutMaps.push_back(_texture);
}

void ShadowMap::SetShadowTransform(XMMATRIX _m)
{
	XMStoreFloat4x4(&shadowTransform, _m);
}

XMFLOAT4X4 ShadowMap::GetShadowTransform()
{
	return shadowTransform;
}

void ShadowMap::SetObjectTransform(int _index, XMMATRIX _m)
{
	if (_index >= 0 && _index < (int)shadowObjectMatrix.size())
	{
		XMStoreFloat4x4(&shadowObjectMatrix[_index], _m);
	}
}

void ShadowMap::SetObjTextureIndex(int _index, int _val)
{
	if (_index >= 0 && _index < (int)shadowObjTextureIndex.size())
	{
		shadowObjTextureIndex[_index] = _val;
	}
}

void ShadowMap::UpdateConstantBuffer(int _frameIndex)
{
	for (int i = 0; i < (int)shadowObjectMatrix.size(); i++)
	{
		ObjectConstants objectConstants;
		objectConstants.World = shadowObjectMatrix[i];
		objectConstants.texIndex = shadowObjTextureIndex[i];
		shadowObjectCB[_frameIndex]->CopyData(i, objectConstants);
	}

	// update to constant buffer
	LightConstants lightConstants;
	XMStoreFloat4x4(&lightConstants.ViewProj, XMLoadFloat4x4(&shadowTransform));
	shadowLightCB[_frameIndex]->CopyData(0, lightConstants);
}

void ShadowMap::RenderShadow(ID3D12GraphicsCommandList * _cmdList, int _frameIndex, bool _indirect, bool _useBundle)
{
	auto shadowHeap = CD3DX12_CPU_DESCRIPTOR_HANDLE(shadowDsvHeap->GetCPUDescriptorHandleForHeapStart(), 0, dsvDescriptorSize);

	// ----------------------------- set view port
	_cmdList->RSSetViewports(1, &shadowViewport);
	_cmdList->RSSetScissorRects(1, &shadowScissorRect);

	// ----------------------------- rendering shadow map
	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(unityShadowResource,
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	_cmdList->ClearDepthStencilView(shadowHeap,
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	_cmdList->OMSetRenderTargets(0, nullptr, false, &shadowHeap);

	// ----------------------------- bind pipeline state & root signature & texture heap
	_cmdList->SetPipelineState(shadowPSO.Get());
	_cmdList->SetGraphicsRootSignature(shadowRS.Get());

	ID3D12DescriptorHeap* descriptorHeaps[] = { cutoutSrvHeap.Get() };
	_cmdList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	// render object by record draw or indirect
	if (!_indirect)
	{
		if (_useBundle)
		{
			_cmdList->ExecuteBundle(bundleCmdList[_frameIndex].Get());
		}
		else
		{
			RenderShadowObjects(_cmdList, _frameIndex);
		}
	}
	else
	{
		RenderShadowIndirect(_cmdList, _frameIndex);
	}

	_cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(unityShadowResource,
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void ShadowMap::RenderShadowObjects(ID3D12GraphicsCommandList * _cmdList, int _frameIndex)
{
	// ------------------------------------------------------------- Draw Index
	UINT objCBByteSize = sizeof(ObjectConstants);
	auto objectCB = shadowObjectCB[_frameIndex]->Resource();
	_cmdList->SetGraphicsRootConstantBufferView(1, shadowLightCB[_frameIndex]->Resource()->GetGPUVirtualAddress());

	for (int i = 0; i < (int)vertexBufferView.size(); i++)
	{
		_cmdList->IASetVertexBuffers(0, 1, &vertexBufferView[i]);
		_cmdList->IASetIndexBuffer(&indexBufferView[i]);
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + i*objCBByteSize;

		_cmdList->SetGraphicsRootConstantBufferView(0, objCBAddress);
		_cmdList->DrawIndexedInstanced(indexBufferView[i].SizeInBytes / 4, 1, 0, 0, 0);
	}
}

void ShadowMap::RenderShadowIndirect(ID3D12GraphicsCommandList * _cmdList, int _frameIndex)
{
	// ------------------------------------------------------------- Indirect Drawing
	_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	_cmdList->ExecuteIndirect(shadowCmdSignature.Get(),
		(UINT)shadowCommands[_frameIndex].size(),
		shadowIndirectBuffer[_frameIndex]->Resource(),
		0,
		nullptr,
		0
	);
}

bool ShadowMap::CreateShadowDsv(ID3D12Resource *_unityResource)
{
	unityShadowResource = _unityResource;

	// decide depth buffer view format according to unity
	D3D12_RESOURCE_DESC desc = unityShadowResource->GetDesc();

	if (desc.Format == DXGI_FORMAT_R16_TYPELESS)
	{
		shadowFormat = DXGI_FORMAT_D16_UNORM;
	}
	else if (desc.Format == DXGI_FORMAT_R32_TYPELESS)
	{
		shadowFormat = DXGI_FORMAT_D32_FLOAT;
	}

	shadowClearValue.Format = shadowFormat;
	shadowClearValue.DepthStencil.Depth = 1.0f;
	shadowClearValue.DepthStencil.Stencil = 0;

	shadowViewport = { 0.0f, 0.0f, (float)desc.Width, (float)desc.Height, 0.0f, 1.0f };
	shadowScissorRect = { 0, 0, (int)desc.Width, (int)desc.Height };

	// create depth stencil view heap
	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
	dsvHeapDesc.NumDescriptors = 1;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;

	if (FAILED(device->CreateDescriptorHeap(
		&dsvHeapDesc, IID_PPV_ARGS(shadowDsvHeap.GetAddressOf()))))
	{
		return false;
	}

	dsvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

	// Create DSV to resource so we can render to the shadow map.
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Format = shadowFormat;
	dsvDesc.Texture2D.MipSlice = 0;
	device->CreateDepthStencilView(unityShadowResource, &dsvDesc,
		CD3DX12_CPU_DESCRIPTOR_HANDLE(shadowDsvHeap->GetCPUDescriptorHandleForHeapStart(), 0, dsvDescriptorSize));

	return true;
}

bool ShadowMap::CreateRootSignature()
{
	CD3DX12_ROOT_PARAMETER slotRootParameter[3];
	slotRootParameter[0].InitAsConstantBufferView(0);		// register b0
	slotRootParameter[1].InitAsConstantBufferView(1);		// register b1

	CD3DX12_DESCRIPTOR_RANGE texTable;						// srv table for textures
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, MaxTexture, 0);	// register t0
	slotRootParameter[2].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);

	// define sampler state
	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		0, // shaderRegister
		D3D12_FILTER_ANISOTROPIC,         // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		16);


	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
		1, &anisotropicWrap,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// serialize root signature for creating
	ComPtr<ID3DBlob> serializedRootSig = nullptr;

	if (FAILED(D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), nullptr)))
	{
		return false;
	}

	if (FAILED(device->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(shadowRS.GetAddressOf()))))
	{
		return false;
	}

	return true;
}

bool ShadowMap::CreatePSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc;
	ZeroMemory(&shadowPsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));

	// build shader input layout
	vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	// complie vertex shader
	if (FAILED(D3DCompileFromFile(L"Assets//Shaders//AsyncShadow.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "VS", "vs_5_1", 0, 0, &shadowVS, nullptr)))
	{
		return false;
	}

	// complie fragment shader
	if (FAILED(D3DCompileFromFile(L"Assets//Shaders//AsyncShadow.hlsl", nullptr, D3D_COMPILE_STANDARD_FILE_INCLUDE, "PS", "ps_5_1", 0, 0, &shadowPS, nullptr)))
	{
		return false;
	}

	// create pso
	shadowPsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	shadowPsoDesc.pRootSignature = shadowRS.Get();
	shadowPsoDesc.VS =
	{
		reinterpret_cast<BYTE*>(shadowVS->GetBufferPointer()),
		shadowVS->GetBufferSize()
	};
	shadowPsoDesc.PS =
	{
		reinterpret_cast<BYTE*>(shadowPS->GetBufferPointer()),
		shadowPS->GetBufferSize()
	};
	shadowPsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	shadowPsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	shadowPsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	shadowPsoDesc.SampleMask = UINT_MAX;
	shadowPsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;

	shadowPsoDesc.NumRenderTargets = 0;
	shadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN;
	shadowPsoDesc.SampleDesc.Count = 1;
	shadowPsoDesc.SampleDesc.Quality = 0;
	shadowPsoDesc.DSVFormat = shadowFormat;

	shadowPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;

	if (FAILED(device->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&shadowPSO))))
	{
		return false;
	}

	return true;
}

bool ShadowMap::CreateConstantBuffers()
{
	if ((int)vertexBufferView.size() != 0)
	{
		bool result = true;
		for (int i = 0; i < NumOfFrameResources; i++)
		{
			shadowObjectCB[i] = make_unique<UploadBuffer<ObjectConstants>>();
			result = shadowObjectCB[i]->Init(device, (UINT)vertexBufferView.size(), true);
			if (!result)
			{
				return false;
			}

			shadowLightCB[i] = make_unique<UploadBuffer<LightConstants>>();
			result = shadowLightCB[i]->Init(device, 1, true);
			if (!result)
			{
				return false;
			}
		}

		shadowObjectMatrix.resize(vertexBufferView.size());
		shadowObjTextureIndex.resize(vertexBufferView.size());

		return true;
	}

	return false;
}

bool ShadowMap::CreateSrv()
{
	// --------------------------------------------------- Create SRV Heap
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc;
	srvHeapDesc.NumDescriptors = MaxTexture;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	srvHeapDesc.NodeMask = 0;

	if (FAILED(device->CreateDescriptorHeap(
		&srvHeapDesc, IID_PPV_ARGS(cutoutSrvHeap.GetAddressOf()))))
	{
		return false;
	}

	srvDescriptorSize = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	// --------------------------------------------------- Creat SRV
	for (int i = 0; i < MaxTexture; i++)
	{
		auto srvAddress = CD3DX12_CPU_DESCRIPTOR_HANDLE(cutoutSrvHeap->GetCPUDescriptorHandleForHeapStart(), i, srvDescriptorSize);
		if (i < (int)cutoutMaps.size())
		{
			// actual texture desc
			D3D12_SHADER_RESOURCE_VIEW_DESC texDesc = {};
			texDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			texDesc.Format = cutoutMaps[i]->GetDesc().Format;
			texDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			texDesc.Texture2D.MostDetailedMip = 0;
			texDesc.Texture2D.MipLevels = cutoutMaps[i]->GetDesc().MipLevels;
			texDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			device->CreateShaderResourceView(cutoutMaps[i], &texDesc, srvAddress);
		}
		else
		{
			// null descriptor
			D3D12_SHADER_RESOURCE_VIEW_DESC texDesc = {};
			texDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
			texDesc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
			texDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
			texDesc.Texture2D.MostDetailedMip = 0;
			texDesc.Texture2D.MipLevels = 0;
			texDesc.Texture2D.ResourceMinLODClamp = 0.0f;
			device->CreateShaderResourceView(nullptr, &texDesc, srvAddress);
		}
	}

	return true;
}

bool ShadowMap::CreateIndirectBuffer(ComPtr<ID3D12GraphicsCommandList> *_cmdLists, ComPtr<ID3D12CommandAllocator> *_cmdAllocs)
{
	// -------------------------------------------------------------------------- create command signature here
	D3D12_INDIRECT_ARGUMENT_DESC shadowIndirectDesc[5] = {};
	shadowIndirectDesc[0].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
	shadowIndirectDesc[0].ConstantBufferView.RootParameterIndex = 0;
	shadowIndirectDesc[1].Type = D3D12_INDIRECT_ARGUMENT_TYPE_CONSTANT_BUFFER_VIEW;
	shadowIndirectDesc[1].ConstantBufferView.RootParameterIndex = 1;
	shadowIndirectDesc[2].Type = D3D12_INDIRECT_ARGUMENT_TYPE_VERTEX_BUFFER_VIEW;
	shadowIndirectDesc[2].VertexBuffer.Slot = 0;
	shadowIndirectDesc[3].Type = D3D12_INDIRECT_ARGUMENT_TYPE_INDEX_BUFFER_VIEW;
	shadowIndirectDesc[4].Type = D3D12_INDIRECT_ARGUMENT_TYPE_DRAW_INDEXED;

	D3D12_COMMAND_SIGNATURE_DESC commandSignatureDesc = {};
	commandSignatureDesc.pArgumentDescs = shadowIndirectDesc;
	commandSignatureDesc.NumArgumentDescs = _countof(shadowIndirectDesc);
	commandSignatureDesc.ByteStride = sizeof(ShadowIndirect);

	if (FAILED(device->CreateCommandSignature(&commandSignatureDesc, shadowRS.Get(), IID_PPV_ARGS(&shadowCmdSignature))))
	{
		return false;
	}

	// -------------------------------------------------------------------------- create indirect buffer resource
	for (int i = 0; i < NumOfFrameResources; i++)
	{
		shadowIndirectBuffer[i] = make_unique<DefaultBuffer<ShadowIndirect>>();
		if (!shadowIndirectBuffer[i]->Init(device, (UINT)vertexBufferView.size(), D3D12_RESOURCE_STATE_COPY_DEST))
		{
			return false;
		}

		shadowIndirectUploader[i] = make_unique<UploadBuffer<ShadowIndirect>>();
		if(!shadowIndirectUploader[i]->Init(device, (UINT)vertexBufferView.size(), false))
		{
			return false;
		}
	}

	// -------------------------------------------------------------------------- create indirect drawing data
	UINT objCBByteSize = sizeof(ObjectConstants);

	for (int i = 0; i < NumOfFrameResources; i++)
	{
		if (FAILED(_cmdLists[i]->Reset(_cmdAllocs[i].Get(), nullptr)))
		{
			return false;
		}

		for (int j = 0; j < (int)vertexBufferView.size(); j++)
		{
			ShadowIndirect si;

			si.objectCbv = shadowObjectCB[i]->Resource()->GetGPUVirtualAddress() + j * objCBByteSize;
			si.lightCbv = shadowLightCB[i]->Resource()->GetGPUVirtualAddress();
			si.vbv = vertexBufferView[j];
			si.ibv = indexBufferView[j];
			si.drawIndexArgus.BaseVertexLocation = 0;
			si.drawIndexArgus.StartIndexLocation = 0;
			si.drawIndexArgus.StartInstanceLocation = 0;
			si.drawIndexArgus.InstanceCount = 1;
			si.drawIndexArgus.IndexCountPerInstance = indexBufferView[j].SizeInBytes / 4;

			shadowCommands[i].push_back(si);
		}

		// copy data by uploader
		D3D12_SUBRESOURCE_DATA commandData = {};
		commandData.pData = shadowCommands[i].data();
		commandData.RowPitch = sizeof(ShadowIndirect) * (UINT)vertexBufferView.size();
		commandData.SlicePitch = commandData.RowPitch;

		UpdateSubresources<1>(_cmdLists[i].Get(), shadowIndirectBuffer[i]->Resource(), shadowIndirectUploader[i]->Resource(), 0, 0, 1, &commandData);
		_cmdLists[i]->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(shadowIndirectBuffer[i]->Resource(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT));

		if (FAILED(_cmdLists[i]->Close()))
		{
			return false;
		}
	}

	return true;
}

bool ShadowMap::CreateShadowBundle()
{
	for (int i = 0; i < NumOfFrameResources; i++)
	{
		if (FAILED(device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_BUNDLE, IID_PPV_ARGS(&bundleCmdAlloc[i]))))
		{
			return false;
		}

		if (FAILED(device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_BUNDLE, bundleCmdAlloc[i].Get(), nullptr, IID_PPV_ARGS(&bundleCmdList[i]))))
		{
			return false;
		}

		// ---------------------------------- record bundles
		bundleCmdList[i]->SetGraphicsRootSignature(shadowRS.Get());		// record root signature so that bundle can inherit state from caller command list
		bundleCmdList[i]->SetPipelineState(shadowPSO.Get());			// inheriting didn't contain pso state, we must record to bundle
		RenderShadowObjects(bundleCmdList[i].Get(), i);

		if (FAILED(bundleCmdList[i]->Close()))
		{
			return false;
		}
	}

	return true;
}
