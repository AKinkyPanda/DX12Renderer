#include "PSOShadowMap.h"
#include <d3dcompiler.h>
#include <filesystem>
#include <cassert>
#include <DirectXMath.h>
#include "DX12Renderer/d3dx12.h"
#include "DX12Renderer/Helpers.h"
#include "DX12Renderer/Application.h"
#include "DX12Renderer/Tutorial2.h"
#include "DX12Renderer/Vertex.h"

using namespace DirectX;



PSOShadowMap::PSOShadowMap(std::wstring vertexName, std::wstring pixelName, D3D12_PRIMITIVE_TOPOLOGY_TYPE type, bool useDepth)
{
	//https://gist.github.com/Jacob-Tate/7b326a086cf3f9d46e32315841101109
	wchar_t path[FILENAME_MAX] = { 0 };

	GetModuleFileNameW(nullptr, path, FILENAME_MAX);
	std::wstring m_basePath = std::filesystem::path(path).parent_path().wstring();

	std::wstring vertexPath;
	std::wstring pixelPath;
	vertexPath = m_basePath + L"\\" + vertexName + L".cso";
	pixelPath = m_basePath + L"\\" + pixelName + L".cso";

	// Find a way to convert string to wchar
	D3DReadFileToBlob(vertexPath.c_str(), &m_vertexShader);
	D3DReadFileToBlob(pixelPath.c_str(), &m_pixelShader);

	CreateRootSignature();
	CreatePipelineState(type, useDepth);
}

ComPtr<ID3D12PipelineState> PSOShadowMap::GetPipelineState()
{
	return m_pipelineState;
}

ComPtr<ID3D12RootSignature> PSOShadowMap::GetRootSignature()
{
	return m_rootSignature;
}

void PSOShadowMap::CreateRootSignature()
{
	ComPtr<ID3D12Device2> device = Application::Get().GetDevice();

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	HRESULT result = device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData));

	if (FAILED(result))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	// Create a root signature.
// Allow input layout and deny unnecessary access to certain pipeline stages.
	D3D12_ROOT_SIGNATURE_FLAGS rootSignatureFlags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_HULL_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_DOMAIN_SHADER_ROOT_ACCESS |
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;


	CD3DX12_ROOT_PARAMETER1 rootParameter[1];
	rootParameter[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // Light View and Proj

	//CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameter), rootParameter, 0, nullptr, rootSignatureFlags);

	ComPtr<ID3DBlob> signatureBlob;
	ComPtr<ID3DBlob> errorBlob;

	ThrowIfFailed(D3DX12SerializeVersionedRootSignature(&rootSignatureDesc, featureData.HighestVersion, &signatureBlob, &errorBlob));
	ThrowIfFailed(device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&m_rootSignature)));

	if (errorBlob)
	{
		const char* errStr = (const char*)errorBlob->GetBufferPointer();
		printf("%s", errStr);
	}
}

void PSOShadowMap::CreatePipelineState(D3D12_PRIMITIVE_TOPOLOGY_TYPE type, bool useDepth)
{
	ComPtr<ID3D12Device2> device = Application::Get().GetDevice();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0} };

	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
	} pipelineStateStream;

	// Should be 0 and UNKNOWN for shadows. Consider changing the pixel shader to void not float4
	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 0;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_UNKNOWN;

	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc{ CD3DX12_DEFAULT() };
	depthStencilDesc.DepthEnable = useDepth;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	CD3DX12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	blendDesc.RenderTarget[0].BlendEnable = TRUE;
	blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
	blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// rasterizer for shadow maps
	CD3DX12_RASTERIZER_DESC rasterizer{ CD3DX12_DEFAULT() };
	rasterizer.FrontCounterClockwise = TRUE;
	rasterizer.CullMode = D3D12_CULL_MODE_NONE;
	rasterizer.DepthBias = 100000;
	rasterizer.DepthBiasClamp = 0.0f;
	rasterizer.SlopeScaledDepthBias = 1.0f;

	pipelineStateStream.pRootSignature = m_rootSignature.Get();
	pipelineStateStream.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	pipelineStateStream.PrimitiveTopologyType = type;
	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(m_vertexShader.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(m_pixelShader.Get());
	pipelineStateStream.DepthStencil = depthStencilDesc;
	pipelineStateStream.Blend = blendDesc;
	pipelineStateStream.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineStateStream.RTVFormats = rtvFormats;
	pipelineStateStream.Rasterizer = rasterizer;


	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState)));
}