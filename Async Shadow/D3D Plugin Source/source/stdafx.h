#pragma once

#include <assert.h>
#include <d3d12.h>
#include "Unity/IUnityGraphicsD3D12.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <memory>
#include <wrl.h>
#include <DirectXMath.h>
#include <D3Dcompiler.h>
#include <process.h>
using namespace DirectX;
#pragma comment(lib,"D3D12.lib")
#pragma comment(lib,"D3DCompiler.lib")

using namespace std;
using Microsoft::WRL::ComPtr;
#include "d3dx12.h"

const int NumOfFrameResources = 3;

static DirectX::XMFLOAT4X4 Identity4x4(
	1.0f, 0.0f, 0.0f, 0.0f,
	0.0f, 1.0f, 0.0f, 0.0f,
	0.0f, 0.0f, 1.0f, 0.0f,
	0.0f, 0.0f, 0.0f, 1.0f);

template <class T> void SafeReset(ComPtr<T> &t)
{
	if (t != nullptr)
	{
		t.Reset();
		t = nullptr;
	}
}

template <class T> void SafeReset(unique_ptr<T> &t)
{
	if (t != nullptr)
	{
		t.reset();
		t = nullptr;
	}
}

inline void SafeClose(HANDLE _handle)
{
	if (_handle != nullptr)
	{
		CloseHandle(_handle);
		_handle = nullptr;
	}
}
