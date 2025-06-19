#include "TerrainChunk.h"
#include <algorithm>
#include "DX12Renderer/Texture.h"

TerrainChunk::TerrainChunk(int chunkX, int chunkZ, int size, float heightScale)
    : m_chunkX(chunkX), m_chunkZ(chunkZ), m_size(size), m_heightScale(heightScale) {}

//void TerrainChunk::Initialize(const std::vector<float>& heightmap, int heightmapWidth, std::unordered_map<std::string, Texture*>& textures) {
//
//    int tessFactor = 4;
//    int scalePatchX = m_size / tessFactor;
//    int scalePatchZ = m_size / tessFactor;
//    int arrSize = scalePatchX * scalePatchZ;
//
//    std::vector <VertexPosition> m_vertices = std::vector<VertexPosition>();
//
//    int vertsPerSide = m_size / tessFactor; // 256
//
//    std::vector<float> heightmapLocal = GenerateChunkHeightmap(
//        FastNoiseLite::NoiseType_OpenSimplex2,
//        m_chunkX, m_chunkZ,
//        vertsPerSide, vertsPerSide,
//        0.02f // or another scale
//    );
//
//    std::vector<uint8_t> imageData(heightmapLocal.size() * 4); // RGBA for each pixel
//
//    for (size_t i = 0; i < heightmapLocal.size(); ++i)
//    {
//        // Remap from [-1,1] to [0,1]
//        float normalized = (heightmapLocal[i] + 1.0f) * 0.5f;
//
//        // Clamp to [0, 255]
//        uint8_t height = static_cast<uint8_t>(std::clamp(normalized * 255.0f, 0.0f, 255.0f));
//
//        // Fill RGBA
//        imageData[i * 4 + 0] = height;
//        imageData[i * 4 + 1] = height;
//        imageData[i * 4 + 2] = height;
//        imageData[i * 4 + 3] = 255; // Fully opaque
//    }
//
//    m_heightmapTexture = std::make_unique<Texture>(imageData, XMFLOAT2(1024 + 1, 1024 + 1));
//
//    m_vertices.clear();
//    m_vertices.resize(arrSize);
//
//    int visibleSize = (m_size / tessFactor - 1) * tessFactor;
//    int chunkWorldX = m_chunkX * visibleSize;
//    int chunkWorldZ = m_chunkZ * visibleSize;
//
//    for (int z = 0; z < scalePatchZ; ++z) {
//        for (int x = 0; x < scalePatchX; ++x) {
//            int sampleX = x * tessFactor + chunkWorldX;
//            int sampleZ = z * tessFactor + chunkWorldZ;
//
//            int sampleXheight = x * tessFactor;
//            int sampleZheigth = z * tessFactor;
//            int noiseIndex = sampleZheigth * (m_size + 1) + sampleXheight;
//
//            // Normalize Perlin noise to [0,1]
//            float height = (heightmapLocal[noiseIndex] + 1.0f) * 0.5f;
//            height *= scalePatchX;
//
//            m_vertices[z * scalePatchX + x].Position = XMFLOAT3(
//                static_cast<float>(sampleX),
//                height,
//                static_cast<float>(sampleZ));
//        }
//    }
//
//    std::vector<UINT> m_indices = std::vector<UINT>();
//
//    // Tessellated quad patch indices (4 control points per patch)
//    m_indices.clear();
//    for (int z = 0; z < scalePatchZ - 1; ++z) {
//        for (int x = 0; x < scalePatchX - 1; ++x) {
//            UINT vert0 = x + z * scalePatchX;
//            UINT vert1 = (x + 1) + z * scalePatchX;
//            UINT vert2 = x + (z + 1) * scalePatchX;
//            UINT vert3 = (x + 1) + (z + 1) * scalePatchX;
//
//            m_indices.push_back(vert0);
//            m_indices.push_back(vert1);
//            m_indices.push_back(vert2);
//            m_indices.push_back(vert3);
//        }
//    }
//
//    textures.erase("Heightmap");
//    textures.emplace(std::make_pair("Heightmap", const_cast<Texture*>(m_heightmapTexture.get())));
//
//    m_mesh.AddVertexData(m_vertices);
//    m_mesh.AddIndexData(m_indices);
//    m_mesh.AddTextureData(textures);
//    m_mesh.CreateBuffers();
//}

