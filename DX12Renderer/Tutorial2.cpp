#include "Tutorial2.h"

#include "Application.h"
#include "CommandQueue.h"
#include "Helpers.h"
#include "Window.h"

#include <wrl.h>
using namespace Microsoft::WRL;

#include "d3dx12.h"
#include <d3dcompiler.h>

#include <algorithm> // For std::min and std::max.

#if defined(min)
#undef min
#endif
#include <iostream>
#include <stdexcept>

#if defined(max)
#undef max
#endif
#include "DXInternal.h"
#include "DXAccess.h"
#include "Mesh.h"
#include "DescriptorHeap.h"
#include "ObjLoader.h"
#include "Texture.h"
#include "../Light.h"
#include "../DirectXColors.h"
#include "DirectXTex.h"
#include "stb_image.h"


using namespace DirectX;

// Clamp a value between a min and max range.
template<typename T>
constexpr const T& clamp(const T& val, const T& min, const T& max)
{
    return val < min ? min : val > max ? max : val;
}

/*static VertexPosColor g_Vertices[8] = {
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
    { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
    { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
    { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
    { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
};

static UINT g_Indicies[36] =
{
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7
};*/

struct Mat
{
    XMMATRIX ModelMatrix;
    XMMATRIX ModelViewMatrix;
    XMMATRIX InverseTransposeModelViewMatrix;
    XMMATRIX ModelViewProjectionMatrix;
};

// Adapter for std::make_unique
class MakeUploadBuffer : public UploadBuffer
{
public:
    MakeUploadBuffer(Microsoft::WRL::ComPtr<ID3D12Device2> device, size_t pageSize = _2MB)
        : UploadBuffer(device, pageSize)
    {}

    virtual ~MakeUploadBuffer() {}
};

template<typename T>
void SetGraphics32BitConstants(uint32_t rootParameterIndex, const T& constants, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandListData);
template<typename T>
void SetGraphicsDynamicStructuredBuffer(uint32_t slot, const std::vector<T>& bufferData, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandListData, UploadBuffer* uploadBuffer);


Tutorial2::Tutorial2(const std::wstring& name, int width, int height, bool vSync)
    : super(name, width, height, vSync)
    , m_ScissorRect(CD3DX12_RECT(0, 0, LONG_MAX, LONG_MAX))
    , m_Viewport(CD3DX12_VIEWPORT(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height)))
    , m_Camera()
    , m_Forward(0)
    , m_Backward(0)
    , m_Left(0)
    , m_Right(0)
    , m_Up(0)
    , m_Down(0)
    , m_Pitch(15)
    , m_Yaw(90)
    , m_PreviousMouseX(0)
    , m_PreviousMouseY(0)
    , m_FoV(45.0)
    , m_Shift(false)
    , m_Width(width)
    , m_Height(height)
    , m_VSync(vSync)
    , m_ContentLoaded(false)
{
    //XMVECTOR cameraPos = DirectX::XMVectorSet(15, 500, -10, 1);
    //XMVECTOR cameraTarget = DirectX::XMVectorSet(35, 500, -10, 1);
    XMVECTOR cameraPos = DirectX::XMVectorSet(-200, 100, 0, 1);
    XMVECTOR cameraTarget = DirectX::XMVectorSet(0, 0, 0, 1);
    XMVECTOR cameraUp = DirectX::XMVectorSet(0, 1, 0, 1);

    m_Camera.set_LookAt(cameraPos, cameraTarget, cameraUp);

    m_pAlignedCameraData = (CameraData*)_aligned_malloc(sizeof(CameraData), 16);

    m_pAlignedCameraData->m_InitialCamPos = m_Camera.get_Translation();
    m_pAlignedCameraData->m_InitialCamRot = m_Camera.get_Rotation();
}

void Tutorial2::UpdateBufferResource(
    ComPtr<ID3D12GraphicsCommandList2> commandList,
    ID3D12Resource** pDestinationResource,
    ID3D12Resource** pIntermediateResource,
    size_t numElements, size_t elementSize, const void* bufferData,
    D3D12_RESOURCE_FLAGS flags)
{
    auto device = Application::Get().GetDevice();

    size_t bufferSize = numElements * elementSize;

    CD3DX12_HEAP_PROPERTIES heapProperties(D3D12_HEAP_TYPE_DEFAULT);
    CD3DX12_RESOURCE_DESC resourceDesc = CD3DX12_RESOURCE_DESC::Buffer(bufferSize, flags);

    HRESULT hr = device->CreateCommittedResource(
        &heapProperties,
        D3D12_HEAP_FLAG_NONE,
        &resourceDesc,
        D3D12_RESOURCE_STATE_COMMON,
        nullptr,
        IID_PPV_ARGS(pDestinationResource));

    if (FAILED(hr)) {
        std::cerr << "CreateCommittedResource failed with HRESULT: " << std::hex << hr << std::endl;
        // Break or handle the error further
    }

    // Create an committed resource for the upload.
    if (bufferData)
    {
        auto temp3 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        auto temp4 = CD3DX12_RESOURCE_DESC::Buffer(bufferSize);

        ThrowIfFailed(device->CreateCommittedResource(
            &temp3,
            D3D12_HEAP_FLAG_NONE,
            &temp4,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS(pIntermediateResource)));

        D3D12_SUBRESOURCE_DATA subresourceData = {};
        subresourceData.pData = bufferData;
        subresourceData.RowPitch = bufferSize;
        subresourceData.SlicePitch = subresourceData.RowPitch;

        UpdateSubresources(commandList.Get(),
            *pDestinationResource, *pIntermediateResource,
            0, 0, 1, &subresourceData);
    }
}


