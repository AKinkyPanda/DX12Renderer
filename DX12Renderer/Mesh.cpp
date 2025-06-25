#include "Mesh.h"

#include "Vertex.h"
#include "d3dx12.h"
#include "Helpers.h"
#include "Application.h"
#include "Texture.h"

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
	// 1) Ensure GPU is idle or not using these resources (done by caller/outer code via fence)

	// 2) Release GPU resources by resetting smart pointers:
	if (m_vertexBuffer)
	{
		m_vertexBuffer->m_bufferResource.Reset(); // releases ID3D12Resource
		// Optionally reset views, though not strictly necessary:
		m_vertexBuffer->m_vertexView = {};
	}
	if (m_indexBuffer)
	{
		m_indexBuffer->m_bufferResource.Reset();
		m_indexBuffer->m_indexView = {};
	}

	m_vertexBuffer.reset(); // release shared_ptr<BufferData>
	m_indexBuffer.reset();

	// 3) Clear CPU-side data if you want:
	m_vertexList.clear();
	m_vertexList.shrink_to_fit();
	m_vertexListPosition.clear();
	m_vertexListPosition.shrink_to_fit();
	m_indexList.clear();
	m_indexList.shrink_to_fit();

	// 4) If you own Texture* in m_textureList, ensure those Texture objects are also released elsewhere.
	//    Typically you might not delete them here if they're shared; but if Mesh "owns" them:
	//    for (auto& kv : m_textureList) { delete kv.second; }
	//    Or call a Shutdown() on each Texture object so they release their own GPU resources.
 
	//for (auto& kv : m_textureList)
	//{
	//	Texture* tex = kv.second;
	//	if (tex)
	//	{
	//		tex->Shutdown();
	//		//delete tex; // if Mesh owns the Texture
	//	}
	//}
	m_textureList.clear();

	// 5) Material: if it holds GPU resources, ensure it has its own Shutdown or destructor handling.
}
