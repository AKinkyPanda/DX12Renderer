#include "ObjLoader.h"
#define TINYOBJLOADER_IMPLEMENTATION
#include "tiny_obj_loader.h"
#include "Mesh.h"
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include "Texture.h"
#include <iostream>

using namespace DirectX;

constexpr int8_t POSITION_SIZE = 3;
constexpr int8_t NORMAL_SIZE = 3;
constexpr int8_t UV_SIZE = 2;

const std::vector<uint8_t> errorTexture =
{
	128, 0, 128, 0,
	 0,  0,  0,  0,
	 0,  0,  0,  0,
	128, 0, 128, 0,
};

const std::string ROOT = "./";

std::unordered_map<std::string, const Texture*> m_textures = std::unordered_map<std::string, const Texture*>();

tinyobj::ObjReaderConfig GetConfig();
XMFLOAT3 GetPositionData(const tinyobj::attrib_t& attributes, const tinyobj::index_t& index);
XMFLOAT3 CoordinateSpaceConversion(const XMFLOAT3& _xyzVector);
XMFLOAT3 GetNormalData(const tinyobj::attrib_t& attributes, const tinyobj::index_t& index);
XMFLOAT2 GetUVData(const tinyobj::attrib_t& attributes, const tinyobj::index_t& index);
std::vector<int32_t> CompressMaterialIndices(const std::vector<int32_t>& materialIndices);
const Texture* LoadTexture(const std::string& filePath);
void UploadTexture(const std::string& filePath, const std::vector<uint8_t>& data, const XMFLOAT2& imageSize);
bool DoesFileExist(const std::string& filePath);


std::vector<Mesh> LoadObjModel(const std::string& filePath)
{
	std::vector<Mesh> meshes = std::vector<Mesh>();
	tinyobj::ObjReader fileReader = tinyobj::ObjReader();
	if (fileReader.ParseFromFile(filePath, GetConfig())) {
		if (!fileReader.Warning().empty()) {
			//spdlog::warn("[OBJ Loader]: Warning for object '{}': {}", filePath, fileReader.Warning());
			std::cout << fileReader.Warning() << "\n";
		}

		auto& attributes = fileReader.GetAttrib();
		auto& shapes = fileReader.GetShapes();
		auto& materials = fileReader.GetMaterials();

		meshes = std::vector<Mesh>(shapes.size());
		std::vector<const Texture*> textures = std::vector<const Texture*>(materials.size());

		for (std::size_t i = 0; i < textures.size(); i++)
		{
			tinyobj::material_t material = materials[i];
			textures[i] = LoadTexture(material.diffuse_texname);
		}

		for (size_t i = 0; i < shapes.size(); i++) {
			Mesh mesh = Mesh();
			std::vector<VertexPosColor> vertices = std::vector<VertexPosColor>(shapes[i].mesh.indices.size());
			std::vector<UINT> indices = std::vector<UINT>(vertices.size());

			for (size_t j = 0; j < vertices.size(); j++) {
				tinyobj::index_t index = shapes[i].mesh.indices[j];
				indices[j] = static_cast<UINT>(j);

				if (index.vertex_index >= 0) {
					vertices[j].SetPosition(GetPositionData(attributes, index));
				}

				if (index.normal_index >= 0) {
					vertices[j].SetNormal(GetNormalData(attributes, index));
				}

				if (index.texcoord_index >= 0) {
					vertices[j].SetTexCoord(GetUVData(attributes, index));
				}
			}

			std::unordered_map<std::string, Texture*> texture;

			if (!textures.empty()) {
				std::vector<int32_t> materialIndices = CompressMaterialIndices(shapes[i].mesh.material_ids);
				texture = std::unordered_map<std::string, Texture*>(materialIndices.size());
				for (size_t index = 0; index < materialIndices.size(); index++)
				{
					texture.insert(std::make_pair("diffuse", const_cast<Texture*>(textures[materialIndices[index]])));
				}
			}

			mesh.AddVertexData(vertices);
			mesh.AddIndexData(indices);
			if (!textures.empty())
				mesh.AddTextureData(texture);
			mesh.CreateBuffers();

			meshes[i] = mesh;
		}
	}
	else {
		if (!fileReader.Error().empty()) {
			//spdlog::error("[OBJ Loader]: '{}': {}", filePath, fileReader.Error());
			std::cout << fileReader.Error();
		}
	}

	return meshes;
}

tinyobj::ObjReaderConfig GetConfig() {
	tinyobj::ObjReaderConfig config = tinyobj::ObjReaderConfig();
	return config;
}

XMFLOAT3 GetPositionData(const tinyobj::attrib_t& attributes, const tinyobj::index_t& index) {
	XMFLOAT3 vertexData = XMFLOAT3();

	// Assuming POSITION_SIZE is 3, we fill vertexData.x, vertexData.y, and vertexData.z.
	vertexData.x = attributes.vertices[POSITION_SIZE * static_cast<size_t>(index.vertex_index)];
	vertexData.y = attributes.vertices[POSITION_SIZE * static_cast<size_t>(index.vertex_index) + 1];
	vertexData.z = attributes.vertices[POSITION_SIZE * static_cast<size_t>(index.vertex_index) + 2];

	return vertexData;
	//return CoordinateSpaceConversion(vertexData);
}

XMFLOAT3 CoordinateSpaceConversion(const XMFLOAT3& _xyzVector)
{
	const XMFLOAT3 xzyVector = XMFLOAT3(_xyzVector.x, -_xyzVector.z, _xyzVector.y);
	return xzyVector;
}

