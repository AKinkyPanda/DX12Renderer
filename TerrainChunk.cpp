#define NOMINMAX

#include "TerrainChunk.h"
#include <algorithm>
#include "DX12Renderer/Texture.h"

TerrainChunk::TerrainChunk(int chunkX, int chunkZ, int size, float heightScale)
    : m_chunkX(chunkX), m_chunkZ(chunkZ), m_size(size), m_heightScale(heightScale), m_position(XMFLOAT3(0,0,0)) {}

void TerrainChunk::Initialize(const std::vector<float>& /*unused global heightmap*/, int /*heightmapWidth*/, std::unordered_map<std::string, Texture*>& textures) {
    int tessFactor = 4;
    int vertsPerSide = m_size / tessFactor; // 256
    int patchesPerSide = vertsPerSide - 1;  // 255
    int visibleSize = patchesPerSide * tessFactor; // 1020
    int arrSize = vertsPerSide * vertsPerSide; // 256*256

    // Generate local noise-based heightmap
    std::vector<float> heightmapLocal = GenerateChunkHeightmap(
        FastNoiseLite::NoiseType_Perlin,
        m_chunkX, m_chunkZ,
        vertsPerSide, vertsPerSide,  // width=vertsPerSide
        0.075f
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

std::vector<float> TerrainChunk::GenerateChunkHeightmap(
    FastNoiseLite::NoiseType noiseType,
    int chunkX, int chunkZ,
    int vertsPerSide,    // = m_size / tessFactor = 256
    int UNUSED_height,   // same as width
    float noiseScale)
{
    m_Noise.SetNoiseType(noiseType);
    m_Noise.SetFrequency(noiseScale);
    m_Noise.SetFractalType(FastNoiseLite::FractalType_None);
    //m_Noise.SetFractalType(FastNoiseLite::FractalType_FBm);
    //m_Noise.SetFractalOctaves(8);
    //m_Noise.SetFractalLacunarity(1.8f);
    //m_Noise.SetFractalGain(0.5f);

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

            //float base = 1 - std::abs(FbmNoiseWithFD2(nx, nz, 8, 1.8f, 0.5f, 1e-3f, /*kAtten*/ 0.5f));
            //float base = 1 - std::abs(m_Noise.GetNoise(nx, nz));

            //XMFLOAT2 q = XMFLOAT2(FbmNoiseWithFD(nx + 0, nz + 0, 8, 1.8f, 0.5f, 1e-3f, /*kAtten*/ 0.5f),
            //                      FbmNoiseWithFD(nx + 5.2f, nz + 1.3f, 8, 1.8f, 0.5f, 1e-3f, /*kAtten*/ 0.5f));            
            //
            //XMFLOAT2 r = XMFLOAT2(FbmNoiseWithFD(nx + 1.7f + 4*q.x, nz + 9.2f + 4*q.y, 8, 1.8f, 0.5f, 1e-3f, /*kAtten*/ 0.5f),
            //                      FbmNoiseWithFD(nx + 8.3f + 4*q.x, nz + 2.8f + 4 * q.y, 8, 1.8f, 0.5f, 1e-3f, /*kAtten*/ 0.5f));

            //float base = FbmNoiseWithFD(nx + 4 * r.x, nz + 4 * r.y, 8, 1.8f, 0.5f, 1e-3f, /*kAtten*/ 0.5f);

            float base = UberNoise(/*x*/ nx, /*y*/ nz, /*octaves*/ 8, /*perturbance*/ 0.15f, /*sharpness*/ 0.25f, /*amplification*/ 0.2f, 
                                   /*altErode*/ 0.75f, /*RidgeErode*/ 0.75f, /*SlopeErode*/ 0.001f,  /*lacunarity*/ 1.8f, /*gain*/ 0.5f, 
                                   /*frequency*/ noiseScale, /*epsilon*/ 1e-3f, /*kAtten*/ 0.5f );

            heightmapLocal[z * vertsPerSide + x] = base;
        }
    }
    return heightmapLocal;
}

