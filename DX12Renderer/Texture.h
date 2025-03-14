#pragma once
#include <string>
#include <vector>
#include <DirectXMath.h>
#include <d3d12.h>
#include <wrl.h>

using namespace DirectX;
using namespace Microsoft::WRL;

class Texture
{
public:
	Texture() = default;
	Texture(std::string path, std::vector<uint8_t> data, XMFLOAT2 imageSize);
	~Texture();
	void* GetTexture() const;
	std::string GetPath() const;
	XMFLOAT2 GetSize() const;

	uint32_t m_descriptorIndex;
private:
	struct TexData;
	ComPtr<ID3D12Resource> m_texture;
	std::string m_path;
	XMFLOAT2 m_imageSize;
	TexData* m_data;
};