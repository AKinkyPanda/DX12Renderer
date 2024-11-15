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

using namespace DirectX;

// Clamp a value between a min and max range.
template<typename T>
constexpr const T& clamp(const T& val, const T& min, const T& max)
{
    return val < min ? min : val > max ? max : val;
}

static VertexPosColor g_Vertices[8] = {
    { XMFLOAT3(-1.0f, -1.0f, -1.0f), XMFLOAT3(0.0f, 0.0f, 0.0f) }, // 0
    { XMFLOAT3(-1.0f,  1.0f, -1.0f), XMFLOAT3(0.0f, 1.0f, 0.0f) }, // 1
    { XMFLOAT3(1.0f,  1.0f, -1.0f), XMFLOAT3(1.0f, 1.0f, 0.0f) }, // 2
    { XMFLOAT3(1.0f, -1.0f, -1.0f), XMFLOAT3(1.0f, 0.0f, 0.0f) }, // 3
    { XMFLOAT3(-1.0f, -1.0f,  1.0f), XMFLOAT3(0.0f, 0.0f, 1.0f) }, // 4
    { XMFLOAT3(-1.0f,  1.0f,  1.0f), XMFLOAT3(0.0f, 1.0f, 1.0f) }, // 5
    { XMFLOAT3(1.0f,  1.0f,  1.0f), XMFLOAT3(1.0f, 1.0f, 1.0f) }, // 6
    { XMFLOAT3(1.0f, -1.0f,  1.0f), XMFLOAT3(1.0f, 0.0f, 1.0f) }  // 7
};

