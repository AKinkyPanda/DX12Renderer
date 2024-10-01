#pragma once
#include <DirectXMath.h>

class Vertex
{
public:
	Vertex() = default;
	Vertex(const DirectX::XMFLOAT3& position, const DirectX::XMFLOAT3& normal, const DirectX::XMFLOAT2& texCoord);

	void SetPosition(const DirectX::XMFLOAT3& position);
	void SetNormal(const DirectX::XMFLOAT3& normal);
	void SetTextureCoordinated(const DirectX::XMFLOAT2& texCoord);

	DirectX::XMFLOAT3 GetPosition() const;
	DirectX::XMFLOAT3 GetNormal() const;
	DirectX::XMFLOAT2 GetTextureCoordinates() const;

	DirectX::XMFLOAT3 Position;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexCoord;
};