#include "TerrainChunkManager.h"
#include <algorithm>
#include <unordered_set>

TerrainChunkManager::TerrainChunkManager(int chunkSize, float heightScale)
    : m_chunkSize(chunkSize), m_heightScale(heightScale) {}

void TerrainChunkManager::UpdateChunks(const XMFLOAT3& cameraPosition, const std::vector<float>& heightmap, int heightmapWidth, std::unordered_map<std::string, Texture*>& textures) {
    // Compute visible size and camera chunk indices
    m_visibleSize = (m_chunkSize / m_tessFactor - 1) * m_tessFactor;

    int cameraChunkX = static_cast<int>(std::floor(cameraPosition.x / float(m_visibleSize)));
    int cameraChunkZ = static_cast<int>(std::floor(cameraPosition.z / float(m_visibleSize)));

    // Prepare new active list
    m_activeChunks.clear();

    // Track which keys should remain active
    std::unordered_set<ChunkKey, ChunkKeyHash> keysToKeep;
    keysToKeep.reserve((2 * m_loadRadius + 1) * (2 * m_loadRadius + 1));

    // Loop over the desired load-radius square
    for (int z = cameraChunkZ - m_loadRadius; z <= cameraChunkZ + m_loadRadius; ++z) {
        for (int x = cameraChunkX - m_loadRadius; x <= cameraChunkX + m_loadRadius; ++x) {
            ChunkKey key = { x, z };
            keysToKeep.insert(key);

            auto it = m_chunks.find(key);
            if (it == m_chunks.end()) {
                // Chunk not present: create & initialize
                auto chunk = std::make_shared<TerrainChunk>(x, z, m_chunkSize, m_heightScale);
                chunk->Initialize(heightmap, heightmapWidth, textures);
                chunk->SetActive(true);
                m_chunks[key] = chunk;
                m_activeChunks.push_back(chunk);
            }
            else {
                // Chunk exists: ensure active
                it->second->SetActive(true);
                m_activeChunks.push_back(it->second);
            }
        }
    }

    // Unload/deactivate chunks outside the radius
    // Iterate over m_chunks; remove those whose key is not in keysToKeep.
    for (auto it = m_chunks.begin(); it != m_chunks.end(); ) {
        const ChunkKey& key = it->first;
        if (keysToKeep.find(key) == keysToKeep.end()) {
            // This chunk is outside the load radius

            // Deactivate
            it->second->SetActive(false);

            // To do : check for explicit cleanup

            //  Erase from map to free memory
            it = m_chunks.erase(it);
        }
        else {
            ++it;
        }
    }
}

const std::vector<std::shared_ptr<TerrainChunk>>& TerrainChunkManager::GetActiveChunks() const {
    return m_activeChunks;
}