static WORD g_Indicies[36] =
{
    0, 1, 2, 0, 2, 3,
    4, 6, 5, 4, 7, 6,
    4, 5, 1, 4, 1, 0,
    3, 2, 6, 3, 6, 7,
    1, 5, 6, 1, 6, 2,
    4, 0, 3, 4, 3, 7
};

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
    XMVECTOR cameraPos = DirectX::XMVectorSet(0, 0, 50, 1);
    XMVECTOR cameraTarget = DirectX::XMVectorSet(-100, 0, 0, 1);
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

    //m_meshes = LoadObjModel("D:/BUAS/Y4/DX12Renderer/Assets/Models/crytek-sponza/sponza_nobanner.obj");

    m_Monkey = LoadObjModel("../../Assets/Models/Lantern/lantern_obj.obj");
    const Texture* color = LoadTextureIndependant("../../Assets/Models/Lantern/textures/color.jpg");
    const Texture* normal = LoadTextureIndependant("../../Assets/Models/Lantern/textures/normal.jpg");
    const Texture* metallic = LoadTextureIndependant("../../Assets/Models/Lantern/textures/worn-shiny-metal-Metallic.png");
    const Texture* roughness = LoadTextureIndependant("../../Assets/Models/Lantern/textures/roughness.jpg");
    const Texture* ao = LoadTextureIndependant("../../Assets/Models/Lantern/textures/ao.jpg");
    m_MonekyTextureList.emplace(std::make_pair("diffuse", const_cast<Texture*>(color)));
    m_MonekyTextureList.emplace(std::make_pair("normal", const_cast<Texture*>(normal)));
    m_MonekyTextureList.emplace(std::make_pair("metallic", const_cast<Texture*>(metallic)));
    m_MonekyTextureList.emplace(std::make_pair("roughness", const_cast<Texture*>(roughness)));
    m_MonekyTextureList.emplace(std::make_pair("ao", const_cast<Texture*>(ao)));

    for (int i = 0; i < m_Monkey.size(); i++)
    {
        m_Monkey[i].AddTextureData(m_MonekyTextureList);
    }

    m_ContentLoaded = true;

    // Resize/Create the depth buffer.
    ResizeDepthBuffer(GetClientWidth(), GetClientHeight());

    m_UploadBuffer = std::make_unique<MakeUploadBuffer>(device, static_cast<size_t>(_2MB));

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
    mat.ModelMatrix = model;
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
    float speedMultipler = (m_Shift ? 200.0f : 100.0f);

    XMVECTOR cameraTranslate = DirectX::XMVectorSet(m_Right - m_Left, 0.0f, m_Forward - m_Backward, 1.0f) * speedMultipler *
        static_cast<float>(e.ElapsedTime);
    XMVECTOR cameraPan =
        DirectX::XMVectorSet(0.0f, m_Up - m_Down, 0.0f, 1.0f) * speedMultipler * static_cast<float>(e.ElapsedTime);
    m_Camera.Translate(cameraTranslate, Space::World);
    m_Camera.Translate(cameraPan, Space::World);

    XMVECTOR cameraRotation =
        DirectX::XMQuaternionRotationRollPitchYaw(XMConvertToRadians(m_Pitch), XMConvertToRadians(m_Yaw), XMConvertToRadians(0.0f));
    m_Camera.set_Rotation(cameraRotation);

    XMMATRIX viewMatrix = m_Camera.get_ViewMatrix();

    // Update the model matrix.
    float angle = static_cast<float>(e.TotalTime * 90.0);
    const XMVECTOR rotationAxis = DirectX::XMVectorSet(0, 1, 1, 0);
    m_ModelMatrix = XMMatrixRotationAxis(rotationAxis, XMConvertToRadians(angle));

    // Update the model matrix.
    m_TempModelMatrix = XMMatrixScaling(0.25, 0.25, 0.25);

    // Update the view matrix.
    const XMVECTOR eyePosition = DirectX::XMVectorSet(0, -50, 150, 1);
    const XMVECTOR focusPoint = DirectX::XMVectorSet(0, 150, 0, 1);
    const XMVECTOR upDirection = DirectX::XMVectorSet(0, 1, 0, 0);
    m_ViewMatrix = XMMatrixLookAtLH(eyePosition, focusPoint, upDirection);

    // Update the projection matrix.
    float aspectRatio = static_cast<float>(GetClientWidth()) / static_cast<float>(GetClientHeight());
    m_ProjectionMatrix = XMMatrixPerspectiveFovLH(XMConvertToRadians(m_FoV), aspectRatio, 0.1f, 1000.0f);

    const int numPointLights = 2;
    const int numSpotLights = 0;

    static const XMVECTORF32 LightColors[] = { Colors::White, Colors::Orange, Colors::Yellow, Colors::Green,
                                               Colors::Blue,  Colors::Indigo, Colors::Violet, Colors::White };

    const float radius = 8.0f;
    const float offset = 2.0f * XM_PI / numPointLights;
    const float offset2 = offset + (offset / 2.0f);

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
            l.PositionWS = { 0.0f, 25.0f, 25.0f, 1.0f };
        } 
        else {
            l.PositionWS = { static_cast<float>(std::sin(offset * i)) * radius, 9.0f,
                 static_cast<float>(std::cos(offset * i)) * radius, 1.0f };
        }

        XMVECTOR positionWS = XMLoadFloat4(&l.PositionWS);
        XMVECTOR positionVS = XMVector3TransformCoord(positionWS, viewMatrix);
        XMStoreFloat4(&l.PositionVS, positionVS);

        l.Color = XMFLOAT4(LightColors[0]);
        l.Intensity = 1.0f;
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
    ID3D12DescriptorHeap* pDescriptorHeaps[] = { Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetDescriptorHeap().Get()};

    UINT currentBackBufferIndex = m_pWindow->GetCurrentBackBufferIndex();
    auto backBuffer = m_pWindow->GetCurrentBackBuffer();
    auto rtv = m_pWindow->GetCurrentRenderTargetView();
    auto dsv = m_DSVHeap->GetCPUDescriptorHandleForHeapStart();

    // Clear the render targets.
    {
        TransitionResource(commandList, backBuffer,
            D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);

        //FLOAT clearColor[] = { 0.4f, 0.6f, 0.9f, 1.0f };
        FLOAT clearColor[] = { 0.0f, 0.0f, 0.0f, 1.0f };

        ClearRTV(commandList, rtv, clearColor);
        ClearDepth(commandList, dsv);
    }

    XMMATRIX viewMatrix = m_Camera.get_ViewMatrix();
    XMMATRIX projectionMatrix = m_Camera.get_ProjectionMatrix();

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    /*
    
    // Test Meshes
    for (int i = 0; i < m_meshes.size(); i++) {
        commandList->SetPipelineState(m_PipelineState->GetPipelineState().Get());
        commandList->SetGraphicsRootSignature(m_PipelineState->GetRootSignature().Get());
        commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, static_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_meshes[i].GetVertexBuffer()));
        commandList->IASetIndexBuffer(static_cast<D3D12_INDEX_BUFFER_VIEW*>(m_meshes[i].GetIndexBuffer()));

        commandList->RSSetViewports(1, &m_Viewport);
        commandList->RSSetScissorRects(1, &m_ScissorRect);

        commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        // Draw sponza
        XMMATRIX translationMatrix = XMMatrixIdentity();
        XMMATRIX rotationMatrix = XMMatrixIdentity();
        XMMATRIX scaleMatrix = XMMatrixIdentity();
        XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
        XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

        Mat matrices;
        ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

        SetGraphicsDynamicConstantBuffer(0, matrices, commandList, m_UploadBuffer.get());

        LightProperties lightProps;
        lightProps.NumPointLights = static_cast<uint32_t>(m_PointLights.size());
        lightProps.NumSpotLights = static_cast<uint32_t>(m_SpotLights.size());
        
        SetGraphics32BitConstants(1, lightProps, commandList);
        SetGraphicsDynamicStructuredBuffer(2, m_PointLights, commandList, m_UploadBuffer.get());
        SetGraphicsDynamicStructuredBuffer(3, m_SpotLights, commandList, m_UploadBuffer.get());

        auto descriptorIndex = m_meshes[i].GetTextureList()["diffuse"]->m_descriptorIndex;
        commandList->SetGraphicsRootDescriptorTable(4, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex));

        commandList->DrawIndexedInstanced(static_cast<uint32_t>(m_meshes[i].GetIndexList().size()), 1, 0, 0, 0);
    }

    */
    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    // Monkey PBR
    for (int i = 0; i < m_Monkey.size(); i++) 
    {
        commandList->SetPipelineState(m_PipelineState->GetPipelineState().Get());
        commandList->SetGraphicsRootSignature(m_PipelineState->GetRootSignature().Get());
        commandList->SetDescriptorHeaps(_countof(pDescriptorHeaps), pDescriptorHeaps);

        commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        commandList->IASetVertexBuffers(0, 1, static_cast<D3D12_VERTEX_BUFFER_VIEW*>(m_Monkey[i].GetVertexBuffer()));
        commandList->IASetIndexBuffer(static_cast<D3D12_INDEX_BUFFER_VIEW*>(m_Monkey[i].GetIndexBuffer()));

        commandList->RSSetViewports(1, &m_Viewport);
        commandList->RSSetScissorRects(1, &m_ScissorRect);

        commandList->OMSetRenderTargets(1, &rtv, FALSE, &dsv);

        XMMATRIX translationMatrix = XMMatrixIdentity();
        XMMATRIX rotationMatrix = XMMatrixIdentity();
        XMMATRIX scaleMatrix = XMMatrixScaling(1, 1, 1); //XMMatrixIdentity();
        XMMATRIX worldMatrix = scaleMatrix * rotationMatrix * translationMatrix;
        XMMATRIX viewProjectionMatrix = viewMatrix * projectionMatrix;

        Mat matrices;
        ComputeMatrices(worldMatrix, viewMatrix, viewProjectionMatrix, matrices);

        SetGraphicsDynamicConstantBuffer(0, matrices, commandList, m_UploadBuffer.get());

        LightProperties lightProps;
        lightProps.NumPointLights = static_cast<uint32_t>(m_PointLights.size());
        lightProps.NumSpotLights = static_cast<uint32_t>(m_SpotLights.size());

        SetGraphics32BitConstants(1, lightProps, commandList);
        XMVECTOR CameraPos = XMVector3Normalize(XMVector3TransformNormal(m_Camera.get_Translation(), viewMatrix));
        SetGraphics32BitConstants(2, CameraPos, commandList);
        SetGraphicsDynamicStructuredBuffer(3, m_PointLights, commandList, m_UploadBuffer.get());
        SetGraphicsDynamicStructuredBuffer(4, m_SpotLights, commandList, m_UploadBuffer.get());

        auto descriptorIndex = m_Monkey[i].GetTextureList()["diffuse"]->m_descriptorIndex;
        commandList->SetGraphicsRootDescriptorTable(5, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex));

        auto descriptorIndex2 = m_Monkey[i].GetTextureList()["normal"]->m_descriptorIndex;
        commandList->SetGraphicsRootDescriptorTable(6, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex2));

        auto descriptorIndex3 = m_Monkey[i].GetTextureList()["metallic"]->m_descriptorIndex;
        commandList->SetGraphicsRootDescriptorTable(7, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex3));

        auto descriptorIndex4 = m_Monkey[i].GetTextureList()["roughness"]->m_descriptorIndex;
        commandList->SetGraphicsRootDescriptorTable(8, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex4));

        auto descriptorIndex5 = m_Monkey[i].GetTextureList()["ao"]->m_descriptorIndex;
        commandList->SetGraphicsRootDescriptorTable(9, Application::Get().GetDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV)->GetGPUHandleAt(descriptorIndex5));

        commandList->DrawIndexedInstanced(static_cast<uint32_t>(m_Monkey[i].GetIndexList().size()), 1, 0, 0, 0);
    }
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
        m_Down = 1.0f;
        break;
    case KeyCode::E:
        m_Up = 1.0f;
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
        m_Down = 0.0f;
        break;
    case KeyCode::E:
        m_Up = 0.0f;
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