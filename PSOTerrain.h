#pragma once

#include <string>
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 Specific headers
#include <d3d12.h>

class Device;

/// <summary>
/// Encapsulates the initialization of a DirectX Root signature and 
/// DX PipelineState object. Gives access to these components through getters.
/// </summary>
class PSOTerrain
{
public:
	PSOTerrain(std::wstring vertexName, std::wstring pixelName, std::wstring hullName, std::wstring domainName, D3D12_PRIMITIVE_TOPOLOGY_TYPE type, bool useDepth = true);

	/// <summary>
	/// Returns a pointer to the pipeline state object
	/// </summary>
	/// <returns></returns>
	ComPtr<ID3D12PipelineState> GetPipelineState();

	/// <summary>
	/// Returns a pointer to the root signature object
	/// </summary>
	/// <returns></returns>
	ComPtr<ID3D12RootSignature> GetRootSignature();

private:
	// For now both of these are hardcoded...
	// but since we don't need anything else that's fine
	void CreateRootSignature();
	void CreatePipelineState(D3D12_PRIMITIVE_TOPOLOGY_TYPE type, bool useDepth);

	ComPtr<ID3D12RootSignature> m_rootSignature;
	ComPtr<ID3D12PipelineState> m_pipelineState;

	ComPtr<ID3DBlob> m_vertexShader;
	ComPtr<ID3DBlob> m_pixelShader;
	ComPtr<ID3DBlob> m_hullShader;
	ComPtr<ID3DBlob> m_domainShader;
};

