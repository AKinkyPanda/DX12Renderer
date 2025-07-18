#pragma once

#include "Game.h"
#include "Window.h"
#include "Mesh.h"
#include "PipelineState.h"
#include "../PSOSkybox.h"
#include "../PSOShadowMap.h"
#include "../Light.h"

#include <DirectXMath.h>
#include "../Camera.h"

#include "../UploadBuffer.h"
#include "../ShadowMap.h"
#include "../PSOTerrain.h"
#include "../PSOTerrainShadowMap.h"
#include "../TerrainChunkManager.h"

#include "FastNoiseLite.h"

class Mesh;

struct LightProperties
{
    uint32_t NumPointLights;
    uint32_t NumSpotLights;
    uint32_t NumDirectionalLights;
};

// An enum for root signature parameters.
// I'm not using scoped enums to avoid the explicit cast that would be required
// to use these as root indices in the root signature.
enum RootParameters
{
    MatricesCB,         // ConstantBuffer<Mat> MatCB : register(b0);
    MaterialCB,         // ConstantBuffer<Material> MaterialCB : register( b0, space1 );
    LightPropertiesCB,  // ConstantBuffer<LightProperties> LightPropertiesCB : register( b1 );
    PointLights,        // StructuredBuffer<PointLight> PointLights : register( t0 );
    SpotLights,         // StructuredBuffer<SpotLight> SpotLights : register( t1 );
    Textures,           // Texture2D DiffuseTexture : register( t2 );
    NumRootParameters
};

class Tutorial2 : public Game
{
public:
    using super = Game;

    Tutorial2(const std::wstring& name, int width, int height, bool vSync = false);
    /**
     *  Load content required for the demo.
     */
    virtual bool LoadContent() override;

    /**
     *  Unload demo specific content that was loaded in LoadContent.
     */
    virtual void UnloadContent() override;

    struct MeshData
    {
        std::vector<VertexPosColor> Vertices;
        std::vector<UINT> Indices32;

        std::vector<UINT>& GetIndices16()
        {
            if (mIndices16.empty())
            {
                mIndices16.resize(Indices32.size());
                for (size_t i = 0; i < Indices32.size(); ++i)
                    mIndices16[i] = static_cast<UINT>(Indices32[i]);
            }

            return mIndices16;
        }

    private:
        std::vector<UINT> mIndices16;
    };

    MeshData CreateBox(float width, float height, float depth, UINT numSubdivisions);

    void Subdivide(MeshData& meshData);
    VertexPosColor MidPoint(const VertexPosColor& v0, const VertexPosColor& v1);

    std::vector<float> Tutorial2::GenerateHeightmap(FastNoiseLite::NoiseType noiseType, int width, int height);

    void Tutorial2::BuildFrustumPlanes(const XMMATRIX& view, const XMMATRIX& proj, std::array<XMFLOAT4, 6>& outPlanes);

    bool Tutorial2::IsBoxInsideFrustum(float minX, float minY, float minZ, float maxX, float maxY, float maxZ, const std::array<XMFLOAT4, 6>& planes);

    //Descriptor Heap for textures
    std::shared_ptr<ID3D12DescriptorHeap> m_SRVHeap;

protected:
    /**
     *  Update the game logic.
     */
    virtual void OnUpdate(UpdateEventArgs& e) override;

    /**
     *  Render stuff.
     */
    virtual void OnRender(RenderEventArgs& e) override;

    /**
     * Invoked by the registered window when a key is pressed
     * while the window has focus.
     */
    virtual void OnKeyPressed(KeyEventArgs& e) override;

    virtual void OnKeyReleased(KeyEventArgs& e) override;

    virtual void OnMouseMoved(MouseMotionEventArgs& e) override;

    /**
     * Invoked when the mouse wheel is scrolled while the registered window has focus.
     */
    virtual void OnMouseWheel(MouseWheelEventArgs& e) override;


