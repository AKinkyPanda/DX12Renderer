#pragma once
#include <cstdint>

//class Device;
class CommandQueue;
class DescriptorHeap;
class PipelineState;

class Window;

class Mesh;
class Model;
class Texture;

namespace DXInternal
{
	float m_clearColor[4] = { 0.05f, 0.05f, 0.05f, 1.0f };

	//ComPtr<Device> m_Device = nullptr;
	std::shared_ptr<CommandQueue> m_CommandQueue = nullptr;
	std::shared_ptr<DescriptorHeap> m_CBVHeap = nullptr;
	std::shared_ptr<DescriptorHeap> m_RTVHeap = nullptr;
	std::shared_ptr<DescriptorHeap> m_DSVHeap = nullptr;
	std::shared_ptr<PipelineState> m_Pipeline = nullptr;
				   

	std::shared_ptr<Window> m_Window = nullptr;

	std::shared_ptr<ID3D12DescriptorHeap> m_BaseHeap = nullptr;

	uint32_t m_frameCount = 0;
	uint32_t m_drawCalls = 0;
}