bool Tutorial2::LoadContent()
{
    auto device = Application::Get().GetDevice();
    auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto commandList = commandQueue->GetCommandList();

    Application::Get().SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, std::make_shared<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, UINT(65536), true));
    Application::Get().SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, std::make_shared<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, Application::Get().GetWindowByName(L"DX12RenderWindowClass")->GetBufferCount(), false));
    Application::Get().SetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, std::make_shared<DescriptorHeap>(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false));

    // Create the descriptor heap for the depth-stencil view.
    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    ThrowIfFailed(device->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&m_DSVHeap)));

    m_PipelineState = std::make_shared<PipelineState>(L"VertexShader", L"PixelShader", D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_SkyboxPipelineState = std::make_shared<PSOSkybox>(L"VSSkybox", L"PSSkybox", D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_ShadowMapPipelineState = std::make_shared<PSOShadowMap>(L"VSShadowMap", L"PSShadowMap", D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE);
    m_TerrainPipelineState = std::make_shared<PSOTerrain>(L"VSTerrain", L"PSTerrain", L"HSTerrain", L"DSTerrain", D3D12_PRIMITIVE_TOPOLOGY_TYPE_PATCH);

    m_meshes = LoadObjModel("D:/BUAS/Y4/DX12Renderer/Assets/Models/crytek-sponza/sponza_nobanner.obj");

    m_Monkey = LoadObjModel("../../Assets/Models/Lantern/lantern_obj.obj");
    const Texture* color = LoadTextureIndependant("../../Assets/Models/Lantern/textures/color.jpg");
    const Texture* normal = LoadTextureIndependant("../../Assets/Models/Lantern/textures/normal.jpg");
    const Texture* metallic = LoadTextureIndependant("../../Assets/Models/Lantern/textures/metallic.jpg");
    const Texture* roughness = LoadTextureIndependant("../../Assets/Models/Lantern/textures/roughness.jpg");
    const Texture* ao = LoadTextureIndependant("../../Assets/Models/Lantern/textures/ao.jpg");
    m_MonekyTextureList.emplace(std::make_pair("diffuse", const_cast<Texture*>(color)));
    m_MonekyTextureList.emplace(std::make_pair("normal", const_cast<Texture*>(normal)));
    m_MonekyTextureList.emplace(std::make_pair("metallic", const_cast<Texture*>(metallic)));
    m_MonekyTextureList.emplace(std::make_pair("roughness", const_cast<Texture*>(roughness)));
    m_MonekyTextureList.emplace(std::make_pair("ao", const_cast<Texture*>(ao)));

    m_Monkey[0].AddTextureData(m_MonekyTextureList);

    //for (int i = 0; i < m_Monkey.size(); i++)
    //{
    //    m_Monkey[i].AddTextureData(m_MonekyTextureList);
    //}

    ///////////////////////////////////////////////////////////////
    // HIGHTMAP
    ///////////////////////////////////////////////////////////////

    const Texture* Heightmap = LoadTextureIndependant("../../Assets/Textures/Heightmap.png");

    const int height = 1024;
    const int width = 1024;

    // Create a vertex buffer
    int arrSize = height * width;

    std::vector <VertexPosition> vertices = std::vector<VertexPosition>();
    vertices.resize(arrSize);
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            vertices[y * width + x].Position = XMFLOAT3((float)x, 0.0f, (float)y);
        }
    }

    // Create a vertex buffer
    float mHeightScale = (float)width / 4.0f;
    int tessFactor = 4;
    int scalePatchX = width / tessFactor;
    int scalePatchY = height / tessFactor;

    unsigned char* textureData = static_cast<unsigned char*>(Heightmap->GetTexture());

    int IMwidth, IMheight, IMchannels = 0;
    std::vector<uint8_t> buffer = ReadBinaryFile("../../Assets/Textures/Heightmap.png");
    unsigned char* data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &IMwidth, &IMheight, &IMchannels, 4);
    std::vector<uint8_t> imageData = std::vector<uint8_t>(data, data + width * height * 4);


    // create a vertex array 1/4 the size of the height map in each dimension,
    // to be stretched over the height map
    arrSize = (int)(scalePatchX * scalePatchY);
    vertices.clear();
    vertices.resize(arrSize);
    for (int y = 0; y < scalePatchY; ++y) {
        for (int x = 0; x < scalePatchX; ++x) {
            vertices[y * scalePatchX + x].Position = XMFLOAT3((float)x * tessFactor, data[(y * width * tessFactor + x * tessFactor) * 4] * mHeightScale,(float)y * tessFactor);
        }
    }

    std::vector<UINT> indices = std::vector<UINT>();

    arrSize = (scalePatchX - 1) * (scalePatchY - 1) * 4;
    indices.clear();
    indices.resize(arrSize);    
    int i = 0;
    for (int y = 0; y < scalePatchY - 1; ++y) {
        for (int x = 0; x < scalePatchX - 1; ++x) {
            UINT vert0 = x + y * scalePatchX;
            UINT vert1 = x + 1 + y * scalePatchX;
            UINT vert2 = x + (y + 1) * scalePatchX;
            UINT vert3 = x + 1 + (y + 1) * scalePatchX;
            indices[i++] = vert0;
            indices[i++] = vert1;
            indices[i++] = vert2;
            indices[i++] = vert3;
        }
    }

    std::unordered_map<std::string, Texture*> newTextures = std::unordered_map<std::string, Texture*>();

    // Terrain textures
    m_TerrainGrassTexture = LoadTextureIndependant("../../Assets/Textures/Terrain/grass.jpg");
    m_TerrainBlendTexture = LoadTextureIndependant("../../Assets/Textures/Terrain/Blend-Rock-Grass.jpg");
    m_TerrainRockTexture = LoadTextureIndependant("../../Assets/Textures/Terrain/Rock.jpg");

    newTextures.emplace(std::make_pair("Heightmap", const_cast<Texture*>(Heightmap)));

    m_Terrain = std::vector<Mesh>();
    Mesh tempTerrain = Mesh();

    tempTerrain.AddVertexData(vertices);
    tempTerrain.AddIndexData(indices);
    tempTerrain.AddTextureData(newTextures);
    tempTerrain.CreateBuffers();

    m_Terrain.push_back(tempTerrain);

    newTextures.clear();

    ///////////////////////////////////////////////////////////////
    // SKY TEXTURE
    ///////////////////////////////////////////////////////////////

    DirectX::TexMetadata* DDSData = new TexMetadata();
    DirectX::ScratchImage DDSImage;
    HRESULT hr = DirectX::LoadFromDDSFile(L"../../Assets/Textures/Skybox/SkyWater.dds", DDS_FLAGS_ALLOW_LARGE_FILES, DDSData, DDSImage);

    std::vector<uint8_t> DDSImageData;
    DDSImageData.emplace_back(*DDSImage.GetPixels());
    //const Texture* SkyTex = new Texture(std::string("../../Assets/Textures/Skybox/BrightSky.dds"), DDSImageData, XMFLOAT2(DDSImage.GetMetadata().width, DDSImage.GetMetadata().height));

    std::shared_ptr<DescriptorHeap> SRVHeap = Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    DirectX::CreateTexture(device.Get(), *DDSData, &m_SkyTexture2);

    std::vector<D3D12_SUBRESOURCE_DATA> subresources;
    hr = PrepareUpload(device.Get(), DDSImage.GetImages(), DDSImage.GetImageCount(), DDSImage.GetMetadata(),
        subresources);

    commandQueue->UploadData(m_SkyTexture2, subresources);

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
    srvDesc.Format = m_SkyTexture2->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Texture2D.MipLevels = m_SkyTexture2->GetDesc().MipLevels;

    m_SkyDescriptorIndex = SRVHeap->GetNextIndex();
    device->CreateShaderResourceView(m_SkyTexture2, &srvDesc, SRVHeap->GetCPUHandleAt(m_SkyDescriptorIndex));

    MeshData skyBox = CreateBox(1, 1, 1, 3);
    m_SkyBoxMesh.AddVertexData(skyBox.Vertices);
    m_SkyBoxMesh.AddIndexData(skyBox.Indices32);
    m_SkyBoxMesh.CreateBuffers();

    ///////////////////////////////////////////////////////////////
    // SHADOW MAP
    ///////////////////////////////////////////////////////////////

    std::shared_ptr<DescriptorHeap> ShadowSRVHeap = Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    std::shared_ptr<DescriptorHeap> ShadowDSVHeap = Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE::D3D12_DESCRIPTOR_HEAP_TYPE_DSV);

    m_ShadowMap = std::make_unique<ShadowMap>(device.Get(), 2048, 2048);

    UINT m_ShadowMapCPUSRVDescriptorIndex = ShadowSRVHeap->GetNextIndex();
    UINT m_ShadowMapGPUSRVDescriptorIndex = m_ShadowMapCPUSRVDescriptorIndex;
    UINT m_ShadowMapCPUDSVDescriptorIndex = ShadowDSVHeap->GetNextIndex();

    m_ShadowMap->BuildDescriptors(
        ShadowSRVHeap->GetCPUHandleAt(m_ShadowMapCPUSRVDescriptorIndex),
        ShadowSRVHeap->GetGPUHandleAt(m_ShadowMapGPUSRVDescriptorIndex),
        ShadowDSVHeap->GetCPUHandleAt(m_ShadowMapCPUDSVDescriptorIndex) );

    m_ContentLoaded = true;

    // Resize/Create the depth buffer.
    ResizeDepthBuffer(GetClientWidth(), GetClientHeight());

    m_UploadBuffer = std::make_unique<MakeUploadBuffer>(device, static_cast<size_t>(_2MB));

    // Wait until initialization is complete.
    commandQueue->Flush();

    ShadowSRVHeap.reset();
    ShadowSRVHeap.~shared_ptr();
    ShadowDSVHeap.reset();
    ShadowDSVHeap.~shared_ptr();

    return true;
}

