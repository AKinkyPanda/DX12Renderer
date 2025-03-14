#pragma once
#include <string>
#include <vector>

class Mesh;
class Texture;

std::vector<Mesh> LoadObjModel(const std::string& filePath);

const Texture* LoadTextureIndependant(const std::string& filePath);

std::vector<uint8_t> ReadBinaryFile(const std::string& _filePath);