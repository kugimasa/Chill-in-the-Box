#pragma once

#include <iostream>
#include <string>
#include <windows.h>

#define CURRENT_CP GetConsoleOutputCP()

enum class PrintInfoType {
    D3D12,
    RTCAMP10,
};

std::string inline GetInfoTypeStr(PrintInfoType info_type) {
    switch (info_type) {
    case PrintInfoType::D3D12:
        return "DirectX 12";
    case PrintInfoType::RTCAMP10:
        return "RTCAMP10";
    default:
        return "";
    }
}

std::string inline WStrToStr(const std::wstring& wstr) {
    int size_needed = WideCharToMultiByte(CURRENT_CP, 0, wstr.c_str(), (int)wstr.size(), nullptr, 0, nullptr, nullptr);
    std::string str(size_needed, 0);
    WideCharToMultiByte(CURRENT_CP, 0, wstr.c_str(), (int)wstr.size(), &str[0], size_needed, nullptr, nullptr);
    return str;
}

void inline Print(const PrintInfoType info_type, const char* message) {
    std::cout << "[" << GetInfoTypeStr(info_type) << "] " << message << std::endl;
}

void inline Print(const PrintInfoType info_type, const std::wstring& message) {
    std::cout << "[" << GetInfoTypeStr(info_type) << "] " << WStrToStr(message) << std::endl;
}

template<typename Any>
void inline Print(const PrintInfoType info_type, const char* message, Any any) {
    std::cout << "[" << GetInfoTypeStr(info_type) << "] " << message << any << std::endl;
}

void inline Error(const PrintInfoType info_type, const char* message) {
    std::cerr << "[" << GetInfoTypeStr(info_type) << "] " << message << std::endl;
    throw std::runtime_error("Error");
}

void inline Error(const PrintInfoType info_type, const std::wstring message) {
    std::cerr << "[" << GetInfoTypeStr(info_type) << "] " << WStrToStr(message) << std::endl;
    throw std::runtime_error("Error");
}

template<typename Any>
void inline Error(const PrintInfoType info_type, const char* message, Any any ) {
    std::cerr << "[" << GetInfoTypeStr(info_type) << "] " << message << any << std::endl;
    throw std::runtime_error("Error");
}