void Tutorial2::ResizeDepthBuffer(int width, int height)
{
    if (m_ContentLoaded)
    {
        // Flush any GPU commands that might be referencing the depth buffer.
        Application::Get().FlushA();

        width = std::max(1, width);
        height = std::max(1, height);

        auto device = Application::Get().GetDevice();

        // Resize screen dependent resources.
        // Create a depth buffer.
        D3D12_CLEAR_VALUE optimizedClearValue = {};
        optimizedClearValue.Format = DXGI_FORMAT_D32_FLOAT;
        optimizedClearValue.DepthStencil = { 1.0f, 0 };

        auto temp1 = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT);
        auto temp2 = CD3DX12_RESOURCE_DESC::Tex2D(DXGI_FORMAT_D32_FLOAT, width, height,
            1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
        auto temp3 = optimizedClearValue;

        ThrowIfFailed(device->CreateCommittedResource(
            &temp1,
            D3D12_HEAP_FLAG_NONE,
            &temp2,
            D3D12_RESOURCE_STATE_DEPTH_WRITE,
            &optimizedClearValue,
            IID_PPV_ARGS(&m_DepthBuffer)
        ));

        // Update the depth-stencil view.
        D3D12_DEPTH_STENCIL_VIEW_DESC dsv = {};
        dsv.Format = DXGI_FORMAT_D32_FLOAT;
        dsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
        dsv.Texture2D.MipSlice = 0;
        dsv.Flags = D3D12_DSV_FLAG_NONE;

        device->CreateDepthStencilView(m_DepthBuffer.Get(), &dsv,
            m_DSVHeap->GetCPUDescriptorHandleForHeapStart());
    }
}

void Tutorial2::ComputeLightSpaceMatrix()
{
    XMFLOAT3 centerBoundingSphere = XMFLOAT3(0.0f, 0.0f, 0.0f);
    float radiusBoundingSphere = 2000;

    XMFLOAT3 lightDir = XMFLOAT3(0, -1, 1);
    XMVECTOR lightdir = XMLoadFloat3(&lightDir);
    lightdir = XMVector3Normalize(lightdir); // Normalize before scaling

    XMVECTOR lightpos = -2.0f * radiusBoundingSphere * lightdir;
    XMVECTOR targetpos = XMLoadFloat3(&centerBoundingSphere);
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX V = XMMatrixLookAtLH(lightpos, targetpos, up); // light space view matrix

    // transform bounding sphere to light space
    XMFLOAT3 spherecenterls;
    XMStoreFloat3(&spherecenterls, XMVector3TransformCoord(targetpos, V));

    // orthographic frustum
    float l = spherecenterls.x - radiusBoundingSphere;
    float b = spherecenterls.y - radiusBoundingSphere;
    float n = spherecenterls.z - radiusBoundingSphere * 2;
    float r = spherecenterls.x + radiusBoundingSphere;
    float t = spherecenterls.y + radiusBoundingSphere;
    float f = spherecenterls.z + radiusBoundingSphere * 2;
    XMMATRIX P = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

    XMMATRIX S = XMMatrixTranspose(V * P);

    m_LightViewProj = S;
}

void Tutorial2::OnResize(ResizeEventArgs& e)
{
    if (e.Width != GetClientWidth() || e.Height != GetClientWidth())
    {
        super::OnResize(e);

        m_Viewport = CD3DX12_VIEWPORT(0.0f, 0.0f,
            static_cast<float>(e.Width), static_cast<float>(e.Height));

        m_Width = std::max(1, e.Width);
        m_Height = std::max(1, e.Height);

        float aspectRatio = m_Width / (float)m_Height;
        m_Camera.set_Projection(45.0f, aspectRatio, 0.1f, 10000.0f);

        ResizeDepthBuffer(e.Width, e.Height);
    }
}

void XM_CALLCONV ComputeMatrices(FXMMATRIX model, CXMMATRIX view, CXMMATRIX viewProjection, Mat& mat)
{
    mat.ModelMatrix = XMMatrixTranspose(model);
    mat.ModelViewMatrix = model * view;
    mat.InverseTransposeModelViewMatrix = XMMatrixTranspose(XMMatrixInverse(nullptr, mat.ModelViewMatrix));
    mat.ModelViewProjectionMatrix = model * viewProjection;
}

void Tutorial2::UnloadContent()
{
    m_ContentLoaded = false;
}