void TerrainChunk::Initialize(const std::vector<float>& /*unused global heightmap*/, int /*heightmapWidth*/, std::unordered_map<std::string, Texture*>& textures) {
    int tessFactor = 4;
    int vertsPerSide = m_size / tessFactor; // 256
    int patchesPerSide = vertsPerSide - 1;  // 255
    int visibleSize = patchesPerSide * tessFactor; // 1020
    int arrSize = vertsPerSide * vertsPerSide; // 256*256

    // Generate local noise-based heightmap
    std::vector<float> heightmapLocal = GenerateChunkHeightmap(
        FastNoiseLite::NoiseType_OpenSimplex2,
        m_chunkX, m_chunkZ,
        vertsPerSide, vertsPerSide,  // width=vertsPerSide
        0.04f
    );

    // (Optional) upload heightmapLocal into a texture of size 256x256 if needed.
    // Fill RGBA imageData as before, but dimensions = vertsPerSide x vertsPerSide:
    std::vector<uint8_t> imageData(vertsPerSide * vertsPerSide * 4);
    for (int i = 0; i < vertsPerSide * vertsPerSide; ++i) {
        float normalized = (heightmapLocal[i] + 1.0f) * 0.5f;
        uint8_t h = static_cast<uint8_t>(std::clamp(normalized * 255.0f, 0.0f, 255.0f));
        imageData[i * 4 + 0] = h;
        imageData[i * 4 + 1] = h;
        imageData[i * 4 + 2] = h;
        imageData[i * 4 + 3] = 255;
    }
    m_heightmapTexture = std::make_unique<Texture>(imageData, XMFLOAT2((float)vertsPerSide, (float)vertsPerSide));

    // Prepare vertex array
    std::vector <VertexPosition> m_vertices = std::vector<VertexPosition>();
    m_vertices.clear();
    m_vertices.resize(arrSize);

    int chunkWorldX = m_chunkX * visibleSize;
    int chunkWorldZ = m_chunkZ * visibleSize;

    for (int z = 0; z < vertsPerSide; ++z) {
        for (int x = 0; x < vertsPerSide; ++x) {
            int idx = z * vertsPerSide + x;
            float worldX = float(chunkWorldX + x * tessFactor);
            float worldZ = float(chunkWorldZ + z * tessFactor);
            float h = (heightmapLocal[idx] + 1.0f) * 0.5f * m_heightScale;
            m_vertices[idx].Position = XMFLOAT3(worldX, h, worldZ);
            // Also set UV = float2(x/(vertsPerSide-1), z/(vertsPerSide-1)) for domain shader if needed
        }
    }
    // Build index list for tessellated patches: patchesPerSide x patchesPerSide, each patch uses 4 control points:
    std::vector<UINT> m_indices = std::vector<UINT>();
    m_indices.clear();
    for (int z = 0; z < patchesPerSide; ++z) {
        for (int x = 0; x < patchesPerSide; ++x) {
            UINT v0 = x + z * vertsPerSide;
            UINT v1 = (x + 1) + z * vertsPerSide;
            UINT v2 = x + (z + 1) * vertsPerSide;
            UINT v3 = (x + 1) + (z + 1) * vertsPerSide;
            m_indices.push_back(v0);
            m_indices.push_back(v1);
            m_indices.push_back(v2);
            m_indices.push_back(v3);
        }
    }

    // Set texture and buffers as before
    textures.erase("Heightmap");
    textures.emplace("Heightmap", const_cast<Texture*>(m_heightmapTexture.get()));
    m_mesh.AddVertexData(m_vertices);
    m_mesh.AddIndexData(m_indices);
    m_mesh.AddTextureData(textures);
    m_mesh.CreateBuffers();
}

void TerrainChunk::SetActive(bool active) {
    m_active = active;
}

bool TerrainChunk::IsActive() const {
    return m_active;
}

Mesh TerrainChunk::GetMesh() {
    return m_mesh;
}

