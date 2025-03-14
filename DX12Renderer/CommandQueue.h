/**
 * Wrapper class for a ID3D12CommandQueue.
 */

#pragma once

#include <d3d12.h>  // For ID3D12CommandQueue, ID3D12Device2, and ID3D12Fence
#include <wrl.h>    // For Microsoft::WRL::ComPtr

#include <cstdint>  // For uint64_t
#include <queue>    // For std::queue

using namespace Microsoft::WRL;

class CommandQueue
{
public:
    CommandQueue(Microsoft::WRL::ComPtr<ID3D12Device2> device, D3D12_COMMAND_LIST_TYPE type);
    virtual ~CommandQueue();

    // Get an available command list from the command queue.
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> GetCommandList();

    // Execute a command list.
    // Returns the fence value to wait for for this command list.
    uint64_t ExecuteCommandList(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList);
    void ExecuteCommandList();

    /// <summary>
    /// Allows you to update/upload data to the GPU
    /// </summary>
    /// <param name="resource"></param>
    /// <param name="subresource"></param>
    void UploadData(ComPtr<ID3D12Resource> resource, D3D12_SUBRESOURCE_DATA subresource, UINT subresourceNumber = 1);
    void UploadData(ID3D12Resource* resource, std::vector<D3D12_SUBRESOURCE_DATA> subresources, UINT subresourceNumber = 1);

    uint64_t Signal();
    bool IsFenceComplete(uint64_t fenceValue);
    void WaitForFenceValue(uint64_t fenceValue);
    void Flush();

    Microsoft::WRL::ComPtr<ID3D12CommandQueue> GetD3D12CommandQueue() const;
protected:

    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator();
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> CreateCommandList(Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator);

private:
    // Keep track of command allocators that are "in-flight"
    struct CommandAllocatorEntry
    {
        uint64_t fenceValue;
        Microsoft::WRL::ComPtr<ID3D12CommandAllocator> commandAllocator;
    };

    using CommandAllocatorQueue = std::queue<CommandAllocatorEntry>;
    using CommandListQueue = std::queue< Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> >;

    D3D12_COMMAND_LIST_TYPE                     m_CommandListType;
    Microsoft::WRL::ComPtr<ID3D12Device2>       m_d3d12Device;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>  m_d3d12CommandQueue;
    Microsoft::WRL::ComPtr<ID3D12Fence>         m_d3d12Fence;
    HANDLE                                      m_FenceEvent;
    uint64_t                                    m_FenceValue;

    CommandAllocatorQueue                       m_CommandAllocatorQueue;
    CommandListQueue                            m_CommandListQueue;

    ComPtr<ID3D12CommandAllocator> m_CommandAllocator;
    ComPtr<ID3D12GraphicsCommandList2> m_CommandList;
};