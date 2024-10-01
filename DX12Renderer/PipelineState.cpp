#include "PipelineState.h"
#include <d3dcompiler.h>
#include <filesystem>
#include <cassert>

ComPtr<ID3DBlob> CompileShader(const std::wstring& filename, const D3D_SHADER_MACRO* defines, const std::string& entrypoint, const std::string& target)
{
	UINT compileFlags = 0;
#if defined(DEBUG) || defined(_DEBUG)
	compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#endif

	HRESULT hr = S_OK;

	ComPtr<ID3DBlob> byteCode = nullptr;
	ComPtr<ID3DBlob> errors;
	hr = D3DCompileFromFile(filename.c_str(), defines, D3D_COMPILE_STANDARD_FILE_INCLUDE,
		entrypoint.c_str(), target.c_str(), compileFlags, 0, &byteCode, &errors);

	if (errors != nullptr)
		OutputDebugStringA((char*)errors->GetBufferPointer());
	assert(SUCCEEDED(hr));

	return byteCode;
}

void PipelineState::SetupPipelineState(std::wstring vertexName, std::wstring pixelName, D3D12_PRIMITIVE_TOPOLOGY_TYPE type, bool useDepth)
{
	//https://gist.github.com/Jacob-Tate/7b326a086cf3f9d46e32315841101109
	wchar_t path[FILENAME_MAX] = { 0 };

	GetModuleFileNameW(nullptr, path, FILENAME_MAX);
	std::wstring m_basePath = std::filesystem::path(path).parent_path().wstring();

	std::wstring vertexPath;
	std::wstring pixelPath;
	//vertexPath = L"../Engine/Include/Subsystem/Graphics/Win32/Shader/" + vertexName + L".vertex.hlsl";
	//pixelPath = L"../Engine/Include/Subsystem/Graphics/Win32/Shader/" + pixelName + L".pixel.hlsl";
	vertexPath = m_basePath + L"\\" + vertexName + L".vertex.cso";
	pixelPath = m_basePath + L"\\" + pixelName + L".pixel.cso";

	//m_vertexShader = CompileShader(vertexPath, nullptr, "main", "vs_5_1");
	//m_pixelShader = CompileShader(pixelPath, nullptr, "main", "ps_5_1");
	// Find a way to convert string to wchar
	D3DReadFileToBlob(vertexPath.c_str(), &m_vertexShader);
	D3DReadFileToBlob(pixelPath.c_str(), &m_pixelShader);

	CreateRootSignature();
	CreatePipelineState(type, useDepth);
}