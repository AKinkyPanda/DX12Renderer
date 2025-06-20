#include "TerrainChunk.h"
#include <algorithm>
#include "DX12Renderer/Texture.h"

TerrainChunk::TerrainChunk(int chunkX, int chunkZ, int size, float heightScale)
    : m_chunkX(chunkX), m_chunkZ(chunkZ), m_size(size), m_heightScale(heightScale) {}

void TerrainChunk::Initialize(const std::vector<float>& /*unused global heightmap*/, int /*heightmapWidth*/, std::unordered_map<std::string, Texture*>& textures) {
    int tessFactor = 4;
    int vertsPerSide = m_size / tessFactor; // 256
    int patchesPerSide = vertsPerSide - 1;  // 255
    int visibleSize = patchesPerSide * tessFactor; // 1020
    int arrSize = vertsPerSide * vertsPerSide; // 256*256

    // Generate local noise-based heightmap
    std::vector<float> heightmapLocal = GenerateChunkHeightmap2(
        FastNoiseLite::NoiseType_Perlin,
        m_chunkX, m_chunkZ,
        vertsPerSide, vertsPerSide,  // width=vertsPerSide
        0.1f
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

            //float base = SampleNoiseWithSlope(nx, nz, 1e-3f, 0.9f);
            //float base = FbmNoiseWithFD(nx, nz, 8, 2, 0.5, 1e-3f, 0.5f);
            //float base = FbmNoiseWithFD(nx, nz, 8, 2, 0.5, 1e-3f, 0.5f, 1.0f, 0.01f, 0.0f, 0.1f, false);
            float base = FbmNoiseWithFD2(nx, nz, 8, 1.8f, 0.5f, 1e-3f, /*kAtten*/ 0.5f);
            //float base = m_Noise.GetNoise(nx, nz);

            heightmapLocal[z * vertsPerSide + x] = base;

            //heightmapLocal[z * vertsPerSide + x] = m_Noise.GetNoise(nx, nz);
        }
    }
    return heightmapLocal;
}

