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

#pragma comment(lib, "dxcompiler.lib")

using Microsoft::WRL::ComPtr;
namespace fs = std::filesystem;

/// <summary>
///  シェーダーコンパイル
/// </summary>
/// <param name="hlslFilePath">HLSLのファイルパス</param>
/// <returns>コンパイル結果</returns>
std::vector<char> inline CompileShaderLibrary(const fs::path& hlslFilePath)
{
    Print(PrintInfoType::RTCAMP10, "シェーダーコンパイル 開始");
    // シェーダーロード
    std::ifstream file(hlslFilePath);
    if (!file.is_open()) {
        Error(PrintInfoType::RTCAMP10, "シェーダーの読み込みに失敗しました :", hlslFilePath);
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::string shaderSource(size, ' ');
    file.seekg(0);
    file.read(shaderSource.data(), size);
    std::wstring fileName = hlslFilePath.filename().wstring();

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
#if _DEBUG
    args.emplace_back(L"/0d");
#endif
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

/// <summary>
/// コンパイル済みのシェーダーライブラリをロード
/// </summary>
/// <param name="shaderLibPath">シェーダーライブラリへのパス</param>
/// <returns>シェーダーライブラリ</returns>
std::vector<char> inline LoadPreCompiledShaderLibrary(const fs::path& shaderLibPath)
{
    // シェーダーロード
    std::ifstream file(shaderLibPath);
    if (!file.is_open()) {
        Error(PrintInfoType::RTCAMP10, "シェーダーライブラリの読み込みに失敗しました :", shaderLibPath);
        throw std::runtime_error("");
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::vector<char> shaderSource;
    file.seekg(0);
    file.read(shaderSource.data(), size);
    return shaderSource;
}