void Tutorial2::OnUpdate(UpdateEventArgs& e)
{
    static uint64_t frameCount = 0;
    static double totalTime = 0.0;

    super::OnUpdate(e);

    totalTime += e.ElapsedTime;
    frameCount++;

    if (totalTime > 1.0)
    {
        double fps = frameCount / totalTime;

        char buffer[512];
        sprintf_s(buffer, "FPS: %f\n", fps);
        OutputDebugStringA(buffer);

        frameCount = 0;
        totalTime = 0.0;
    }

    // Update the camera.
    float speedMultipler = (m_Shift ? 500.0f : 100.0f);
    float deltaTime = static_cast<float>(e.ElapsedTime);

    // Compute intended movement along forward/backward and right/left.
    float moveForward = m_Forward - m_Backward;
    float moveRight = m_Right - m_Left;

    // Move the camera forwards and backwards
    XMVECTOR cameraTranslate = DirectX::XMVectorSet(moveRight, 0.0f, moveForward, 1.0f) * speedMultipler * deltaTime;
    XMVECTOR cameraPan = DirectX::XMVectorSet(0.0f, m_Up - m_Down, 0.0f, 1.0f) * speedMultipler * deltaTime;
    m_Camera.Translate(cameraTranslate, Space::Local);
    m_Camera.Translate(cameraPan, Space::Local);

    // Rotate the camera
    XMVECTOR cameraRotation =
        DirectX::XMQuaternionRotationRollPitchYaw(XMConvertToRadians(m_Pitch), XMConvertToRadians(m_Yaw), XMConvertToRadians(0.0f));
    m_Camera.set_Rotation(cameraRotation);

    XMMATRIX viewMatrix = m_Camera.get_ViewMatrix();

    const int numPointLights = 1;
    const int numSpotLights = 0;
    const int numDirectionalLights = 1;

    static const XMVECTORF32 LightColors[] = { Colors::White, Colors::Orange, Colors::Yellow, Colors::Green,
                                               Colors::Blue,  Colors::Indigo, Colors::Violet, Colors::White };

    float radius = 0;
    float offset = 0;
    float offset2 = 0;

    if (numPointLights > 0)
    {
        radius = 8.0f;
        //offset = 2.0f * XM_PI / numPointLights;
        offset2 = offset + (offset / 2.0f);
    }

    // Setup the light buffers.
    m_PointLights.resize(numPointLights);
    for (int i = 0; i < numPointLights; ++i)
    {
        PointLight& l = m_PointLights[i];

        if (i == 0) {
            //l.PositionWS = { 15.0f, 135.0f, -10.0f, 1.0f };
            l.PositionWS = { 0.0f, 0.0f, 50.0f, 1.0f };
        }
        else if (i == 1) {
            l.PositionWS = { 0.0f, 30.0f, 90.0f, 1.0f };
        } 
        else {
            l.PositionWS = { static_cast<float>(std::sin(offset * i)) * radius, 9.0f,
                 static_cast<float>(std::cos(offset * i)) * radius, 1.0f };
        }

        XMVECTOR positionWS = XMLoadFloat4(&l.PositionWS);
        XMVECTOR positionVS = XMVector3TransformCoord(positionWS, viewMatrix);
        XMStoreFloat4(&l.PositionVS, positionVS);

        l.Color = XMFLOAT4(LightColors[0]);
        l.Intensity = 5.0f;
        l.Attenuation = 0.0f;
    }

    m_SpotLights.resize(numSpotLights);
    for (int i = 0; i < numSpotLights; ++i)
    {
        SpotLight& l = m_SpotLights[i];

        l.PositionWS = { static_cast<float>(std::sin(offset * i + offset2)) * radius, 9.0f,
                         static_cast<float>(std::cos(offset * i + offset2)) * radius, 1.0f };
        XMVECTOR positionWS = XMLoadFloat4(&l.PositionWS);
        XMVECTOR positionVS = XMVector3TransformCoord(positionWS, viewMatrix);
        XMStoreFloat4(&l.PositionVS, positionVS);

        XMVECTOR directionWS = XMVector3Normalize(XMVectorSetW(XMVectorNegate(positionWS), 0));
        XMVECTOR directionVS = XMVector3Normalize(XMVector3TransformNormal(directionWS, viewMatrix));
        XMStoreFloat4(&l.DirectionWS, directionWS);
        XMStoreFloat4(&l.DirectionVS, directionVS);

        l.Color = XMFLOAT4(LightColors[0]);
        l.Intensity = 1.0f;
        l.SpotAngle = XMConvertToRadians(45.0f);
        l.Attenuation = 0.0f;
    }

    m_DirectionalLights.resize(numDirectionalLights);
    for (int i = 0; i < numDirectionalLights; ++i)
    {
        DirectionalLight& l = m_DirectionalLights[i];

        XMVECTOR temp = XMVectorSet(0, -1, 1, 0);
        XMVECTOR directionWS = XMVector3Normalize(XMVectorSetW(XMVectorNegate(temp), 0));
        XMVECTOR directionVS = XMVector3Normalize(XMVector3TransformNormal(directionWS, viewMatrix));
        XMStoreFloat4(&l.DirectionWS, directionWS);
        XMStoreFloat4(&l.DirectionVS, directionVS);

        l.Color = XMFLOAT4(LightColors[0]);
        l.Intensity = 5.0f;
    }

    ComputeLightSpaceMatrix();
}

// Transition a resource
void Tutorial2::TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
    Microsoft::WRL::ComPtr<ID3D12Resource> resource,
    D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState)
{
    CD3DX12_RESOURCE_BARRIER barrier = CD3DX12_RESOURCE_BARRIER::Transition(
        resource.Get(),
        beforeState, afterState);

    commandList->ResourceBarrier(1, &barrier);
}

// Clear a render target.
void Tutorial2::ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor)
{
    commandList->ClearRenderTargetView(rtv, clearColor, 0, nullptr);
}

void Tutorial2::ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
    D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth)
{
    commandList->ClearDepthStencilView(dsv, D3D12_CLEAR_FLAG_DEPTH, depth, 0, 0, nullptr);
}

template<typename T>
void SetGraphicsDynamicConstantBuffer(uint32_t rootParameterIndex, const T& data, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandListData, UploadBuffer* uploadBuffer)
{
    SetGraphicsDynamicConstantBufferInternal(rootParameterIndex, sizeof(T), &data, commandListData, uploadBuffer);
}

void SetGraphicsDynamicConstantBufferInternal(uint32_t rootParameterIndex, size_t sizeInBytes, const void* bufferData, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandListData, UploadBuffer* uploadBuffer)
{
    // Constant buffers must be 256-byte aligned.
    auto heapAllococation = uploadBuffer->Allocate(sizeInBytes, D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT);
    memcpy(heapAllococation.CPU, bufferData, sizeInBytes);

    commandListData->SetGraphicsRootConstantBufferView(rootParameterIndex, heapAllococation.GPU);
}

template<typename T>
void SetGraphics32BitConstants(uint32_t rootParameterIndex, const T& constants, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandListData)
{
    static_assert(sizeof(T) % sizeof(uint32_t) == 0, "Size of type must be a multiple of 4 bytes");
    commandListData->SetGraphicsRoot32BitConstants(rootParameterIndex, sizeof(T) / sizeof(uint32_t), &constants, 0);
}

template<typename T>
void SetGraphicsDynamicStructuredBuffer(uint32_t slot, const std::vector<T>& bufferData, Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandListData, UploadBuffer* uploadBuffer)
{
    size_t bufferSize = bufferData.size() * sizeof(T);

    auto heapAllocation = uploadBuffer->Allocate(bufferSize, sizeof(T));

    memcpy(heapAllocation.CPU, bufferData.data(), bufferSize);

    commandListData->SetGraphicsRootShaderResourceView(slot, heapAllocation.GPU);
}

