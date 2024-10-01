#include "Mesh.h"

#include "Vertex.h"
#include "d3dx12.h"
#include "Helpers.h"
#include "Application.h"

using namespace DirectX;

const std::vector<uint8_t> blackTexture
{
	0,0,0,255
};

void Mesh::AddVertexData(const std::vector<VertexPosColor>& vertexList)
{
	m_vertexList = vertexList;
}

void Mesh::AddIndexData(const std::vector<WORD>& indexList)
{
	m_indexList = indexList;
}

void Mesh::AddTextureData(const std::unordered_map<std::string, Texture*>& textureList)
{
	m_textureList = textureList;
}

void Mesh::AddMaterial(const Material& material)
{
	m_material = material;
}

void* Mesh::GetVertexBuffer()
{
	return &m_vertexBuffer->m_vertexView;
}

void* Mesh::GetIndexBuffer()
{
	return &m_vertexBuffer->m_indexView;
}

void Mesh::CreateBuffers()
{
	CreateVertexBuffer();
	CreateIndexBuffer();
	CreateTexturesBuffer();
}

void Mesh::CreateVertexBuffer()
{
	m_vertexBuffer = std::make_shared<BufferData>();

	const UINT bufferSize = static_cast<UINT>(m_vertexList.size() * sizeof(VertexPosColor));

	D3D12_HEAP_PROPERTIES properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);

	ThrowIfFailed(Application::Get().GetDevice()->CreateCommittedResource(
		&properties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_vertexBuffer->m_bufferResource)));

	UINT8* vertexData = nullptr;
	D3D12_RANGE range{ 0, 0 };

	ThrowIfFailed(m_vertexBuffer->m_bufferResource->Map(0, &range, reinterpret_cast<void**>(&vertexData)));
	memcpy(vertexData, &m_vertexList[0], bufferSize);
	m_vertexBuffer->m_bufferResource->Unmap(0, nullptr);

	m_vertexBuffer->m_vertexView.BufferLocation = m_vertexBuffer->m_bufferResource->GetGPUVirtualAddress();
	m_vertexBuffer->m_vertexView.StrideInBytes = sizeof(VertexPosColor);
	m_vertexBuffer->m_vertexView.SizeInBytes = bufferSize;
}

void Mesh::CreateIndexBuffer()
{
	m_indexBuffer = std::make_shared<BufferData>();

	const UINT bufferSize = static_cast<UINT>(m_indexList.size() * sizeof(WORD));

	D3D12_HEAP_PROPERTIES properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	D3D12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, D3D12_RESOURCE_FLAG_NONE);

	ThrowIfFailed(Application::Get().GetDevice()->CreateCommittedResource(
		&properties,
		D3D12_HEAP_FLAG_NONE,
		&resourceDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&m_indexBuffer->m_bufferResource)));

	UINT8* indexData = nullptr;
	D3D12_RANGE range{ 0, 0 };

	ThrowIfFailed(m_indexBuffer->m_bufferResource->Map(0, &range, reinterpret_cast<void**>(&indexData)));
	memcpy(indexData, &m_indexList[0], bufferSize);
	m_indexBuffer->m_bufferResource->Unmap(0, nullptr);

	m_vertexBuffer->m_indexView.BufferLocation = m_indexBuffer->m_bufferResource->GetGPUVirtualAddress();
	m_vertexBuffer->m_indexView.Format = DXGI_FORMAT_R16_UINT;
	m_vertexBuffer->m_indexView.SizeInBytes = bufferSize;
}

void Mesh::CreateTexturesBuffer()
{

}

void Mesh::Shutdown()
{
	m_vertexBuffer->m_bufferResource.~ComPtr();
	m_indexBuffer->m_bufferResource.~ComPtr();

	m_indexBuffer.~shared_ptr();
	m_indexBuffer = nullptr;
	m_vertexBuffer.~shared_ptr();
	m_vertexBuffer = nullptr;
}
