#pragma once
#include "Material.h"
#include "Vertex.h"

#include <DirectXMath.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <d3d12.h>
#include <memory>
#include <wrl.h>

using namespace DirectX;

class Texture;

struct VertexPosColor
{
	void SetPosition(const DirectX::XMFLOAT3& position)
	{
		Position = position;
	}
	void SetNormal(const DirectX::XMFLOAT3& normal)
	{
		Color = normal;
	}
	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Color;
};

struct BufferData
{
	Microsoft::WRL::ComPtr<ID3D12Resource> m_bufferResource;
	D3D12_VERTEX_BUFFER_VIEW m_vertexView{};
	D3D12_INDEX_BUFFER_VIEW m_indexView{};
};

class Mesh
{
public:
	void CreateBuffers();

	void AddVertexData(const std::vector<VertexPosColor>& vertexList);
	void AddIndexData(const std::vector<WORD>& indexList);
	void AddTextureData(const std::unordered_map<std::string, Texture*>& textureList);
	void AddMaterial(const Material& material);

	std::vector<VertexPosColor>& GetVertexList() { return m_vertexList; }
	std::vector<WORD>& GetIndexList() { return m_indexList; }
	std::unordered_map<std::string, Texture*>& GetTextureList() { return m_textureList; }
	std::vector<VertexPosColor> GetVertexList() const { return m_vertexList; }
	std::vector<WORD> GetIndexList() const { return m_indexList; }
	std::unordered_map<std::string, Texture*> GetTextureList() const { return m_textureList; }
	Material& GetMaterial() { return m_material; }
	void* GetVertexBuffer();
	void* GetIndexBuffer();
	void Shutdown();
private:
	void CreateVertexBuffer();
	void CreateIndexBuffer();
	void CreateTexturesBuffer();

	std::vector<VertexPosColor> m_vertexList;
	std::vector<WORD> m_indexList;
	std::unordered_map<std::string, Texture*> m_textureList;

	Material m_material;

	std::shared_ptr<BufferData> m_vertexBuffer = nullptr;
	std::shared_ptr<BufferData> m_indexBuffer = nullptr;
};

