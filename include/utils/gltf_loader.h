#pragma once

#include <filesystem>
#include <fstream>
#include "print_util.h"

#define TINYGLTF_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "tiny_gltf.h"

namespace fs = std::filesystem;

bool inline LoadGLTF(const std::wstring& fileName)
{
    using namespace tinygltf;
    Model model;
    TinyGLTF loader;
    std::string err;
    std::string warn;
    const fs::path gltfPath{ RESOURCE_DIR L"/scene/" + fileName + L".gltf" };
    bool result = loader.LoadASCIIFromFile(&model, &err, &warn, gltfPath.string());
    if (!warn.empty())
    {
        Error(PrintInfoType::RTCAMP10, "GLTFファイルの読み込み中の警告 :", warn);
    }
    if (!err.empty())
    {
        Error(PrintInfoType::RTCAMP10, "GLTFファイルの読み込み中のエラー :", err);
    }
    if (!result) {
        return false;
    }
    return true;
}