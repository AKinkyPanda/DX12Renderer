#include "DescriptorHeap.h"
#include <cassert>
#include "Helpers.h"
#include "DXAccess.h"
#include "Application.h"

DescriptorHeap::DescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT amountOfDescriptors, bool isShaderVisible)
{
	ComPtr<ID3D12Device2> device = Application::Get().GetDevice();

	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = type;
	heapDesc.NumDescriptors = amountOfDescriptors;
	heapDesc.Flags = isShaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	ThrowIfFailed(device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&m_descriptorHeap)));
	m_descriptorSize = device->GetDescriptorHandleIncrementSize(type);

	m_maxDescriptorIndex = amountOfDescriptors - 1;

	m_freeFlags.assign(m_maxDescriptorIndex, false);
	m_nextFree = 0;
}

ComPtr<ID3D12DescriptorHeap> DescriptorHeap::GetDescriptorHeap()
{
	return m_descriptorHeap;
}

UINT DescriptorHeap::GetDescriptorSize()
{
	return m_descriptorSize;
}

UINT DescriptorHeap::GetNextIndex()
{
	UINT idx = UINT_MAX;
	// 1) Try reuse from free list
	if (!m_freeList.empty())
	{
		idx = m_freeList.top();
		m_freeList.pop();
		m_freeFlags[idx] = false;
		return idx;
	}
	// 2) Allocate new if within capacity
	if (m_nextFree < m_maxDescriptorIndex)
	{
		idx = m_nextFree++;
		// m_freeFlags[idx] is already false
		return idx;
	}
	// 3) Out of descriptors
	assert(!"DescriptorHeap: out of descriptors");
	return UINT_MAX;
}

void DescriptorHeap::FreeIndex(UINT index)
{
	if (index >= m_maxDescriptorIndex)
		return; // out of range
	if (m_freeFlags[index])
		return; // already freed
	// Mark free and push to free-list
	m_freeFlags[index] = true;
	m_freeList.push(index);
}

CD3DX12_CPU_DESCRIPTOR_HANDLE DescriptorHeap::GetCPUHandleAt(UINT index)
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetCPUDescriptorHandleForHeapStart(), index, m_descriptorSize);
}

CD3DX12_GPU_DESCRIPTOR_HANDLE DescriptorHeap::GetGPUHandleAt(UINT index)
{
	return CD3DX12_GPU_DESCRIPTOR_HANDLE(m_descriptorHeap->GetGPUDescriptorHandleForHeapStart(), index, m_descriptorSize);
}