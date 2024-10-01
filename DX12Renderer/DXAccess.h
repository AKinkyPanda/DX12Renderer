#pragma once

class Application;
//class Device;
class CommandQueue;
class Window;
class DescriptorHeap;
class PipelineState;
class CommandQueue;

/// <summary>
/// namespace with functions that access the user to
/// Custom DX components like DXDevice.
/// </summary>

enum class HeapType
{
	CBV_SRV_UAV, // Constant buffer/Shader resource/Unordered access views
	DSV,		 // Depth stencil view
	RTV			 // Render target view
};

/// <summary>
/// Global getters that are defined in the Windows implementation of renderer
/// </summary>
namespace DXAccess
{
	//ComPtr<Device> GetDevice();
	//std::shared_ptr<CommandQueue> GetCommandQueue();
	//std::shared_ptr<Window> GetWindow();
	//std::shared_ptr<DescriptorHeap> GetDescriptorHeap(HeapType type);

	//Microsoft::WRL::ComPtr<ID3D12Device2> GetDeviceTemp();
	//std::shared_ptr<CommandQueue> GetCommandQueueTemp();
	//std::shared_ptr<ID3D12DescriptorHeap> GetDescriptorHeapTemp(HeapType type);
}