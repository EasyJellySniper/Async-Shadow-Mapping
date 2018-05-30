#pragma once
#include "stdafx.h"

template<typename T>
class UploadBuffer
{
public:
	UploadBuffer() {}

	bool Init(ID3D12Device* device, UINT elementCount, bool isConstantBuffer)
	{
		mIsConstantBuffer = isConstantBuffer;

		mElementByteSize = sizeof(T);
		if (isConstantBuffer)
		{
			mElementByteSize = (mElementByteSize + 255) & ~255;
		}

		HRESULT hr = device->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(mElementByteSize*elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&mUploadBuffer));

		if (FAILED(hr))
		{
			return false;
		}

		hr = mUploadBuffer->Map(0, nullptr, reinterpret_cast<void**>(&mMappedData));

		if (FAILED(hr))
		{
			return false;
		}

		return true;
	}

	UploadBuffer(const UploadBuffer& rhs) = delete;
	UploadBuffer& operator=(const UploadBuffer& rhs) = delete;
	~UploadBuffer()
	{
		if (mUploadBuffer != nullptr)
		{
			mUploadBuffer->Unmap(0, nullptr);
		}

		mMappedData = nullptr;

		SafeReset(mUploadBuffer);
	}

	ID3D12Resource* Resource()const
	{
		return mUploadBuffer.Get();
	}

	void CopyData(int elementIndex, const T& data)
	{
		memcpy(&mMappedData[elementIndex*mElementByteSize], &data, sizeof(T));
	}

private:
	ComPtr<ID3D12Resource> mUploadBuffer;
	BYTE* mMappedData = nullptr;

	UINT mElementByteSize = 0;
	bool mIsConstantBuffer = false;
};