std::vector<float> TerrainChunk::GenerateChunkHeightmap2(
    FastNoiseLite::NoiseType noiseType,
    int chunkX, int chunkZ,
    int vertsPerSide,    // = m_size / tessFactor = 256
    int UNUSED_height,   // same as width
    float noiseScale)    // maps world units into noise space
{
    // 1. Configure m_Noise for plain Perlin (or Value) noise, no built-in fractal.
    m_Noise.SetNoiseType(noiseType);
    // We will handle frequency by scaling the sample coordinates ourselves.
    // Therefore set the internal frequency to 1.0 so GetNoise(x, z) ≈ noise(x, z).
    m_Noise.SetFrequency(noiseScale);
    m_Noise.SetFractalType(FastNoiseLite::FractalType_None);

    int tessFactor = 4;
    int patchesPerSide = vertsPerSide - 1; // 255
    int visibleSize = patchesPerSide * tessFactor; // 1020

    std::vector<float> heightmapLocal(vertsPerSide * vertsPerSide);

    // === Parameters for layering ===
    // 2. Mask parameters: low-frequency mask to select mountain zones
    const float maskFreq = 0.75f;    // relative to noise-space: adjust to world scale
    const float maskPower = 2.0f;     // raise [0,1] mask to this power to concentrate peaks
    const float maskThreshold = 0.0f; // optional: below this, treat as flat (0). Set <=0 to disable threshold.

    // 3. Base shape fBm parameters (low-frequency silhouette of each mountain)
    const int baseOctaves = 3;
    const float baseLacunarity = 2.0f;
    const float baseGain = 0.4f;
    const float baseAmplitudeScale = 100.0f;
    //    baseAmplitudeScale is roughly the max height of mountains from base shape (before detail).
    //    Tune: larger -> taller mountains; if too tall relative to detail, adjust.

    // 4. Detail fBm parameters: slope-attenuated fBm only in masked areas
    const int detailOctaves = 6;
    const float detailLacunarity = 2.0f;
    const float detailGain = 0.5f;
    const float detailAmpScale = 60.0f;
    //    detailAmpScale: maximum magnitude of roughness added atop base shape.

    const float eps = 1e-3f;   // for finite differences in FbmNoiseWithFD2
    const float kAtten = 1.5f; // attenuation strength for slope-based erosion

    // Precompute normalization sums for base fBm (no attenuation here)
    float baseSumAmp = 0.0f;
    {
        float amp = 1.0f;
        for (int i = 0; i < baseOctaves; ++i) {
            baseSumAmp += amp;
            amp *= baseGain;
        }
    }
    // detail fBm uses your FbmNoiseWithFD2 which itself normalizes internally by its sumAmp.

    // Loop over each vertex in the chunk
    for (int z = 0; z < vertsPerSide; ++z) {
        for (int x = 0; x < vertsPerSide; ++x) {
            int idx = z * vertsPerSide + x;
            // Compute world-space position
            float worldX = float(chunkX * visibleSize + x * tessFactor);
            float worldZ = float(chunkZ * visibleSize + z * tessFactor);
            // Map to noise-space
            float nx = worldX * noiseScale;
            float nz = worldZ * noiseScale;

            // === 1. Low-frequency mask ===
            // Sample noise at very low frequency to decide mountain zones
            float rawMask = m_Noise.GetNoise(nx * maskFreq, nz * maskFreq); // in [-1,1]
            float mask01 = rawMask * 0.5f + 0.5f;                             // [0,1]
            float maskPow = std::pow(mask01, maskPower);                     // concentrate peaks
            if (maskThreshold > 0.0f && maskPow < maskThreshold) {
                maskPow = 0.0f;
            }
            // If maskPow == 0, we'll produce flat or slight rolling terrain below.

            // === Base mountain shape ===
            float baseShape = 0.0f;
            if (maskPow > 0.0f) {
                // Simple low-frequency fBm:
                float freq = 1.0f;
                float amp = 1.0f;
                float sum = 0.0f;
                for (int o = 0; o < baseOctaves; ++o) {
                    float sample = m_Noise.GetNoise(nx * freq, nz * freq); // [-1,1]
                    sum += sample * amp;
                    freq *= baseLacunarity;
                    amp *= baseGain;
                }
                // Normalize to [-1,1]
                float baseFbmNorm = sum / baseSumAmp;
                // Scale to desired height and apply mask
                baseShape = baseFbmNorm * baseAmplitudeScale * maskPow;
            }
            else {
                // Optional slight rolling terrain on plains:
                // e.g., a very low-amplitude noise so plains aren’t perfectly flat.
                const float plainsFreq = 0.1f;
                const float plainsAmp = 2.0f;
                float plains = m_Noise.GetNoise(nx * plainsFreq, nz * plainsFreq) * plainsAmp;
                baseShape = plains * (1.0f - maskPow); // maskPow==0 => full plains; maskPow>0 => reduce plains under mountains
            }

            // === Detail shape (slope-attenuated fBm) ===
            float detailShape = 0.0f;
            if (maskPow > 0.0f) {
                float detailFbmNorm = FbmNoiseWithFD2(nx, nz,
                    detailOctaves,
                    detailLacunarity,
                    detailGain,
                    eps,
                    kAtten);
                // Scale detail and apply mask so that detail fades near mountain edges
                detailShape = detailFbmNorm * detailAmpScale * maskPow;
            }
            // else detailShape remains 0

            float tinyShape = 0.0f;
            if (maskPow > 0.0f) {
                // Call your existing FbmNoiseWithFD2 which returns normalized [-1,1]
                float detailFbmNorm = FbmNoiseWithFD2(nx, nz,
                    8,
                    2.5,
                    0.6f,
                    eps,
                    kAtten);
                // Scale detail and apply mask so that detail fades near mountain edges
                tinyShape = detailFbmNorm * 40 * maskPow;
            }

            float minisculeShape = 0.0f;
            if (maskPow > 0.0f) {
                // Call your existing FbmNoiseWithFD2 which returns normalized [-1,1]
                float detailFbmNorm = FbmNoiseWithFD2(nx, nz,
                    10,
                    3.0f,
                    0.8f,
                    eps,
                    kAtten);
                // Scale detail and apply mask so that detail fades near mountain edges
                minisculeShape = detailFbmNorm * 20 * maskPow;
            }

            // === 4. Combine base shape + detail ===
            float finalH = baseShape + detailShape + tinyShape + minisculeShape;

            // === 5. Normalize/mapping for heightmapLocal ===
            // We want heightmapLocal in [-1,1], since existing code does (val+1)*0.5 * heightScale for vertex Y.
            // finalH is in world units; we need to remap it into [-1,1] relative to some maxHeight.
            // Choose a maxPossibleHeight = baseAmplitudeScale + detailAmpScale (worst-case). You can adjust or compute empirically.
            float maxPossible = baseAmplitudeScale + detailAmpScale;
            // Map finalH from [-maxPossible, +maxPossible] to [-1,1]:
            float hNorm = finalH / maxPossible;
            // Clamp to [-1,1]
            hNorm = std::clamp(hNorm, -1.0f, 1.0f);

            heightmapLocal[idx] = hNorm;
        }
    }

    return heightmapLocal;
}

