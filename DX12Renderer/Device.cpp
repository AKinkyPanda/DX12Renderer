#include "Device.h"
#include "DX12Renderer/Helpers.h"

Device::Device()
{
	CreateFactory();
	CreateDevice();
}

ComPtr<IDXGIFactory4> Device::GetFactory()
{
	return m_factory.Get();
}

ComPtr<ID3D12Device2> Device::Get()
{
	return m_device;
}

/// <summary>
/// Creates a Dx12 Factory, an object that is able to query hardware/software GPUs
/// which we need to determine the optimal GPU to create our Dx12 Device with.
/// </summary>
void Device::CreateFactory()
{
	UINT factoryFlags = 0;

	ThrowIfFailed(CreateDXGIFactory2(factoryFlags, IID_PPV_ARGS(&m_factory)));
}

/// <summary>
/// Uses the earlier created Dx12 Factory to query GPU adapters.
/// It finds the "best" one and creates a Dx12 Device out of it, our entry point to the Dx12 API
/// </summary>
void Device::CreateDevice()
{
	ComPtr<IDXGIAdapter1> finalAdapter;
	ComPtr<IDXGIAdapter1> adapter1;
	SIZE_T maxVRAM = 0;

	for (UINT i = 0; DXGI_ERROR_NOT_FOUND != m_factory->EnumAdapters1(i, &adapter1); i++)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter1->GetDesc1(&desc);

		// In case the GPU is a 'software' one instead of hardware
		// skip the adapter.
		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			continue;
		}

		if (SUCCEEDED(D3D12CreateDevice(adapter1.Get(), D3D_FEATURE_LEVEL_12_0, __uuidof(ID3D12Device), nullptr)))
		{
			if (desc.DedicatedVideoMemory > maxVRAM)
			{
				maxVRAM = desc.DedicatedVideoMemory;
				finalAdapter = adapter1;
			}
		}
	}

	ThrowIfFailed(D3D12CreateDevice(finalAdapter.Get(), D3D_FEATURE_LEVEL_12_0, IID_PPV_ARGS(&m_device)));
}