void Tutorial2::OnRender(RenderEventArgs& e)
{
    super::OnRender(e);

    auto commandQueue = Application::Get().GetCommandQueue(D3D12_COMMAND_LIST_TYPE_DIRECT);
    auto commandList = commandQueue->GetCommandList();
    ID3D12DescriptorHeap* pDescriptorHeaps[] = { Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetDescriptorHeap().Get() };
    ID3D12DescriptorHeap* pDescriptorHeaps2[] = { Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetDescriptorHeap().Get() };

    UINT currentBackBufferIndex = m_pWindow->GetCurrentBackBufferIndex();
    auto backBuffer = m_pWindow->GetCurrentBackBuffer();
    auto rtv = m_pWindow->GetCurrentRenderTargetView();
    auto dsv = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();
    auto dsv2 = Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV)->GetDescriptorHeap().Get()->GetCPUDescriptorHandleForHeapStart();

    // Clear the render targets.
    {
        TransitionResource(commandList, backBuffer,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        //FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        FLOAT clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

        ClearRTV(commandList, rtv, clearColor);
        ClearDepth(commandList, dsv);
        //ClearDepth(commandList, dsv2);
    }

    XMMATRIX viewMatrix = m_Camera.get_ViewMatrix();
    XMMATRIX projectionMatrix = m_Camera.get_ProjectionMatrix();

    /* //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    *   SKY BOX START
    */ //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    commandList->RSSetViewports(1, &m_Viewport);
    commandList->RSSetScissorRects(1, &m_ScissorRect);

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    commandList->SetPipelineState(m_SkyboxPipelineState->GetPipelineState().Get());
    commandList->SetGraphicsRootSignature(m_SkyboxPipelineState->GetRootSignature().Get());
    commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

    commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
    commandList->IASetVertexBuffers(0, 1, static_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_SkyBoxMesh.GetVertexBuffer()));
    commandList->IASetIndexBuffer(static_cast<D3D12_INDEX_BUFFER_VIEW*>(m_SkyBoxMesh.GetIndexBuffer()));

    XMMATRIX translationMatrix = XMMatrixIdentity();
    XMMATRIX rotationMatrix = XMMatrixIdentity();
    XMMATRIX scaleMatrix = XMMatrixScaling(1, 1, 1); //XMMatrixIdentity();
    XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
    XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

    Mat matrices;
    ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);
    matrices.ModelMatrix = worldMatrix;
    matrices.ModelViewProjectionMatrix = viewProjectionMatrix;
    SetGraphicsDynamicConstantBuffer(0, matrices, commandList, m_UploadBuffer.get());

    XMVECTOR CameraPos = XMVector3Normalize(XMVector3TransformNormal(m_Camera.get_Translation(), viewMatrix));
    //XMVECTOR CameraPos = m_Camera.get_Translation();
    SetGraphics32BitConstants(1, CameraPos, commandList);

    auto descriptorIndexSky = m_SkyDescriptorIndex;
    commandList->SetGraphicsRootDescriptorTable(2, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndexSky));

    commandList->DrawIndexedInstanced(static_cast<uint32_t>(m_SkyBoxMesh.GetIndexList().size()), 1, 0, 0, 0);

    /* //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    *  
    */ //

    ///
    // 
    ///                                                             FIX translation not being tied to model but hardcoded here

       ///
    // 
    ///   
    // SHADOW MAP START
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    commandList->RSSetViewports(1, &m_ShadowMap->Viewport());
    commandList->RSSetScissorRects(1, &m_ShadowMap->ScissorRect());

    // Change to DEPTH_WRITE.
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_ShadowMap->Resource(),
        D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));

    commandList->OMSetRenderTargets(0, nullptr, false, &m_ShadowMap->Dsv());

    commandList->ClearDepthStencilView(m_ShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);

    commandList->SetPipelineState(m_ShadowMapPipelineState->GetPipelineState().Get());
    commandList->SetGraphicsRootSignature(m_ShadowMapPipelineState->GetRootSignature().Get());
    commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

    XMFLOAT4X4 lightViewProj;
    XMStoreFloat4x4(&lightViewProj, m_LightViewProj);
    SetGraphics32BitConstants(0, lightViewProj, commandList);

    // Lanter PBR
    for (int i = 0; i < m_Monkey.size(); i++)
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, static_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_Monkey[i].GetVertexBuffer()));
        commandList->IASetIndexBuffer(static_cast<D3D12_INDEX_BUFFER_VIEW*>(m_Monkey[i].GetIndexBuffer()));

        XMMATRIX translationMatrix = XMMatrixTranslation(0, 0, -5000); //XMMatrixIdentity();
        XMMATRIX rotationMatrix = XMMatrixIdentity();
        XMMATRIX scaleMatrix = XMMatrixScaling(6, 6, 6); //XMMatrixIdentity();
        XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
        XMFLOAT4X4 lightWorld;
        XMStoreFloat4x4(&lightWorld, XMMatrixTranspose(worldMatrix));
        SetGraphics32BitConstants(1, lightWorld, commandList);
        commandList->DrawIndexedInstanced(static_cast<uint32_t>(m_Monkey[i].GetIndexList().size()), 1, 0, 0, 0);
    }

    // Test Mesh
    for (int i = 0; i < m_meshes.size(); i++)
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, static_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_meshes[i].GetVertexBuffer()));
        commandList->IASetIndexBuffer(static_cast<D3D12_INDEX_BUFFER_VIEW*>(m_meshes[i].GetIndexBuffer()));

        XMMATRIX translationMatrix = XMMatrixTranslation(0, -1000, 5000); //XMMatrixIdentity();
        XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), 0, 0);//XMMatrixIdentity();
        XMMATRIX scaleMatrix = XMMatrixScaling(1, 1, 1); //XMMatrixIdentity();
        XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
        XMFLOAT4X4 lightWorld;
        XMStoreFloat4x4(&lightWorld, XMMatrixTranspose(worldMatrix));
        SetGraphics32BitConstants(1, lightWorld, commandList);
        commandList->DrawIndexedInstanced(static_cast<uint32_t>(m_meshes[i].GetIndexList().size()), 1, 0, 0, 0);
    }

    // Terrain
    for (int i = 0; i < m_Terrain.size(); i++)
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, static_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_Terrain[i].GetVertexBuffer()));
        commandList->IASetIndexBuffer(static_cast<D3D12_INDEX_BUFFER_VIEW*>(m_Terrain[i].GetIndexBuffer()));

        XMMATRIX translationMatrix = XMMatrixTranslation(0, 0, 0); //XMMatrixIdentity();
        XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(0, 0, 0);//XMMatrixIdentity();
        XMMATRIX scaleMatrix = XMMatrixScaling(1, 1, 1); //XMMatrixIdentity();
        XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
        XMFLOAT4X4 lightWorld;
        XMStoreFloat4x4(&lightWorld, XMMatrixTranspose(worldMatrix));
        SetGraphics32BitConstants(1, lightWorld, commandList);

        commandList->DrawIndexedInstanced(static_cast<uint32_t>(m_Terrain[i].GetIndexList().size()), 1, 0, 0, 0);
    }

    // Change back to GENERIC_READ so we can read the texture in a shader.
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_ShadowMap->Resource(),
        D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));

    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // SHADOW MAP END
    //////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    commandList->RSSetViewports(1, &m_Viewport);
    commandList->RSSetScissorRects(1, &m_ScissorRect);

    commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

    commandList->SetPipelineState(m_PipelineState->GetPipelineState().Get());
    commandList->SetGraphicsRootSignature(m_PipelineState->GetRootSignature().Get());
    commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

    LightProperties lightProps;
    lightProps.NumPointLights = static_cast<uint32_t>(m_PointLights.size());
    lightProps.NumSpotLights = static_cast<uint32_t>(m_SpotLights.size());
    lightProps.NumDirectionalLights = static_cast<uint32_t>(m_DirectionalLights.size());

    SetGraphics32BitConstants(1, lightProps, commandList);
    SetGraphics32BitConstants(2, CameraPos, commandList);
    SetGraphics32BitConstants(3, m_LightViewProj, commandList);

    SetGraphicsDynamicStructuredBuffer(4, m_PointLights, commandList, m_UploadBuffer.get());
    SetGraphicsDynamicStructuredBuffer(5, m_SpotLights, commandList, m_UploadBuffer.get());
    SetGraphicsDynamicStructuredBuffer(6, m_DirectionalLights, commandList, m_UploadBuffer.get());

    auto descriptorIndex = m_Monkey[0].GetTextureList()["diffuse"]->m_descriptorIndex;
    commandList->SetGraphicsRootDescriptorTable(7, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex));

    auto descriptorIndex2 = m_Monkey[0].GetTextureList()["normal"]->m_descriptorIndex;
    commandList->SetGraphicsRootDescriptorTable(8, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex2));

    auto descriptorIndex3 = m_Monkey[0].GetTextureList()["metallic"]->m_descriptorIndex;
    commandList->SetGraphicsRootDescriptorTable(9, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex3));

    auto descriptorIndex4 = m_Monkey[0].GetTextureList()["roughness"]->m_descriptorIndex;
    commandList->SetGraphicsRootDescriptorTable(10, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex4));

    auto descriptorIndex5 = m_Monkey[0].GetTextureList()["ao"]->m_descriptorIndex;
    commandList->SetGraphicsRootDescriptorTable(11, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex5));

    commandList->SetGraphicsRootDescriptorTable(13, m_ShadowMap->Srv());

    // Lanter PBR
    for (int i = 0; i < m_Monkey.size(); i++)
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, static_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_Monkey[i].GetVertexBuffer()));
        commandList->IASetIndexBuffer(static_cast<D3D12_INDEX_BUFFER_VIEW*>(m_Monkey[i].GetIndexBuffer()));

        XMMATRIX translationMatrix = XMMatrixTranslation(0, 0, - 5000); //XMMatrixIdentity();
        XMMATRIX rotationMatrix = XMMatrixIdentity();
        XMMATRIX scaleMatrix = XMMatrixScaling(6, 6, 6); //XMMatrixIdentity();
        XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
        XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;
        Mat matrices;
        ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);
        SetGraphicsDynamicConstantBuffer(0, matrices, commandList, m_UploadBuffer.get());

        commandList->DrawIndexedInstanced(static_cast<uint32_t>(m_Monkey[i].GetIndexList().size()), 1, 0, 0, 0);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Test Meshes
    for (int i = 0; i < m_meshes.size(); i++)
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, static_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_meshes[i].GetVertexBuffer()));
        commandList->IASetIndexBuffer(static_cast<D3D12_INDEX_BUFFER_VIEW*>(m_meshes[i].GetIndexBuffer()));

        // Draw sponza
        XMMATRIX translationMatrix = XMMatrixTranslation(0, -1000, 5000); //XMMatrixIdentity();
        XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(XMConvertToRadians(0), 0, 0);//XMMatrixIdentity();
        XMMATRIX scaleMatrix = XMMatrixScaling(1, 1, 1); //XMMatrixIdentity();
        XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
        XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;
        Mat matrices;
        ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);
        SetGraphicsDynamicConstantBuffer(0, matrices, commandList, m_UploadBuffer.get());

        auto descriptorIndex = m_meshes[i].GetTextureList()["diffuse"]->m_descriptorIndex;
        commandList->SetGraphicsRootDescriptorTable(7, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex));

        commandList->DrawIndexedInstanced(static_cast<uint32_t>(m_meshes[i].GetIndexList().size()), 1, 0, 0, 0);
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

        // Transition texture to the correct state for use in the pixel shader
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(static_cast<ID3D12Resource*>(m_Terrain[0].GetTextureList()["Heightmap"]->GetTexture()),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE));

    commandList->SetPipelineState(m_TerrainPipelineState->GetPipelineState().Get());
    commandList->SetGraphicsRootSignature(m_TerrainPipelineState->GetRootSignature().Get());
    commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

    // Terrain
    for (int i = 0; i < m_Terrain.size(); i++)
    {
        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_4_CONTROL_POINT_PATCHLIST);
        commandList->IASetVertexBuffers(0, 1, static_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_Terrain[i].GetVertexBuffer()));
        commandList->IASetIndexBuffer(static_cast<D3D12_INDEX_BUFFER_VIEW*>(m_Terrain[i].GetIndexBuffer()));

        XMMATRIX translationMatrix = XMMatrixTranslation(0, 0, 0); //XMMatrixIdentity();
        XMMATRIX rotationMatrix = XMMatrixRotationRollPitchYaw(0,0,0);//XMMatrixIdentity();
        XMMATRIX scaleMatrix = XMMatrixScaling(1, 1, 1); //XMMatrixIdentity();
        XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
        XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;
        Mat matrices;
        ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);
        SetGraphicsDynamicConstantBuffer(0, matrices, commandList, m_UploadBuffer.get());

        XMVECTOR heightWidth = XMVectorSet(1024, 1024, 0, 0);
        SetGraphics32BitConstants(1, heightWidth, commandList);

        auto descriptorIndexTerrain = m_Terrain[0].GetTextureList()["Heightmap"]->m_descriptorIndex;
        commandList->SetGraphicsRootDescriptorTable(2, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndexTerrain));

        SetGraphics32BitConstants(3, lightProps, commandList);
        SetGraphics32BitConstants(4, CameraPos, commandList);
        SetGraphics32BitConstants(5, m_LightViewProj, commandList);

        SetGraphicsDynamicStructuredBuffer(6, m_PointLights, commandList, m_UploadBuffer.get());
        SetGraphicsDynamicStructuredBuffer(7, m_SpotLights, commandList, m_UploadBuffer.get());
        SetGraphicsDynamicStructuredBuffer(8, m_DirectionalLights, commandList, m_UploadBuffer.get());

        commandList->SetGraphicsRootDescriptorTable(9, m_ShadowMap->Srv());

        commandList->SetGraphicsRootDescriptorTable(10, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(m_TerrainGrassTexture->m_descriptorIndex));
        commandList->SetGraphicsRootDescriptorTable(11, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(m_TerrainBlendTexture->m_descriptorIndex));
        commandList->SetGraphicsRootDescriptorTable(12, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(m_TerrainRockTexture->m_descriptorIndex));

        SetGraphics32BitConstants(13, CameraPos, commandList);

        SetGraphicsDynamicConstantBuffer(14, matrices, commandList, m_UploadBuffer.get());

        commandList->SetGraphicsRootDescriptorTable(15, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndexTerrain));

        commandList->DrawIndexedInstanced(static_cast<uint32_t>(m_Terrain[i].GetIndexList().size()), 1, 0, 0, 0);
    }

    // Transition texture to the correct state for use out of the pixel shader
    commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(static_cast<ID3D12Resource*>(m_Terrain[0].GetTextureList()["Heightmap"]->GetTexture()),
        D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE | D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE, D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE));

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Reset upload buffer so no memory leak
    m_UploadBuffer.get()->Reset();

    // Present
    {
        TransitionResource(commandList, backBuffer,
            D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);

        m_FenceValues[currentBackBufferIndex] = commandQueue->ExecuteCommandList(commandList);

        currentBackBufferIndex = m_pWindow->Present();

        commandQueue->WaitForFenceValue(m_FenceValues[currentBackBufferIndex]);
    }
}