float TerrainChunk::SampleNoiseWithSlope(float x, float z, float eps, float exaggeration) {
    float n0 = m_Noise.GetNoise(x, z);
    float nx = m_Noise.GetNoise(x + eps, z);
    float nz = m_Noise.GetNoise(x, z + eps);
    float dx = (nx - n0) / eps;
    float dz = (nz - n0) / eps;
    float slope = sqrt(dx * dx + dz * dz);
    return n0 + slope * exaggeration;
}

float TerrainChunk::FbmNoiseWithFD(float x0, float z0, int octaves, float lac, float gain, float eps, float kAtten) {
    float h = 0, amplitude = 1, freq = 1;
    for (int i = 0; i < octaves; i++) {
        float x = x0 * freq, z = z0 * freq;
        float n = m_Noise.GetNoise(x, z);
        // finite difference derivative
        float nx = m_Noise.GetNoise(x + eps, z);
        float nz = m_Noise.GetNoise(x, z + eps);
        float dx = (nx - n) / eps;
        float dz = (nz - n) / eps;
        float slope = std::sqrt(dx * dx + dz * dz) * freq;
        float attenuation = std::exp(-kAtten * slope);
        h += n * amplitude;
        amplitude *= gain * attenuation;
        freq *= lac;
        if (amplitude < 1e-5f) break;
    }
    return h;
}

//float TerrainChunk::FbmNoiseWithFD2(
//    float x0, float z0,
//    int octaves,
//    float lac, float gain,
//    float eps,
//    float kAtten  // if you want attenuation; otherwise set kAtten=0
//) {
//    // Prepare theoretical sumAmp for normalization
//    float sumAmp = 0.0f;
//    {
//        float ampTmp = 1.0f;
//        for (int i = 0; i < octaves; i++) {
//            sumAmp += ampTmp;
//            ampTmp *= gain;
//        }
//    }
//
//    float h = 0.0f;
//    float amplitude = 1.0f;
//    float freq = 1.0f;
//
//    for (int i = 0; i < octaves; i++) {
//        float x = x0 * freq;
//        float z = z0 * freq;
//
//        // Sample noise at base frequency (m_Noise.SetFrequency was set once before)
//        float n = m_Noise.GetNoise(x, z); // in [-1,1]
//
//        // Finite-difference derivative if attenuation wanted
//        float slope = 0.0f;
//        if (kAtten > 0.0f) {
//            float nx = m_Noise.GetNoise(x + eps, z);
//            float nz = m_Noise.GetNoise(x, z + eps);
//            float dx = (nx - n) / eps;
//            float dz = (nz - n) / eps;
//            slope = std::sqrt(dx * dx + dz * dz) * freq;
//        }
//
//        float attenuation = 1.0f;
//        if (kAtten > 0.0f) {
//            attenuation = std::exp(-kAtten * slope);
//            // optionally clamp a small minAtt here if desired, but you said remove those
//        }
//
//        h += n * amplitude;
//        amplitude *= gain * attenuation;
//        freq *= lac;
//    }
//
//    // Normalize to [-1,1]
//    float hNorm = h / sumAmp;
//    return hNorm;
//}

