#include "Texture.h"
#include "d3dx12.h"
#include "Helpers.h"
#include "DXAccess.h"
#include "DescriptorHeap.h"
#include "CommandQueue.h"
#include "Application.h"


struct Texture::TexData
{
	ComPtr<ID3D12Resource> m_texture;
};

Texture::Texture(std::string path, std::vector<uint8_t> data, XMFLOAT2 imageSize)
{
	m_data = new TexData();
	m_imageSize = imageSize;
	m_path = path;

	ComPtr<ID3D12Device2> device = Application::Get().GetDevice();
	std::shared_ptr<DescriptorHeap> SRVHeap = Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	std::shared_ptr<CommandQueue> commands = Application::Get().GetCommandQueue();

	D3D12_HEAP_PROPERTIES properties = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
	D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM, imageSize.x, imageSize.y);

	ThrowIfFailed(device->CreateCommittedResource(
		&properties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&m_data->m_texture)));

	D3D12_SUBRESOURCE_DATA subresource;
	subresource.pData = data.data();
	subresource.RowPitch = imageSize.x * sizeof(uint32_t);
	subresource.SlicePitch = imageSize.x * imageSize.y * sizeof(uint32_t);

	commands->UploadData(m_data->m_texture, subresource);

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = 1; // for now...

	m_descriptorIndex = SRVHeap->GetNextIndex();
	device->CreateShaderResourceView(m_data->m_texture.Get(), &srvDesc, SRVHeap->GetCPUHandleAt(m_descriptorIndex));
	m_data->m_texture.Get()->SetName(L"Texture Resource Heap");
}

Texture::Texture(const std::vector<uint8_t>& data, XMFLOAT2 imageSize)
{
    m_data = new TexData();
    m_imageSize = imageSize;

    ComPtr<ID3D12Device2> device = Application::Get().GetDevice();
    auto SRVHeap = Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    auto commands = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);

    // Describe the texture resource
    D3D12_HEAP_PROPERTIES heapProps = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
    D3D12_RESOURCE_DESC textureDesc = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R8G8B8A8_UNORM,
        static_cast<UINT>(imageSize.x),
        static_cast<UINT>(imageSize.y)
    );

    ThrowIfFailed(device->CreateCommittedResource(
        &heapProps,
        D3D12_HEAP_FLAG_NONE,
        &textureDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(&m_data->m_texture)));

    // Fill in subresource
    D3D12_SUBRESOURCE_DATA subresource = {};
    subresource.pData = data.data();
    //subresource.RowPitch = static_cast<LONG_PTR>(imageSize.x) * sizeof(uint32_t);
    //subresource.SlicePitch = static_cast<LONG_PTR>(imageSize.x) * imageSize.y * sizeof(uint32_t);
    UINT rowPitch = static_cast<UINT>(imageSize.x) * sizeof(uint32_t);
    UINT slicePitch = rowPitch * static_cast<UINT>(imageSize.y);

    subresource.RowPitch = rowPitch;
    subresource.SlicePitch = slicePitch;

    // Upload to GPU
    commands->UploadData(m_data->m_texture, subresource);

    // SRV descriptor
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = 1;

    m_descriptorIndex = SRVHeap->GetNextIndex();
    device->CreateShaderResourceView(m_data->m_texture.Get(), &srvDesc, SRVHeap->GetCPUHandleAt(m_descriptorIndex));

    m_data->m_texture->SetName(L"Procedural Texture Resource");
}

Texture::~Texture()
{
	//delete m_data;
	//m_data = nullptr;
}

void* Texture::GetTexture() const
{
	return (void*)m_data->m_texture.Get();
}

std::string Texture::GetPath() const
{
	return m_path;
}

XMFLOAT2 Texture::GetSize() const
{
	return m_imageSize;
}