float TerrainChunk::FbmNoiseWithFD(
    float x0, float z0,
    int octaves,
    float lac,
    float gain,
    float eps,
    float kAtten) 
{
    // 1. Theoretical amplitude sum (no attenuation) for normalization
    float sumAmp = 0.0f;
    {
        float ampTmp = 1.0f;
        for (int i = 0; i < octaves; i++) {
            sumAmp += ampTmp;
            ampTmp *= gain;
        }
    }

    float sum = 0.5f;
    float amplitude = 1.0f;
    float freq = 1.0f;

    XMFLOAT2 dsum = XMFLOAT2(0, 0);

    // Running derivative of height so far (in base coordinate space)
    float derivX = 0.0f;
    float derivZ = 0.0f;

    for (int i = 0; i < octaves; i++) {
        float x = x0 * freq;
        float z = z0 * freq;

        // Sample noise
        float n = m_Noise.GetNoise(x, z); // [-1,1]

        // Compute derivative of this octave’s noise (in base coords)
        float dx_n = 0.0f, dz_n = 0.0f;
        if (kAtten > 0.0f) {
            float nx = m_Noise.GetNoise(x + eps, z);
            float nz = m_Noise.GetNoise(x, z + eps);
            dx_n = (nx - n) / eps * freq;
            dz_n = (nz - n) / eps * freq;
        }

        // Accumulate height using current amplitude
        //sum += n * amplitude;
        dsum = XMFLOAT2(dsum.x + dx_n, dsum.y + dz_n);
        float dot = dsum.x * dsum.x + dsum.y * dsum.y;

        sum += amplitude * n / (1 + dot);


        // Update running derivative sum
        if (kAtten > 0.0f) {
            derivX += dx_n * amplitude;
            derivZ += dz_n * amplitude;
            // Compute slope of accumulated height so far
            float slopeAccum = std::sqrt(derivX * derivX + derivZ * derivZ);
            // Attenuate next octave’s amplitude
            float attenuation = std::exp(-kAtten * slopeAccum);
            amplitude *= gain * attenuation;
        }
        else {
            // No attenuation
            amplitude *= gain;
        }

        freq *= lac;
    }

    // Normalize to [-1,1]
    float hNorm = sum / sumAmp;
    return hNorm;
    //return 0.5f * (hNorm + 1.0f);
    //return sum;
}

float TerrainChunk::UberNoise(
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
    float kAtten)       // FDG’s slope attenuation 
{
    // 1. Theoretical amplitude sum (no attenuation) for normalization
    float sumAmp = 0.0f;
    {
        float ampTmp = 1.0f;
        for (int i = 0; i < octaves; i++) {
            sumAmp += ampTmp;
            ampTmp *= gain;
        }
    }

    float x = x0;
    float z = z0;

    float sum = 0.5f;
    float amplitude = 1.0f;
    float freq = 1.0f;
    float dampedAmplitude = 0.0f;
    float currentGain = gain;

    XMFLOAT2 slopeErosionDerSum = XMFLOAT2(0, 0);
    XMFLOAT2 ridgeErosionDerSum = XMFLOAT2(0, 0);
    XMFLOAT2 perturbDerSum = XMFLOAT2(0, 0);

    // Running derivative of height so far (in base coordinate space)
    float derivX = 0.0f;
    float derivZ = 0.0f;

    for (int i = 0; i < octaves; i++) {

        // Sample noise
        float n = m_Noise.GetNoise(x, z); // [-1,1]

        float ridgedNoise = 1.0 - abs(n);
        float billowNoise = n * n;

        n = std::lerp(n, billowNoise, std::max(0.0f, sharpness));
        n = std::lerp(n, ridgedNoise, std::abs(std::min(0.0f, sharpness)));


        // Compute derivative of this octave’s noise (in base coords)
        float dx_n = 0.0f, dz_n = 0.0f;
        if (kAtten > 0.0f) {
            float nx = m_Noise.GetNoise(x + eps, z);
            float nz = m_Noise.GetNoise(x, z + eps);
            dx_n = (nx - n) / eps * freq;
            dz_n = (nz - n) / eps * freq;
        }

        slopeErosionDerSum = XMFLOAT2((slopeErosionDerSum.x + dx_n) * slopeErode, (slopeErosionDerSum.y + dz_n) * slopeErode);
        float dot = slopeErosionDerSum.x * slopeErosionDerSum.x + slopeErosionDerSum.y * slopeErosionDerSum.y;

        sum += amplitude * n * (1.0f / (1.0f + dot));

        sum += dampedAmplitude * n * (1.0f / (1.0f + dot));

        amplitude *= std::lerp(currentGain, currentGain * smoothstep(0.0f, 1.0f, sum), altitudeErode);

        ridgeErosionDerSum = XMFLOAT2((ridgeErosionDerSum.x + dx_n) * ridgeErode, (ridgeErosionDerSum.y + dz_n) * ridgeErode);
        float dotRidge = ridgeErosionDerSum.x * ridgeErosionDerSum.x + ridgeErosionDerSum.y * ridgeErosionDerSum.y;
        dampedAmplitude = amplitude * (1.0f - (ridgeErode / (1.0f + dotRidge)));

        x = (x0 * freq) + perturbDerSum.x;
        z = (z0 * freq) + perturbDerSum.y;
        perturbDerSum = XMFLOAT2((perturbDerSum.x + dx_n) * perturbAmt, (perturbDerSum.y + dz_n) * perturbAmt);

        currentGain = gain + amplify;
        freq *= lacunarity;
    }

    // Normalize to [-1,1]
    float hNorm = sum / sumAmp;
    return hNorm;
    //return 0.5f * (hNorm + 1.0f);
    //return sum;
}

float TerrainChunk::smoothstep(float edge0, float edge1, float x) {
    // Scale, bias and saturate x to 0..1 range
    x = std::clamp((x - edge0) / (edge1 - edge0), 0.0f, 1.0f);
    // Evaluate polynomial
    return x * x * (3 - 2 * x);
}