float TerrainChunk::FbmNoiseWithFD2(
    float x0, float z0,
    int octaves,
    float lac,
    float gain,
    float eps,
    float kAtten
) {
    // 1. Theoretical amplitude sum (no attenuation) for normalization
    float sumAmp = 0.0f;
    {
        float ampTmp = 1.0f;
        for (int i = 0; i < octaves; i++) {
            sumAmp += ampTmp;
            ampTmp *= gain;
        }
    }

    float h = 0.0f;
    float amplitude = 1.0f;
    float freq = 1.0f;

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
        h += n * amplitude;

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
    float hNorm = h / sumAmp;
    return hNorm;
}

float TerrainChunk::FbmNoiseWithFD(
    float x0, float z0,
    int octaves,
    float lac, float gain,
    float eps,
    float kAtten,
    float minAtt,            // new: minimum attenuation (e.g. 0.2f)
    float maskFreq,          // new: frequency for mountain mask (e.g. 0.01f)
    float maskPower,         // new: exponent to sharpen mask (e.g. 3.0f)
    float maskThreshold,     // new: optional cutoff in [0,1], e.g. 0.1f (set <=0 to disable)
    bool useAltAttenuation   // new: if true, use 1/(1 + kAtten*slope) instead of exp(-kAtten*slope)
) {
    // 1. Compute low-frequency mask
    float mask = 1.0f;
    if (maskFreq > 0.0f) {
        float raw = m_Noise.GetNoise(x0 * maskFreq, z0 * maskFreq); // in [-1,1]
        float raw01 = raw * 0.5f + 0.5f;                             // [0,1]
        // apply power to concentrate peaks
        mask = std::pow(raw01, maskPower);
        // optional threshold: flatten small mask values
        if (maskThreshold > 0.0f && mask < maskThreshold) {
            mask = 0.0f;
        }
    }
    // If mask is zero, optionally return flat noise or zero:
    if (mask <= 0.0f) {
        // Option A: purely flat (zero)
        // return 0.0f;
        // Option B: slight rolling terrain: low-frequency noise
        float flatFreq = maskFreq * 2.0f; // or some other
        float flatAmp = 0.1f;             // small amplitude
        return m_Noise.GetNoise(x0 * flatFreq, z0 * flatFreq) * flatAmp;
    }

    // 2. fBm with finite-difference derivatives and attenuation clamped
    float h = 0.0f;
    float amplitude = 1.0f;
    float freq = 1.0f;

    for (int i = 0; i < octaves; i++) {
        float x = x0 * freq;
        float z = z0 * freq;

        float frequency = 0.04f - i * 0.01f;
        if (frequency < 0.01f)
            frequency = 0.01f;

        m_Noise.SetFrequency(frequency);
        float n = m_Noise.GetNoise(x, z); // in [-1,1]

        // Finite-difference derivative
        float nx = m_Noise.GetNoise(x + eps, z);
        float nz = m_Noise.GetNoise(x, z + eps);
        float dx = (nx - n) / eps;
        float dz = (nz - n) / eps;
        // slope magnitude in base coord: derivative wrt base multiplied by freq
        float slope = std::sqrt(dx * dx + dz * dz) * freq;

        // Attenuation: decreasing function of slope, but clamped to minAtt
        float attenuation;
        if (useAltAttenuation) {
            attenuation = 1.0f / (1.0f + kAtten * slope);
        }
        else {
            attenuation = std::exp(-kAtten * slope);
        }
        // clamp/blend so attenuation >= minAtt
        //attenuation = std::max(minAtt, attenuation);
        if (minAtt > attenuation)
            attenuation = minAtt;

        h += n * amplitude;

        amplitude *= gain * attenuation;
        freq *= lac;
        if (amplitude < 1e-5f) break;
    }

    // 3. Optionally normalize or remap `h` here if desired.
    // Raw h is roughly in [-sumAmp, sumAmp], where sumAmp depends on gain/lacunarity etc.
    // You might want to scale it to [0,1] or to a desired height range:
    // float normalized = (h * 0.5f) + 0.5f; // if h in [-1,1], but here range may differ.
    // For simplicity, we return raw h and apply scaling outside.

    // 4. Apply mask to height
    return h * mask;
}