    virtual void OnResize(ResizeEventArgs& e) override;

private:
    // Helper functions
    // Transition a resource
    void TransitionResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        Microsoft::WRL::ComPtr<ID3D12Resource> resource,
        D3D12_RESOURCE_STATES beforeState, D3D12_RESOURCE_STATES afterState);

    // Clear a render target view.
    void ClearRTV(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE rtv, FLOAT* clearColor);

    // Clear the depth of a depth-stencil view.
    void ClearDepth(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        D3D12_CPU_DESCRIPTOR_HANDLE dsv, FLOAT depth = 1.0f);

    // Create a GPU buffer.
    void UpdateBufferResource(Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList2> commandList,
        ID3D12Resource** pDestinationResource, ID3D12Resource** pIntermediateResource,
        size_t numElements, size_t elementSize, const void* bufferData,
        D3D12_RESOURCE_FLAGS flags = D3D12_RESOURCE_FLAG_NONE);

    // Resize the depth buffer to match the size of the client area.
    void ResizeDepthBuffer(int width, int height);

    void ComputeLightSpaceMatrix();

    uint64_t m_FenceValues[Window::BufferCount] = {};

    // Heightmap / Terrain
    std::vector<Mesh> m_Terrain;
    TerrainChunkManager m_TerrainChunkManager;
    std::vector<float> m_HeightmapData;
    std::shared_ptr<PSOTerrain> m_TerrainPipelineState;
    const Texture* m_TerrainGrassTexture;
    const Texture* m_TerrainBlendTexture;
    const Texture* m_TerrainRockTexture;

    // Terrain Shadow Map
    std::unique_ptr<ShadowMap> m_TerrainShadowMap;
    uint32_t m_TerrainShadowMapCPUSRVDescriptorIndex;
    uint32_t m_TerrainShadowMapGPUSRVDescriptorIndex;
    uint32_t m_TerrainShadowMapCPUDSVDescriptorIndex;
    std::shared_ptr<PSOTerrainShadowMap> m_TerrainShadowMapPipelineState;

    // Terrain Noise Heihtmap
    FastNoiseLite m_Noise;

    // Skybox
    Mesh m_SkyBoxMesh;
    ComPtr<ID3D12Resource> m_SkyTexture;
    ID3D12Resource* m_SkyTexture2;
    uint32_t m_SkyDescriptorIndex;

    std::shared_ptr<PSOSkybox> m_SkyboxPipelineState;

    //Shadow Map
    std::unique_ptr<ShadowMap> m_ShadowMap;
    uint32_t m_ShadowMapCPUSRVDescriptorIndex;
    uint32_t m_ShadowMapGPUSRVDescriptorIndex;
    uint32_t m_ShadowMapCPUDSVDescriptorIndex;
    std::shared_ptr<PSOShadowMap> m_ShadowMapPipelineState;

    // Depth buffer.
    Microsoft::WRL::ComPtr<ID3D12Resource> m_DepthBuffer;
    // Descriptor heap for depth buffer.
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> m_DSVHeap;

    // Root signature
    Microsoft::WRL::ComPtr<ID3D12RootSignature> m_RootSignature;

    // Pipeline state object.
    //Microsoft::WRL::ComPtr<ID3D12PipelineState> m_PipelineState;
    //Microsoft::WRL::ComPtr<PipelineState> m_PipelineState;
    std::shared_ptr<PipelineState> m_PipelineState;

    D3D12_VIEWPORT m_Viewport;
    D3D12_RECT m_ScissorRect;

    Camera m_Camera;
    struct alignas(16) CameraData
    {
        DirectX::XMVECTOR m_InitialCamPos;
        DirectX::XMVECTOR m_InitialCamRot;
    };
    CameraData* m_pAlignedCameraData;

    float m_FoV;

    int  m_Width;
    int  m_Height;

    // Camera controller
    float m_Forward;
    float m_Backward;
    float m_Left;
    float m_Right;
    float m_Up;
    float m_Down;

    float m_Pitch;
    float m_Yaw;
    int m_PreviousMouseX;
    int m_PreviousMouseY;

    bool m_Shift;
    bool g_AllowFullscreenToggle;
    bool m_VSync;

    DirectX::XMMATRIX m_ModelMatrix;
    DirectX::XMMATRIX m_ViewMatrix;
    DirectX::XMMATRIX m_ProjectionMatrix;

    DirectX::XMMATRIX m_LightViewProj;

    // Define some lights.
    std::vector<PointLight> m_PointLights;
    std::vector<SpotLight>  m_SpotLights;
    std::vector<DirectionalLight>  m_DirectionalLights;
    std::unique_ptr<UploadBuffer> m_UploadBuffer;

    bool m_ContentLoaded;
};