XMFLOAT3 TerrainChunk::GetWorldPosition() const {
    // Compute number of quads (patches) in the chunk
    int patchCount = (m_size - 1) / 4; // -1 because we count edges between vertices
    int chunkWorldSize = patchCount * 4; // This gives you how much space the chunk spans in world units

    return XMFLOAT3(
        static_cast<float>(m_chunkX * chunkWorldSize),
        0.0f,
        static_cast<float>(m_chunkZ * chunkWorldSize)
    );

    //return XMFLOAT3((float)(m_chunkX * (m_size / 4 - 1)), 0.0f, (float)(m_chunkZ * (m_size / 4 - 1)));
}

//XMFLOAT3 TerrainChunk::GetWorldPosition() const {
//    int quadCountPerSide = (m_size - 1); // number of quads = vertices - 1
//    return XMFLOAT3(
//        static_cast<float>(m_chunkX * quadCountPerSide),
//        0.0f,
//        static_cast<float>(m_chunkZ * quadCountPerSide)
//    );
//}

//XMFLOAT3 TerrainChunk::GetWorldPosition() const
//{
//    int patchCount = (m_size - 1) / 4;
//    int worldSize = patchCount * 4;
//
//    return XMFLOAT3((float)(m_chunkX * (m_size / 4 - 1)), 0.0f, (float)(m_chunkZ * (m_size / 4 - 1)));
//
//    return XMFLOAT3(
//        static_cast<float>(m_chunkX * worldSize),
//        0.0f,
//        static_cast<float>(m_chunkZ * worldSize)
//    );
//}

//std::vector<float> TerrainChunk::GenerateChunkHeightmap(
//    FastNoiseLite::NoiseType noiseType,
//    int chunkX, int chunkZ,
//    int width, int height,
//    float noiseScale)
//{
//    m_Noise.SetNoiseType(noiseType);
//    m_Noise.SetFrequency(noiseScale);
//
//    m_Noise.SetFractalType(FastNoiseLite::FractalType_FBm);
//    m_Noise.SetFractalOctaves(4);
//    m_Noise.SetFractalLacunarity(2.0f);
//    m_Noise.SetFractalGain(0.5f);
//
//    std::vector<float> heightmap((width + 1) * (height + 1));
//
//    // Sample from *world position* based on chunk offset
//    for (int z = 0; z <= height; ++z)
//    {
//        for (int x = 0; x <= width; ++x)
//        {
//            // Convert local chunk coord to world-space coordinate
//            int visibleSize = (m_size / 4 - 1) * 4; // 1020
//            float worldX = static_cast<float>(chunkX * visibleSize + x * 4);
//            float worldZ = static_cast<float>(chunkZ * visibleSize + z * 4);
//
//            // Scale it into noise space
//            float nx = worldX * noiseScale;
//            float nz = worldZ * noiseScale;
//
//            heightmap[z * (width + 1) + x] = m_Noise.GetNoise(nx, nz);
//        }
//    }
//
//    return heightmap;
//}

std::vector<float> TerrainChunk::GenerateChunkHeightmap(
    FastNoiseLite::NoiseType noiseType,
    int chunkX, int chunkZ,
    int vertsPerSide,    // = m_size / tessFactor = 256
    int UNUSED_height,   // same as width
    float noiseScale)
{
    m_Noise.SetNoiseType(noiseType);
    m_Noise.SetFrequency(noiseScale);
    m_Noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    m_Noise.SetFractalOctaves(4);
    m_Noise.SetFractalLacunarity(2.0f);
    m_Noise.SetFractalGain(0.5f);

    int tessFactor = 4;
    int patchesPerSide = vertsPerSide - 1; // 255
    int visibleSize = patchesPerSide * tessFactor; // 1020

    std::vector<float> heightmapLocal(vertsPerSide * vertsPerSide);

    // Sample from world position based on chunk offset, stepping by tessFactor
    for (int z = 0; z < vertsPerSide; ++z) {
        for (int x = 0; x < vertsPerSide; ++x) {
            float worldX = float(chunkX * visibleSize + x * tessFactor);
            float worldZ = float(chunkZ * visibleSize + z * tessFactor);
            float nx = worldX * noiseScale;
            float nz = worldZ * noiseScale;
            heightmapLocal[z * vertsPerSide + x] = m_Noise.GetNoise(nx, nz);
        }
    }
    return heightmapLocal;
}