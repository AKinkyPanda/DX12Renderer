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

void Mesh::AddVertexData(const std::vector<VertexPosition>& vertexListPosition)
{
	m_vertexListPosition = vertexListPosition;
}

void Mesh::AddIndexData(const std::vector<UINT>& indexList)
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

	UINT bufferSize = 0;

	if (!m_vertexList.empty()) {
		bufferSize = static_cast<UINT>(m_vertexList.size() * sizeof(VertexPosColor));
	}
	else {
		bufferSize = static_cast<UINT>(m_vertexListPosition.size() * sizeof(VertexPosition));
	}

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
	if (!m_vertexList.empty()) {
		memcpy(vertexData, &m_vertexList[0], bufferSize);
	}
	else {
		memcpy(vertexData, &m_vertexListPosition[0], bufferSize);
	}
	m_vertexBuffer->m_bufferResource->Unmap(0, nullptr);

	m_vertexBuffer->m_vertexView.BufferLocation = m_vertexBuffer->m_bufferResource->GetGPUVirtualAddress();
	if (!m_vertexList.empty()) {
		m_vertexBuffer->m_vertexView.StrideInBytes = sizeof(VertexPosColor);
	}
	else {
		m_vertexBuffer->m_vertexView.StrideInBytes = sizeof(VertexPosition);
	}
	m_vertexBuffer->m_vertexView.SizeInBytes = bufferSize;
}

void Mesh::CreateIndexBuffer()
{
	m_indexBuffer = std::make_shared<BufferData>();

	const UINT bufferSize = static_cast<UINT>(m_indexList.size() * sizeof(UINT));

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
	m_vertexBuffer->m_indexView.Format = DXGI_FORMAT_R32_UINT;
	m_vertexBuffer->m_indexView.SizeInBytes = bufferSize;
}

void Mesh::CreateTexturesBuffer()
{

}

void Mesh::Draw(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList, D3D_PRIMITIVE_TOPOLOGY topologyType)
{
	if (!m_vertexBuffer || !m_indexBuffer)
		return;

	commandList->IASetVertexBuffers(0, 1, &m_vertexBuffer->m_vertexView);
	commandList->IASetIndexBuffer(&m_vertexBuffer->m_indexView);
	commandList->IASetPrimitiveTopology(topologyType);

	commandList->DrawIndexedInstanced(static_cast<UINT>(m_indexList.size()), 1, 0, 0, 0);
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
