#pragma once

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <wrl.h>
#include <d3d12.h>
#include <dxcapi.h>

#include "print_util.h"

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

#ifdef _DEBUG
/// <summary>
///  シェーダーコンパイル
/// </summary>
/// <param name="hlslFilePath">HLSLのファイルパス</param>
/// <returns>コンパイル結果</returns>
std::vector<char> inline CompileShaderLibrary(const fs::path& hlslFilePath)
{
    Print(PrintInfoType::RTCAMP10, "シェーダーコンパイル 開始");
    // シェーダーロード
    std::ifstream file(hlslFilePath, std::ifstream::binary);
    if (!file.is_open()) {
        Error(PrintInfoType::RTCAMP10, "シェーダーの読み込みに失敗しました :", hlslFilePath);
    }

    std::wstring fileName = hlslFilePath.filename().wstring();
    std::stringstream strStream;
    strStream << file.rdbuf();
    std::string shaderSource = strStream.str();

    ComPtr<IDxcLibrary> pDxcLibrary;
    DxcCreateInstance(CLSID_DxcLibrary, IID_PPV_ARGS(&pDxcLibrary));
    ComPtr<IDxcCompiler> pDxcCompiler;
    DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&pDxcCompiler));

    ComPtr<IDxcBlobEncoding> pShaderBlob;
    pDxcLibrary->CreateBlobWithEncodingFromPinned(
        (LPBYTE)shaderSource.c_str(), (UINT32)shaderSource.size(), CP_UTF8, &pShaderBlob
    );
    ComPtr<IDxcIncludeHandler> pIncludeHandler;
    pDxcLibrary->CreateIncludeHandler(&pIncludeHandler);

    // コンパイル時引数
    std::vector<LPCWSTR> args;
    args.emplace_back(L"/0d");
    args.emplace_back(L"-I");
    args.emplace_back(RESOURCE_DIR L"/shader/");
    // シェーダーコンパイル
    const auto entryPoint = L"";
    const auto target = L"lib_6_4";
    ComPtr<IDxcOperationResult> pDxcResult;
    HRESULT hr;
    hr = pDxcCompiler->Compile(
        pShaderBlob.Get(), fileName.c_str(),
        entryPoint, target,
        args.data(), UINT(args.size()),
        nullptr, 0,
        pIncludeHandler.Get(), &pDxcResult
    );
    if (FAILED(hr))
    {
        Error(PrintInfoType::RTCAMP10, "コンパイルの実行に失敗しました :", hlslFilePath);
    }
    pDxcResult->GetStatus(&hr);
    if (FAILED(hr))
    {
        ComPtr<IDxcBlobEncoding> pErrBlob;
        hr = pDxcResult->GetErrorBuffer(&pErrBlob);
        if (FAILED(hr))
        {
            Error(PrintInfoType::RTCAMP10, "コンパイルエラーの取得に失敗しました :", hlslFilePath);
        }
        // コンパイルエラーの出力
        auto size = pErrBlob->GetBufferSize();
        std::vector<char> infoLog(size + 1);
        memcpy(infoLog.data(), pErrBlob->GetBufferPointer(), size);
        infoLog.back() = 0;
        OutputDebugStringA(infoLog.data());
        Error(PrintInfoType::RTCAMP10, "シェーダーコンパイルに失敗しました :", hlslFilePath);
    }

    ComPtr<IDxcBlob> pBlob;
    hr = pDxcResult->GetResult(&pBlob);
    if (FAILED(hr)) {
        Error(PrintInfoType::RTCAMP10, "シェーダーコンパイルの結果の取得に失敗しました", hlslFilePath);
    }
    std::vector<char> result;
    auto blobSize = pBlob->GetBufferSize();
    result.resize(blobSize);
    memcpy(result.data(), pBlob->GetBufferPointer(), blobSize);
    return result;
}
#endif // _DEBUG

/// <summary>
/// コンパイル済みのシェーダーライブラリをロード
/// </summary>
/// <param name="shaderLibPath">シェーダーライブラリへのパス</param>
/// <returns>シェーダーライブラリ</returns>
std::vector<char> inline LoadPreCompiledShaderLibrary(const fs::path& shaderLibPath)
{
    // シェーダーロード
    std::ifstream file(shaderLibPath, std::ios::binary);
    if (!file.is_open()) {
        std::wstring err = L"シェーダーライブラリの読み込みに失敗しました :" + StrToWStr(shaderLibPath.string());
        Error(PrintInfoType::RTCAMP10, err);
        throw std::runtime_error("");
    }
    std::vector<char> shaderSource;
    shaderSource.resize(file.seekg(0, std::ios::end).tellg());
    file.seekg(0, std::ios::beg).read(shaderSource.data(), shaderSource.size());
    return shaderSource;
}

/// <summary>
/// シェーダーセットアップ
/// </summary>
/// <param name="shaderName">シェーダー名</param>
/// <returns></returns>
std::vector<char> inline SetupShader(const std::wstring& shaderName)
{
    std::vector<char> shaderBin;
    // シェーダーロード
#if _DEBUG
    const fs::path shaderPath{ RESOURCE_DIR L"/shader/" + shaderName + L".hlsl" };
    // シェーダーのランタイムコンパイル
    shaderBin = CompileShaderLibrary(shaderPath);
#else
    const fs::path shaderPath{ RESOURCE_DIR L"/shader/" + shaderName + L".dxlib" };
    shaderBin = LoadPreCompiledShaderLibrary(shaderPath);
#endif
    return shaderBin;
}