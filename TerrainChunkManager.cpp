#include "TerrainChunkManager.h"
#include <algorithm>

TerrainChunkManager::TerrainChunkManager(int chunkSize, float heightScale)
    : m_chunkSize(chunkSize), m_heightScale(heightScale) {}

void TerrainChunkManager::UpdateChunks(const XMFLOAT3& cameraPosition, const std::vector<float>& heightmap, int heightmapWidth, std::unordered_map<std::string, Texture*>& textures) {
    //int cameraChunkX = (int)(cameraPosition.x / (m_chunkSize - 1));
    //int cameraChunkZ = (int)(cameraPosition.z / (m_chunkSize - 1));

    int cameraChunkX = static_cast<int>(std::floor(cameraPosition.x / float(m_chunkSize - 1)));
    int cameraChunkZ = static_cast<int>(std::floor(cameraPosition.z / float(m_chunkSize - 1)));

    m_activeChunks.clear();

    for (int z = cameraChunkZ - m_loadRadius; z <= cameraChunkZ + m_loadRadius; ++z) {
        for (int x = cameraChunkX - m_loadRadius; x <= cameraChunkX + m_loadRadius; ++x) {
            ChunkKey key = { x, z };

            if (m_chunks.find(key) == m_chunks.end()) {
                auto chunk = std::make_shared<TerrainChunk>(x, z, m_chunkSize, m_heightScale);

                chunk->Initialize(heightmap, heightmapWidth, textures);
                m_chunks[key] = chunk;
            }

            m_chunks[key]->SetActive(true);
            m_activeChunks.push_back(m_chunks[key]);
        }
    }
}

const std::vector<std::shared_ptr<TerrainChunk>>& TerrainChunkManager::GetActiveChunks() const {
    return m_activeChunks;
}