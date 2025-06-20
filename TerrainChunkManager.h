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

    // 2) Equality comparator: two keys equal if both coords match
    struct ChunkKeyEqual {
        bool operator()(const ChunkKey& a, const ChunkKey& b) const noexcept {
            return a.first == b.first && a.second == b.second;
        }
    };

    // 3) Hash functor: combine x and z into a size_t
    struct ChunkKeyHash {
        size_t operator()(const ChunkKey& key) const noexcept {
            // Option A: combine into 64-bit then hash
            // Shift x into high 32 bits, z in low 32 bits:
            uint64_t ux = static_cast<uint32_t>(key.first);
            uint64_t uz = static_cast<uint32_t>(key.second);
            uint64_t combined = (ux << 32) ^ uz;
            return std::hash<uint64_t>()(combined);

            // Option B: manual combine with primes:
            // size_t h1 = std::hash<int>()(key.x);
            // size_t h2 = std::hash<int>()(key.z);
            // return h1 ^ (h2 + 0x9e3779b97f4a7c15ULL + (h1<<6) + (h1>>2));
        }
    };

    // assuming TerrainChunk is some class
    std::unordered_map<ChunkKey, std::shared_ptr<TerrainChunk>,
        ChunkKeyHash, ChunkKeyEqual> m_chunks;
    //std::unordered_map<ChunkKey, std::shared_ptr<TerrainChunk>, pair_hash> m_chunks;
    std::vector<std::shared_ptr<TerrainChunk>> m_activeChunks;
    int m_chunkSize;
    float m_heightScale;
    int m_loadRadius = 3; // Radius in chunks

    int   m_tessFactor = 4;   // e.g. 4
    int   m_visibleSize = 0;  // = (m_chunkSize/m_tessFactor - 1) * m_tessFactor

    FastNoiseLite m_Noise;
};

