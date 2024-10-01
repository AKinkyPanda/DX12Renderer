#pragma once
#include <string>
#include <vector>

class Mesh;

std::vector<Mesh> LoadObjModel(const std::string& filePath);
