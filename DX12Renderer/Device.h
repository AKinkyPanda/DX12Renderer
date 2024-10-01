#pragma once

#include <wrl.h>
using namespace Microsoft::WRL;

#include <d3d12.h>
#include <dxgi1_6.h>


/// <summary>
/// Encapsulates the initialization of a DirectX Device.
/// Gives access to to this component through getters.
/// </summary>
class Device
{
public:
	Device();

	/// <summary>
	/// Returns a pointer to the DX Device
	/// </summary>
	/// <returns></returns>
	ComPtr<ID3D12Device2> Get();

	/// <summary>
	/// Returns a pointer to the DX Factory
	/// </summary>
	/// <returns></returns>
	ComPtr<IDXGIFactory4> GetFactory();

private:
	void CreateFactory();
	void CreateDevice();

	ComPtr<IDXGIFactory4> m_factory;
	ComPtr<ID3D12Device2> m_device;
};