void Tutorial2::OnKeyPressed(KeyEventArgs& e)
{
    super::OnKeyPressed(e);

    switch (e.Key)
    {
    case KeyCode::Escape:
        Application::Get().Quit(0);
        break;
    case KeyCode::Enter:
        if (e.Alt)
        {
    case KeyCode::F11:
        m_pWindow->ToggleFullscreen();
        break;
        }
    case KeyCode::V:
        m_pWindow->ToggleVSync();
        break;
    case KeyCode::R:
        // Reset camera transform
        m_Camera.set_Translation(m_pAlignedCameraData->m_InitialCamPos);
        m_Camera.set_Rotation(m_pAlignedCameraData->m_InitialCamRot);
        m_Pitch = 0.0f;
        m_Yaw = 0.0f;
        break;
    case KeyCode::Up:
    case KeyCode::W:
        m_Forward = 1.0f;
        break;
    case KeyCode::Left:
    case KeyCode::A:
        m_Left = 1.0f;
        break;
    case KeyCode::Down:
    case KeyCode::S:
        m_Backward = 1.0f;
        break;
    case KeyCode::Right:
    case KeyCode::D:
        m_Right = 1.0f;
        break;
    case KeyCode::Q:
        m_Up = 1.0f;
        break;
    case KeyCode::E:
        m_Down = 1.0f;
        break;
    case KeyCode::ShiftKey:
        m_Shift = true;
        break;
    }
}

