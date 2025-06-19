#include "PSOTerrainShadowMap.h"

#include <d3dcompiler.h>
#include <filesystem>
#include <cassert>
#include <DirectXMath.h>
#include <array>

#include "DX12Renderer/d3dx12.h"
#include "DX12Renderer/Helpers.h"
#include "DX12Renderer/Application.h"
#include "DX12Renderer/Tutorial2.h"
#include "DX12Renderer/Vertex.h"

using namespace DirectX;



PSOTerrainShadowMap::PSOTerrainShadowMap(std::wstring vertexName, std::wstring hullName, std::wstring domainName, D3D12_PRIMITIVE_TOPOLOGY_TYPE type, bool useDepth)
{
	//https://gist.github.com/Jacob-Tate/7b326a086cf3f9d46e32315841101109
	wchar_t path[FILENAME_MAX] = { 0 };

	GetModuleFileNameW(nullptr, path, FILENAME_MAX);
	std::wstring m_basePath = std::filesystem::path(path).parent_path().wstring();

	std::wstring vertexPath;
	std::wstring hullPath;
	std::wstring domainPath;
	vertexPath = m_basePath + L"\\" + vertexName + L".cso";
	hullPath = m_basePath + L"\\" + hullName + L".cso";
	domainPath = m_basePath + L"\\" + domainName + L".cso";

	// Find a way to convert string to wchar
	D3DReadFileToBlob(vertexPath.c_str(), &m_vertexShader);
	D3DReadFileToBlob(hullPath.c_str(), &m_hullShader);
	D3DReadFileToBlob(domainPath.c_str(), &m_domainShader);

	CreateRootSignature();
	CreatePipelineState(type, useDepth);
}

ComPtr<ID3D12PipelineState> PSOTerrainShadowMap::GetPipelineState()
{
	return m_pipelineState;
}

ComPtr<ID3D12RootSignature> PSOTerrainShadowMap::GetRootSignature()
{
	return m_rootSignature;
}

void PSOTerrainShadowMap::CreateRootSignature()
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
		D3D12_ROOT_SIGNATURE_FLAG_DENY_GEOMETRY_SHADER_ROOT_ACCESS;

	CD3DX12_DESCRIPTOR_RANGE1 descRange[5];
	descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // Heightmap
	descRange[1].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 4); // Shadow Map Texture
	descRange[2].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 5); // Grass Texture
	descRange[3].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 6); // Blend Texture
	descRange[4].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 7); // Rock Texture

	CD3DX12_DESCRIPTOR_RANGE1 descRangeDomain[1];
	descRangeDomain[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 2); // Heightmap for domain shader

	CD3DX12_ROOT_PARAMETER1 rootParameter[8];
	// Vertex Shader
	rootParameter[0].InitAsConstantBufferView(0, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_VERTEX); // MVP & Model
	rootParameter[1].InitAsConstants(sizeof(XMVECTOR) / 4, 1, 0, D3D12_SHADER_VISIBILITY_VERTEX); // Terrain data
	rootParameter[2].InitAsConstants(sizeof(XMFLOAT4) / 4, 2, 0, D3D12_SHADER_VISIBILITY_VERTEX); // chunk data

	// All Shader
	rootParameter[3].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_ALL); // Heightmap for all shaders

	// Hull Shader
	rootParameter[4].InitAsConstants(sizeof(XMVECTOR) / 4, 3, 0, D3D12_SHADER_VISIBILITY_HULL);

	// Domain Shader
	rootParameter[5].InitAsConstantBufferView(2, 0, D3D12_ROOT_DESCRIPTOR_FLAG_NONE, D3D12_SHADER_VISIBILITY_DOMAIN); // Matrix
	rootParameter[6].InitAsDescriptorTable(1, &descRangeDomain[0], D3D12_SHADER_VISIBILITY_DOMAIN); // Heightmap
	rootParameter[7].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_DOMAIN); // Light View and Proj

	CD3DX12_STATIC_SAMPLER_DESC heightmapSampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR, 
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

	CD3DX12_STATIC_SAMPLER_DESC colorSampler(1, D3D12_FILTER_MIN_MAG_MIP_LINEAR, 
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,
		D3D12_TEXTURE_ADDRESS_MODE_WRAP);

	CD3DX12_STATIC_SAMPLER_DESC shadowSampler(
		2, // shaderRegister
		D3D12_FILTER_COMPARISON_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,  // addressW
		0.0f,                               // mipLODBias
		16,                                 // maxAnisotropy
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK);


	std::array<CD3DX12_STATIC_SAMPLER_DESC, 3> staticSamplers = { heightmapSampler, colorSampler, shadowSampler };

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameter), rootParameter, staticSamplers.size(), staticSamplers.data(), rootSignatureFlags);


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

void PSOTerrainShadowMap::CreatePipelineState(D3D12_PRIMITIVE_TOPOLOGY_TYPE type, bool useDepth)
{
	ComPtr<ID3D12Device2> device = Application::Get().GetDevice();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "POSITION", 1, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_HS HS;
		CD3DX12_PIPELINE_STATE_STREAM_DS DS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
		CD3DX12_PIPELINE_STATE_STREAM_BLEND_DESC Blend;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_DESC SampleDesc;
		CD3DX12_PIPELINE_STATE_STREAM_SAMPLE_MASK SampleMask;
	} pipelineStateStream;

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

	CD3DX12_RASTERIZER_DESC rasterizer{ CD3DX12_DEFAULT() };
	rasterizer.FrontCounterClockwise = TRUE;
	rasterizer.CullMode = D3D12_CULL_MODE_BACK;
	rasterizer.FillMode = D3D12_FILL_MODE_SOLID;
	rasterizer.DepthBias = 1500.0f;
	rasterizer.DepthBiasClamp = 0.0f;
	rasterizer.SlopeScaledDepthBias = 2.5f;

	//DXGI_SAMPLE_DESC sampler{};
	//sampler.Count = 1;
	//sampler.Quality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;

	DXGI_SAMPLE_DESC descSample = {};
	descSample.Count = 1; // turns multi-sampling off. Not supported feature for my card.


	pipelineStateStream.pRootSignature = m_rootSignature.Get();
	pipelineStateStream.InputLayout = VertexPosition::InputLayout;
	//pipelineStateStream.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	pipelineStateStream.PrimitiveTopologyType = type;
	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(m_vertexShader.Get());
	pipelineStateStream.HS = CD3DX12_SHADER_BYTECODE(m_hullShader.Get());
	pipelineStateStream.DS = CD3DX12_SHADER_BYTECODE(m_domainShader.Get());
	pipelineStateStream.DepthStencil = depthStencilDesc;
	pipelineStateStream.Blend = blendDesc;
	pipelineStateStream.DSVFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;
	pipelineStateStream.RTVFormats = rtvFormats;
	pipelineStateStream.Rasterizer = rasterizer;
	pipelineStateStream.SampleDesc = descSample;
	pipelineStateStream.SampleMask = UINT_MAX;


	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState)));
}