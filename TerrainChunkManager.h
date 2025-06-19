#pragma once

#include <DirectXMath.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <d3d12.h>
#include <memory>
#include <wrl.h>

#include "TerrainChunk.h"

using namespace DirectX;

class TerrainChunkManager {
public:
    TerrainChunkManager() = default;
    TerrainChunkManager(int chunkSize, float heightScale);
    void UpdateChunks(const XMFLOAT3& cameraPosition, const std::vector<float>& heightmap, int heightmapWidth, std::unordered_map<std::string, Texture*>& textures);
    const std::vector<std::shared_ptr<TerrainChunk>>& GetActiveChunks() const;

private:
    using ChunkKey = std::pair<int, int>;

    struct pair_hash {
        std::size_t operator()(const ChunkKey& k) const {
            return std::hash<int>()(k.first) ^ std::hash<int>()(k.second);
        }
    };

    std::unordered_map<ChunkKey, std::shared_ptr<TerrainChunk>, pair_hash> m_chunks;
    std::vector<std::shared_ptr<TerrainChunk>> m_activeChunks;
    int m_chunkSize;
    float m_heightScale;
    int m_loadRadius = 3; // Radius in chunks

    FastNoiseLite m_Noise;
};

