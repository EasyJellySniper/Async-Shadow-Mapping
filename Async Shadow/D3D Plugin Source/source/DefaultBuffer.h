#pragma once
#include "stdafx.h"

template<typename T>
class DefaultBuffer
{
public:
	DefaultBuffer() {}

	bool Init(ID3D12Device* device, UINT elementCount, D3D12_RESOURCE_STATES initialState)
	{
		mElementByteSize = sizeof(T);

		HRESULT hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize*elementCount),
			initialState,
			nullptr,
			IID_PPV_ARGS(&mDefaultBuffer));

		if (FAILED(hr))
		{
			return false;
		}
		return true;
	}

	DefaultBuffer(const DefaultBuffer& rhs) = delete;
	DefaultBuffer& operator=(const DefaultBuffer& rhs) = delete;
	~DefaultBuffer()
	{
		SafeReset(mDefaultBuffer);
	}

	ID3D12Resource* Resource()const
	{
		return mDefaultBuffer.Get();
	}

private:
	ComPtr<ID3D12Resource> mDefaultBuffer;
	UINT mElementByteSize = 0;
};
