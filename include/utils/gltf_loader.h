#pragma once

#include <filesystem>
#include <fstream>

#define TINYGLTF_IMPLEMENTATION
#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#endif
#ifndef STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#endif 
#include "tiny_gltf.h"
#include "utils/print_util.h"


namespace fs = std::filesystem;

bool inline LoadGLTF(const std::wstring& fileName, tinygltf::Model& model)
{
    tinygltf::TinyGLTF loader;
    std::string err;
    std::string warn;
    const fs::path gltfPath{ RESOURCE_DIR L"/scene/" + fileName };
    bool result = false;
    if (gltfPath.extension() == L".gltf")
    {
        result = loader.LoadASCIIFromFile(&model, &err, &warn, gltfPath.string());
    }
    else if (gltfPath.extension() == L".glb")
    {
        result = loader.LoadBinaryFromFile(&model, &err, &warn, gltfPath.string());
    }
    else
    {
        std::wstring errWStr = L"ファイル形式が対応していません：" + std::wstring(gltfPath.extension().c_str());
        Error(PrintInfoType::RTCAMP10, errWStr);
    }
    if (!warn.empty())
    {
        std::wstring errWStr = L"GLTFファイルの読み込み中の警告 :" + StrToWStr(warn);
        Error(PrintInfoType::RTCAMP10, errWStr);
    }
    if (!err.empty())
    {
        std::wstring errWStr = L"GLTFファイルの読み込み中のエラー :" + StrToWStr(err);
        Error(PrintInfoType::RTCAMP10, errWStr);
    }
    return result;
}