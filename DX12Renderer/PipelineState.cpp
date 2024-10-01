#include "PipelineState.h"

#include <d3dcompiler.h>
#include <filesystem>
#include <cassert>
#include <DirectXMath.h>
#include "d3dx12.h"
#include "Helpers.h"
#include "Application.h"

using namespace DirectX;



PipelineState::PipelineState(std::wstring vertexName, std::wstring pixelName, D3D12_PRIMITIVE_TOPOLOGY_TYPE type, bool useDepth)
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

ComPtr<ID3D12PipelineState> PipelineState::GetPipelineState()
{
	return m_pipelineState;
}

ComPtr<ID3D12RootSignature> PipelineState::GetRootSignature()
{
	return m_rootSignature;
}

void PipelineState::CreateRootSignature()
{
	ComPtr<ID3D12Device2> device = Application::Get().GetDevice();

	D3D12_FEATURE_DATA_ROOT_SIGNATURE featureData = {};
	featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_1;
	HRESULT result = device->CheckFeatureSupport(D3D12_FEATURE_ROOT_SIGNATURE, &featureData, sizeof(featureData));

	if (FAILED(result))
	{
		featureData.HighestVersion = D3D_ROOT_SIGNATURE_VERSION_1_0;
	}

	CD3DX12_DESCRIPTOR_RANGE1 descRange[1];
	descRange[0].Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

	CD3DX12_ROOT_PARAMETER1 rootParameter[2];
	rootParameter[0].InitAsConstants(sizeof(XMMATRIX) / 4, 0, 0, D3D12_SHADER_VISIBILITY_VERTEX); // MVP & Model
	//rootParameter[1].InitAsConstants(5, 1, 0, D3D12_SHADER_VISIBILITY_PIXEL);
	rootParameter[1].InitAsDescriptorTable(1, &descRange[0], D3D12_SHADER_VISIBILITY_PIXEL);
	//rootParameter[2].InitAsConstants(3, 2, 0, D3D12_SHADER_VISIBILITY_PIXEL);

	CD3DX12_STATIC_SAMPLER_DESC sampler(0, D3D12_FILTER_MIN_MAG_MIP_LINEAR);

	CD3DX12_VERSIONED_ROOT_SIGNATURE_DESC rootSignatureDesc;
	rootSignatureDesc.Init_1_1(_countof(rootParameter), rootParameter, 1, &sampler, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

void PipelineState::CreatePipelineState(D3D12_PRIMITIVE_TOPOLOGY_TYPE type, bool useDepth)
{
	ComPtr<ID3D12Device2> device = Application::Get().GetDevice();

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] = {
		{"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		{"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
		/*{"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}*/ };

	struct PipelineStateStream
	{
		CD3DX12_PIPELINE_STATE_STREAM_ROOT_SIGNATURE pRootSignature;
		CD3DX12_PIPELINE_STATE_STREAM_INPUT_LAYOUT InputLayout;
		CD3DX12_PIPELINE_STATE_STREAM_PRIMITIVE_TOPOLOGY PrimitiveTopologyType;
		CD3DX12_PIPELINE_STATE_STREAM_VS VS;
		CD3DX12_PIPELINE_STATE_STREAM_PS PS;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL DepthStencil;
		CD3DX12_PIPELINE_STATE_STREAM_DEPTH_STENCIL_FORMAT DSVFormat;
		CD3DX12_PIPELINE_STATE_STREAM_RENDER_TARGET_FORMATS RTVFormats;
		CD3DX12_PIPELINE_STATE_STREAM_RASTERIZER Rasterizer;
	} pipelineStateStream;

	D3D12_RT_FORMAT_ARRAY rtvFormats = {};
	rtvFormats.NumRenderTargets = 1;
	rtvFormats.RTFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;

	CD3DX12_DEPTH_STENCIL_DESC depthStencilDesc{ CD3DX12_DEFAULT() };
	depthStencilDesc.DepthEnable = useDepth;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;

	CD3DX12_RASTERIZER_DESC rasterizer{ CD3DX12_DEFAULT() };
	rasterizer.FrontCounterClockwise = TRUE;
	rasterizer.CullMode = D3D12_CULL_MODE_NONE;

	pipelineStateStream.pRootSignature = m_rootSignature.Get();
	pipelineStateStream.InputLayout = { inputElementDescs, _countof(inputElementDescs) };
	pipelineStateStream.PrimitiveTopologyType = type;
	pipelineStateStream.VS = CD3DX12_SHADER_BYTECODE(m_vertexShader.Get());
	pipelineStateStream.PS = CD3DX12_SHADER_BYTECODE(m_pixelShader.Get());
	pipelineStateStream.DepthStencil = depthStencilDesc;
	pipelineStateStream.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	pipelineStateStream.RTVFormats = rtvFormats;
	pipelineStateStream.Rasterizer = rasterizer;


	D3D12_PIPELINE_STATE_STREAM_DESC pipelineStateStreamDesc = {
		sizeof(PipelineStateStream), &pipelineStateStream
	};

	ThrowIfFailed(device->CreatePipelineState(&pipelineStateStreamDesc, IID_PPV_ARGS(&m_pipelineState)));
}