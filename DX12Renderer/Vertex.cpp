#include "Vertex.h"

Vertex::Vertex(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT2& texCoord)
{
	m_position[0] = position.x;
	m_position[1] = position.y;
	m_position[2] = position.z;

	m_normal[0] = normal.x;
	m_normal[1] = normal.y;
	m_normal[2] = normal.z;

	//m_texCoord[0] = texCoord.x;
	//m_texCoord[1] = texCoord.y;
}

void Vertex::SetPosition(const DirectX::XMFLOAT3& position)
{
	m_position[0] = position.x;
	m_position[1] = position.y;
	m_position[2] = position.z;
}

void Vertex::SetNormal(const DirectX::XMFLOAT3& normal)
{
	m_normal[0] = normal.x;
	m_normal[1] = normal.y;
	m_normal[2] = normal.z;
}

//void Vertex::SetTextureCoordinated(const DirectX::XMFLOAT2& texCoord)
//{
//	m_texCoord[0] = texCoord.x;
//	m_texCoord[1] = texCoord.y;
//}

DirectX::XMFLOAT3 Vertex::GetPosition() const
{
	return DirectX::XMFLOAT3(m_position[0], m_position[1], m_position[2]);
}

DirectX::XMFLOAT3 Vertex::GetNormal() const
{
	return DirectX::XMFLOAT3(m_normal[0], m_normal[1], m_normal[2]);
}

//DirectX::XMFLOAT2 Vertex::GetTextureCoordinates() const
//{
//	return DirectX::XMFLOAT2(m_texCoord[0], m_texCoord[1]);
//}