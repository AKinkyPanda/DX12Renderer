#include "Vertex.h"

Vertex::Vertex(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT2& texCoord)
{
	Position = position;

	Normal = normal;

	TexCoord = texCoord;
}

void Vertex::SetPosition(const DirectX::XMFLOAT3& position)
{
	Position = position;
}

void Vertex::SetNormal(const DirectX::XMFLOAT3& normal)
{
	Normal = normal;
}

void Vertex::SetTextureCoordinated(const DirectX::XMFLOAT2& texCoord)
{
	TexCoord = texCoord;
}

XMFLOAT3 Vertex::GetPosition() const
{
	return Position;
}

XMFLOAT3 Vertex::GetNormal() const
{
	return Normal;
}

XMFLOAT2 Vertex::GetTextureCoordinates() const
{
	return TexCoord;
}

// clang-format off
const D3D12_INPUT_ELEMENT_DESC VertexPosition::InputElements[] = {
    { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
};

const D3D12_INPUT_LAYOUT_DESC VertexPosition::InputLayout = {
    VertexPosition::InputElements,
    VertexPosition::InputElementCount
};

const D3D12_INPUT_ELEMENT_DESC VertexPositionNormalTangentBitangentTexture::InputElements[] = {
    { "POSITION",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "NORMAL",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TANGENT",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "BITANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    { "TEXCOORD",  0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
};

const D3D12_INPUT_LAYOUT_DESC VertexPositionNormalTangentBitangentTexture::InputLayout = {
    VertexPositionNormalTangentBitangentTexture::InputElements,
    VertexPositionNormalTangentBitangentTexture::InputElementCount
};
// clang-format on