void Tutorial2::OnKeyReleased(KeyEventArgs& e)
{
    switch (e.Key)
    {
    case KeyCode::Enter:
        if (e.Alt)
        {
    case KeyCode::F11:
        g_AllowFullscreenToggle = true;
        }
        break;
    case KeyCode::Up:
    case KeyCode::W:
        m_Forward = 0.0f;
        break;
    case KeyCode::Left:
    case KeyCode::A:
        m_Left = 0.0f;
        break;
    case KeyCode::Down:
    case KeyCode::S:
        m_Backward = 0.0f;
        break;
    case KeyCode::Right:
    case KeyCode::D:
        m_Right = 0.0f;
        break;
    case KeyCode::Q:
        m_Up = 0.0f;
        break;
    case KeyCode::E:
        m_Down = 0.0f;
        break;
    case KeyCode::ShiftKey:
        m_Shift = false;
        break;
    }
}

void Tutorial2::OnMouseMoved(MouseMotionEventArgs& e)
{
    const float mouseSpeed = 0.1f;

    e.RelX = e.X - m_PreviousMouseX;
    e.RelY = e.Y - m_PreviousMouseY;

    m_PreviousMouseX = e.X;
    m_PreviousMouseY = e.Y;

    //if (!ImGui::GetIO().WantCaptureMouse)
    {
        if (e.LeftButton)
        {
            m_Pitch -= e.RelY * mouseSpeed;

            m_Pitch = std::clamp(m_Pitch, -90.0f, 90.0f);

            m_Yaw -= e.RelX * mouseSpeed;
        }
    }
}

void Tutorial2::OnMouseWheel(MouseWheelEventArgs& e)
{
    m_FoV -= e.WheelDelta;
    m_FoV = clamp(m_FoV, 12.0f, 90.0f);

    auto fov = m_Camera.get_FoV();

    fov -= e.WheelDelta;
    fov = std::clamp(fov, 12.0f, 90.0f);

    m_Camera.set_FoV(fov);

    char buffer[256];
    sprintf_s(buffer, "FoV: %f\n", m_FoV);
    OutputDebugStringA(buffer);
}

