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

DirectX::XMFLOAT3 Vertex::GetPosition() const
{
	return Position;
}

DirectX::XMFLOAT3 Vertex::GetNormal() const
{
	return Normal;
}

DirectX::XMFLOAT2 Vertex::GetTextureCoordinates() const
{
	return TexCoord;
}