#pragma once

#include "../DX12Renderer/DX12Renderer/Mesh.h"
#include <DirectXMath.h>
#include "DX12Renderer/FastNoiseLite.h"
#include "DX12Renderer/Texture.h"

using namespace DirectX;

class TerrainChunk {
public:
    TerrainChunk(int chunkX, int chunkZ, int size, float heightScale);
    void Initialize(const std::vector<float>& heightmap, int heightmapWidth, std::unordered_map<std::string, Texture*>& textures);

    std::vector<float> GenerateChunkHeightmap(
        FastNoiseLite::NoiseType noiseType,
        int chunkX, int chunkZ,
        int width, int height,
        float noiseScale);

    float FbmNoiseWithFD(float x0, float z0, int octaves, float lac, float gain, float eps, float kAtten);
    float UberNoise(
        float x0, float z0,
        int   octaves,
        float perturbAmt,
        float sharpness,
        float amplify,
        float altitudeErode,
        float ridgeErode,
        float slopeErode,
        float lacunarity,
        float gain,
        float baseFreq,
        float eps,
        float kAtten);

    float smoothstep(float edge0, float edge1, float x);

    void SetActive(bool active);
    bool IsActive() const;

    Mesh GetMesh();
    XMFLOAT3 GetWorldPosition() const;

    inline XMINT2 GetChunk() { return XMINT2(m_chunkX, m_chunkZ); }
    inline int GetChunkSize() { return m_size; }

    // Optional if you're reusing identity scaling/rotation
    inline XMMATRIX GetWorldMatrix() const {
        return XMMatrixTranslation(m_position.x, m_position.y, m_position.z);
    }

private:
    int m_chunkX, m_chunkZ;
    int m_size;
    float m_heightScale;
    bool m_active = true;

    FastNoiseLite m_Noise;

    Mesh m_mesh;
    XMFLOAT3 m_position;

    std::unique_ptr<Texture> m_heightmapTexture;
};