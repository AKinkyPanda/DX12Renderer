#pragma once

#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 Specific headers
#include <d3d12.h>
#include <dxgi1_6.h>
#include "d3dx12.h"

class Device;

/// <summary>
/// A wrapper for a DX Descriptor heap. Is able to return CPU/GPU handles to descriptors
/// with a given index. You are able to retrieve a 'fresh/new' index, ensuring that the slot 
/// isn't already taken.
/// </summary>
class DescriptorHeap
{
public:
	DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT amountOfDescriptors, bool isShaderVisible);

	/// <summary>
	/// Returns a pointer to the DirectX Descriptor Heap
	/// </summary>
	/// <returns></returns>
	ComPtr<ID3D12DescriptorHeap> GetDescriptorHeap();

	/// <summary>
	/// Returns a CPU handle with a given offset based on the passed along index
	/// </summary>
	/// <param name="index"></param>
	/// <returns></returns>
	CD3DX12_CPU_DESCRIPTOR_HANDLE GetCPUHandleAt(UINT index);

	/// <summary>
	/// Returns a GPU handle with a given offset based on the passed along index
	/// </summary>
	/// <param name="index"></param>
	/// <returns></returns>
	CD3DX12_GPU_DESCRIPTOR_HANDLE GetGPUHandleAt(UINT index);

	/// <summary>
	/// Returns the size of the descriptor (type)
	/// </summary>
	/// <returns></returns>
	UINT GetDescriptorSize();

	/// <summary>
	/// Retrieves a 'fresh/new' index, use this to ensure you get an available
	/// slot inside of the descriptor heap.
	/// </summary>
	/// <returns></returns>
	UINT GetNextIndex();

private:
	ComPtr<ID3D12DescriptorHeap> m_descriptorHeap;
	UINT m_descriptorSize = 0;
	UINT m_maxDescriptorIndex = 0;
	UINT m_descriptorIndex = 0;
};