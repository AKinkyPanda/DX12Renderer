#pragma once
#include <DirectXMath.h>

#include <d3d12.h>
#include <DirectXMath.h>

using namespace DirectX;

class Vertex
{
public:
	Vertex() = default;
	Vertex(const XMFLOAT3& position, const XMFLOAT3& normal, const XMFLOAT2& texCoord);

	void SetPosition(const XMFLOAT3& position);
	void SetNormal(const XMFLOAT3& normal);
	void SetTextureCoordinated(const XMFLOAT2& texCoord);

	XMFLOAT3 GetPosition() const;
	XMFLOAT3 GetNormal() const;
	XMFLOAT2 GetTextureCoordinates() const;

	XMFLOAT3 Position;
	XMFLOAT3 Normal;
	XMFLOAT2 TexCoord;
};

struct VertexPosition
{
    VertexPosition() = default;

    explicit VertexPosition(const XMFLOAT3& position)
        : Position(position)
    {}

    explicit VertexPosition(FXMVECTOR position)
    {
        XMStoreFloat3(&(this->Position), position);
    }

    XMFLOAT3 Position;

    static const D3D12_INPUT_LAYOUT_DESC InputLayout;
private:
    static const int                      InputElementCount = 1;
    static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};

struct VertexPositionNormalTangentBitangentTexture
{
    VertexPositionNormalTangentBitangentTexture() = default;

    explicit VertexPositionNormalTangentBitangentTexture(const XMFLOAT3& position,
        const XMFLOAT3& normal,
        const XMFLOAT3& texCoord,
        const XMFLOAT3& tangent = { 0, 0, 0 },
        const XMFLOAT3& bitangent = { 0, 0, 0 })
        : Position(position)
        , Normal(normal)
        , Tangent(tangent)
        , Bitangent(bitangent)
        , TexCoord(texCoord)
    {}

    explicit VertexPositionNormalTangentBitangentTexture(FXMVECTOR position, FXMVECTOR normal,
        FXMVECTOR texCoord,
        GXMVECTOR tangent = { 0, 0, 0, 0 },
        HXMVECTOR bitangent = { 0, 0, 0, 0 })
    {
        XMStoreFloat3(&(this->Position), position);
        XMStoreFloat3(&(this->Normal), normal);
        XMStoreFloat3(&(this->Tangent), tangent);
        XMStoreFloat3(&(this->Bitangent), bitangent);
        XMStoreFloat3(&(this->TexCoord), texCoord);
    }

    XMFLOAT3 Position;
    XMFLOAT3 Normal;
    XMFLOAT3 Tangent;
    XMFLOAT3 Bitangent;
    XMFLOAT3 TexCoord;

    static const D3D12_INPUT_LAYOUT_DESC InputLayout;
private:
    static const int                      InputElementCount = 5;
    static const D3D12_INPUT_ELEMENT_DESC InputElements[InputElementCount];
};