XMFLOAT3 GetNormalData(const tinyobj::attrib_t& attributes, const tinyobj::index_t& index) {
	XMFLOAT3 vertexData = XMFLOAT3();

	vertexData.x = attributes.normals[NORMAL_SIZE * static_cast<size_t>(index.normal_index)];
	vertexData.y = attributes.normals[NORMAL_SIZE * static_cast<size_t>(index.normal_index) + 1];
	vertexData.z = attributes.normals[NORMAL_SIZE * static_cast<size_t>(index.normal_index) + 2];

	return vertexData;
}

XMFLOAT2 GetUVData(const tinyobj::attrib_t& attributes, const tinyobj::index_t& index) {
	XMFLOAT2 vertexData = XMFLOAT2();

	vertexData.x = attributes.texcoords[UV_SIZE * static_cast<size_t>(index.texcoord_index)];
	vertexData.y = -attributes.texcoords[UV_SIZE * static_cast<size_t>(index.texcoord_index) + 1];
	return vertexData;
}

std::vector<int32_t> CompressMaterialIndices(const std::vector<int32_t>& materialIndices) {
	std::vector<int32_t> previousIndices = std::vector<int32_t>();
	previousIndices.reserve(materialIndices.size());
	for (size_t materialIndexIndex = 0; materialIndexIndex < materialIndices.size(); materialIndexIndex++) {
		bool presentInList = false;
		for (uint32_t previousIndicesIndex = 0; previousIndicesIndex < previousIndices.size() && !presentInList; previousIndicesIndex++) {
			presentInList = materialIndices[materialIndexIndex] == previousIndices[previousIndicesIndex] || presentInList;
		}
		if (!presentInList) {
			previousIndices.push_back(materialIndices[materialIndexIndex]);
		}
	}
	return previousIndices;
}

const Texture* LoadTexture(const std::string& filePath)
{
	const Texture* texture = nullptr;
	const auto textureInMap = m_textures.find(filePath);

	if (textureInMap == m_textures.end())
	{
		// Go to project settings and fix this !!!
		if (DoesFileExist("../../Assets/Models/crytek-sponza/" + filePath))
		{
			int width, height, channels = 0;
			std::vector<uint8_t> buffer = ReadBinaryFile("D:/Git/DX12Renderer/Assets/Models/crytek-sponza/" + filePath);
			unsigned char* data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, &channels, 4);
			std::vector<uint8_t> imageData = std::vector<uint8_t>(data, data + width * height * 4);
			texture = new Texture(filePath, imageData, XMFLOAT2(width, height));
			buffer.clear();
			imageData.clear();
			buffer.shrink_to_fit();
			imageData.shrink_to_fit();
			stbi_image_free(data);
			m_textures.emplace(std::make_pair(texture->GetPath(), texture));
		}
		else
		{
			UploadTexture(filePath, errorTexture, XMFLOAT2(2, 2));
			texture = m_textures.find(filePath)->second;
			//spdlog::error("[Asset Manager]: image '{}' does not exist.", filePath.c_str());
		}
	}
	else
	{
		texture = textureInMap->second;
	}

	return texture;
}

void UploadTexture(const std::string& filePath, const std::vector<uint8_t>& data, const XMFLOAT2& imageSize)
{
	const auto textureInMap = m_textures.find(filePath);
	if (textureInMap == m_textures.end())
	{
		m_textures.emplace(std::make_pair(filePath, new Texture(filePath, data, imageSize)));
	}
}

bool DoesFileExist(const std::string& filePath)
{
	std::ifstream readOnlyFile = std::ifstream();
	readOnlyFile.open(filePath);
	const bool isOpen = readOnlyFile.is_open();
	readOnlyFile.close();
	return isOpen;
}

std::vector<uint8_t> ReadBinaryFile(const std::string& _filePath)
{
	// Open file
	std::vector<uint8_t> readBuffer = std::vector<uint8_t>(0);
	std::ifstream binaryFile = std::ifstream();
	binaryFile.open(_filePath, std::ios::binary);

	if (binaryFile.is_open())
	{
		// Get file size
		binaryFile.seekg(0, std::ios::end);
		const long long fileSize = binaryFile.tellg();
		binaryFile.seekg(0, std::ios::beg);

		// Read binary file
		readBuffer = std::vector<uint8_t>(static_cast<size_t>(fileSize));
		binaryFile.read(reinterpret_cast<char*>(&*readBuffer.begin()), static_cast<std::streamsize>(fileSize));

		// Clean up
		binaryFile.close();
	}
	else
	{
		//spdlog::warn("[FileIO]: Could not read file at '{}'", _filePath.c_str());
	}

	return readBuffer;
}

const Texture* LoadTextureIndependant(const std::string& filePath)
{
	const Texture* texture = nullptr;

	// Go to project settings and fix this !!!
	if (DoesFileExist(filePath))
	{
		int width, height, channels = 0;
		std::vector<uint8_t> buffer = ReadBinaryFile(filePath);
		unsigned char* data = stbi_load_from_memory(buffer.data(), static_cast<int>(buffer.size()), &width, &height, &channels, 4);
		std::vector<uint8_t> imageData = std::vector<uint8_t>(data, data + width * height * 4);
		texture = new Texture(filePath, imageData, XMFLOAT2(width, height));
		buffer.clear();
		imageData.clear();
		buffer.shrink_to_fit();
		imageData.shrink_to_fit();
		stbi_image_free(data);
	}
	else
	{
		UploadTexture(filePath, errorTexture, XMFLOAT2(2, 2));
		texture = m_textures.find(filePath)->second;
		//spdlog::error("[Asset Manager]: image '{}' does not exist.", filePath.c_str());
	}

	return texture;
}