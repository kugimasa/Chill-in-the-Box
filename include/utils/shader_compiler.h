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
///  �V�F�[�_�[�R���p�C��
/// </summary>
/// <param name="hlslFilePath">HLSL�̃t�@�C���p�X</param>
/// <returns>�R���p�C������</returns>
std::vector<char> inline CompileShaderLibrary(const fs::path& hlslFilePath)
{
    Print(PrintInfoType::RTCAMP10, "�V�F�[�_�[�R���p�C�� �J�n");
    // �V�F�[�_�[���[�h
    std::ifstream file(hlslFilePath);
    if (!file.is_open()) {
        Error(PrintInfoType::RTCAMP10, "�V�F�[�_�[�̓ǂݍ��݂Ɏ��s���܂��� :", hlslFilePath);
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

    // �R���p�C��������
    std::vector<LPCWSTR> args;
#if _DEBUG
    args.emplace_back(L"/0d");
#endif
    // �V�F�[�_�[�R���p�C��
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
        Error(PrintInfoType::RTCAMP10, "�R���p�C���̎��s�Ɏ��s���܂��� :", hlslFilePath);
    }
    pDxcResult->GetStatus(&hr);
    if (FAILED(hr))
    {
        ComPtr<IDxcBlobEncoding> pErrBlob;
        hr = pDxcResult->GetErrorBuffer(&pErrBlob);
        if (FAILED(hr))
        {
            Error(PrintInfoType::RTCAMP10, "�R���p�C���G���[�̎擾�Ɏ��s���܂��� :", hlslFilePath);
        }
        // �R���p�C���G���[�̏o��
        auto size = pErrBlob->GetBufferSize();
        std::vector<char> infoLog(size + 1);
        memcpy(infoLog.data(), pErrBlob->GetBufferPointer(), size);
        infoLog.back() = 0;
        OutputDebugStringA(infoLog.data());
        Error(PrintInfoType::RTCAMP10, "�V�F�[�_�[�R���p�C���Ɏ��s���܂��� :", hlslFilePath);
    }

    ComPtr<IDxcBlob> pBlob;
    hr = pDxcResult->GetResult(&pBlob);
    if (FAILED(hr)) {
        Error(PrintInfoType::RTCAMP10, "�V�F�[�_�[�R���p�C���̌��ʂ̎擾�Ɏ��s���܂���", hlslFilePath);
    }
    std::vector<char> result;
    auto blobSize = pBlob->GetBufferSize();
    result.resize(blobSize);
    memcpy(result.data(), pBlob->GetBufferPointer(), blobSize);
    return result;
}

/// <summary>
/// �R���p�C���ς݂̃V�F�[�_�[���C�u���������[�h
/// </summary>
/// <param name="shaderLibPath">�V�F�[�_�[���C�u�����ւ̃p�X</param>
/// <returns>�V�F�[�_�[���C�u����</returns>
std::vector<char> inline LoadPreCompiledShaderLibrary(const fs::path& shaderLibPath)
{
    // �V�F�[�_�[���[�h
    std::ifstream file(shaderLibPath);
    if (!file.is_open()) {
        Error(PrintInfoType::RTCAMP10, "�V�F�[�_�[���C�u�����̓ǂݍ��݂Ɏ��s���܂��� :", shaderLibPath);
        throw std::runtime_error("");
    }
    file.seekg(0, std::ios::end);
    size_t size = file.tellg();
    std::vector<char> shaderSource;
    file.seekg(0);
    file.read(shaderSource.data(), size);
    return shaderSource;
}