Tutorial2::MeshData Tutorial2::CreateBox(float width, float height, float depth, UINT numSubdivisions)
{
    MeshData meshData;

    //
    // Create the vertices.
    //

    VertexPosColor v[24];

    float w2 = 0.5f * width;
    float h2 = 0.5f * height;
    float d2 = 0.5f * depth;

    // Fill in the front face vertex data.
    v[0] = VertexPosColor(-w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f);
    v[1] = VertexPosColor(-w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f);
    v[2] = VertexPosColor(+w2, +h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f);
    v[3] = VertexPosColor(+w2, -h2, -d2, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f);

    // Fill in the back face vertex data.
    v[4] = VertexPosColor(-w2, -h2, +d2, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f);
    v[5] = VertexPosColor(+w2, -h2, +d2, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f);
    v[6] = VertexPosColor(+w2, +h2, +d2, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f);
    v[7] = VertexPosColor(-w2, +h2, +d2, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

    // Fill in the top face vertex data.
    v[8] = VertexPosColor(-w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f);
    v[9] = VertexPosColor(-w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f);
    v[10] = VertexPosColor(+w2, +h2, +d2, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f);
    v[11] = VertexPosColor(+w2, +h2, -d2, 0.0f, 1.0f, 0.0f, 1.0f, 1.0f);

    // Fill in the bottom face vertex data.
    v[12] = VertexPosColor(-w2, -h2, -d2, 0.0f, -1.0f, 0.0f, 1.0f, 1.0f);
    v[13] = VertexPosColor(+w2, -h2, -d2, 0.0f, -1.0f, 0.0f, 0.0f, 1.0f);
    v[14] = VertexPosColor(+w2, -h2, +d2, 0.0f, -1.0f, 0.0f, 0.0f, 0.0f);
    v[15] = VertexPosColor(-w2, -h2, +d2, 0.0f, -1.0f, 0.0f, 1.0f, 0.0f);

    // Fill in the left face vertex data.
    v[16] = VertexPosColor(-w2, -h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    v[17] = VertexPosColor(-w2, +h2, +d2, -1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    v[18] = VertexPosColor(-w2, +h2, -d2, -1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    v[19] = VertexPosColor(-w2, -h2, -d2, -1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    // Fill in the right face vertex data.
    v[20] = VertexPosColor(+w2, -h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    v[21] = VertexPosColor(+w2, +h2, -d2, 1.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    v[22] = VertexPosColor(+w2, +h2, +d2, 1.0f, 0.0f, 0.0f, 1.0f, 0.0f);
    v[23] = VertexPosColor(+w2, -h2, +d2, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f);

    meshData.Vertices.assign(&v[0], &v[24]);

    //
    // Create the indices.
    //

    UINT i[36];

    // Fill in the front face index data
    i[0] = 0; i[1] = 1; i[2] = 2;
    i[3] = 0; i[4] = 2; i[5] = 3;

    // Fill in the back face index data
    i[6] = 4; i[7] = 5; i[8] = 6;
    i[9] = 4; i[10] = 6; i[11] = 7;

    // Fill in the top face index data
    i[12] = 8; i[13] = 9; i[14] = 10;
    i[15] = 8; i[16] = 10; i[17] = 11;

    // Fill in the bottom face index data
    i[18] = 12; i[19] = 13; i[20] = 14;
    i[21] = 12; i[22] = 14; i[23] = 15;

    // Fill in the left face index data
    i[24] = 16; i[25] = 17; i[26] = 18;
    i[27] = 16; i[28] = 18; i[29] = 19;

    // Fill in the right face index data
    i[30] = 20; i[31] = 21; i[32] = 22;
    i[33] = 20; i[34] = 22; i[35] = 23;

    meshData.Indices32.assign(&i[0], &i[36]);

    // Put a cap on the number of subdivisions.
    numSubdivisions = std::min<UINT>(numSubdivisions, 6u);

    for (UINT i = 0; i < numSubdivisions; ++i)
        Subdivide(meshData);

    return meshData;
}

void Tutorial2::Subdivide(MeshData& meshData)
{
    // Save a copy of the input geometry.
    MeshData inputCopy = meshData;


    meshData.Vertices.resize(0);
    meshData.Indices32.resize(0);

    //       v1
    //       *
    //      / \
	//     /   \
	//  m0*-----*m1
    //   / \   / \
	//  /   \ /   \
	// *-----*-----*
    // v0    m2     v2

    UINT numTris = (UINT)inputCopy.Indices32.size() / 3;
    for (UINT i = 0; i < numTris; ++i)
    {
        VertexPosColor v0 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 0]];
        VertexPosColor v1 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 1]];
        VertexPosColor v2 = inputCopy.Vertices[inputCopy.Indices32[i * 3 + 2]];

        //
        // Generate the midpoints.
        //

        VertexPosColor m0 = MidPoint(v0, v1);
        VertexPosColor m1 = MidPoint(v1, v2);
        VertexPosColor m2 = MidPoint(v0, v2);

        //
        // Add new geometry.
        //

        meshData.Vertices.push_back(v0); // 0
        meshData.Vertices.push_back(v1); // 1
        meshData.Vertices.push_back(v2); // 2
        meshData.Vertices.push_back(m0); // 3
        meshData.Vertices.push_back(m1); // 4
        meshData.Vertices.push_back(m2); // 5

        meshData.Indices32.push_back(i * 6 + 0);
        meshData.Indices32.push_back(i * 6 + 3);
        meshData.Indices32.push_back(i * 6 + 5);

        meshData.Indices32.push_back(i * 6 + 3);
        meshData.Indices32.push_back(i * 6 + 4);
        meshData.Indices32.push_back(i * 6 + 5);

        meshData.Indices32.push_back(i * 6 + 5);
        meshData.Indices32.push_back(i * 6 + 4);
        meshData.Indices32.push_back(i * 6 + 2);

        meshData.Indices32.push_back(i * 6 + 3);
        meshData.Indices32.push_back(i * 6 + 1);
        meshData.Indices32.push_back(i * 6 + 4);
    }
}

VertexPosColor Tutorial2::MidPoint(const VertexPosColor& v0, const VertexPosColor& v1)
{
    XMVECTOR p0 = XMLoadFloat3(&v0.Position);
    XMVECTOR p1 = XMLoadFloat3(&v1.Position);

    XMVECTOR n0 = XMLoadFloat3(&v0.Normal);
    XMVECTOR n1 = XMLoadFloat3(&v1.Normal);

    XMVECTOR tex0 = XMLoadFloat2(&v0.TexCoord);
    XMVECTOR tex1 = XMLoadFloat2(&v1.TexCoord);

    // Compute the midpoints of all the attributes.  Vectors need to be normalized
    // since linear interpolating can make them not unit length.  
    XMVECTOR pos = 0.5f * (p0 + p1);
    XMVECTOR normal = XMVector3Normalize(0.5f * (n0 + n1));
    XMVECTOR tex = 0.5f * (tex0 + tex1);

    VertexPosColor v;
    XMStoreFloat3(&v.Position, pos);
    XMStoreFloat3(&v.Normal, normal);
    XMStoreFloat2(&v.TexCoord, tex);

    return v;
}

/*
#pragma region DXAccess
//ComPtr<Device> GetDevice()
//{
//    return DXInternal::m_Device;
//}

std::shared_ptr<CommandQueue> GetCommandQueue()
{
    return DXInternal::m_CommandQueue;
}

std::shared_ptr<Window> GetWindow()
{
    return DXInternal::m_Window;
}

std::shared_ptr<DescriptorHeap> GetDescriptorHeap(HeapType type)
{
    switch (type)
    {
    case HeapType::CBV_SRV_UAV:
        return DXInternal::m_CBVHeap;
        break;

    case HeapType::DSV:
        return DXInternal::m_DSVHeap;
        break;

    case HeapType::RTV:
        return DXInternal::m_RTVHeap;
        break;

    default:
        return DXInternal::m_CBVHeap;
        break;
    }
}

Microsoft::WRL::ComPtr<ID3D12Device2> GetDeviceTemp()
{
    return Application::Get().GetDevice();
}

std::shared_ptr<CommandQueue> GetCommandQueueTemp()
{
    return Application::Get().GetCommandQueue();
}

std::shared_ptr<ID3D12DescriptorHeap> GetDescriptorHeapTemp(HeapType type)
{
    //Nothing
    return DXInternal::m_BaseHeap;
}
#pragma endregion*/