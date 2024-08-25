#pragma once

#include <iostream>
#include <string>

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

void inline Print(const PrintInfoType info_type, const char* message) {
    std::cout << "[" << GetInfoTypeStr(info_type) << "] " << message << std::endl;
}

template<typename Any>
void inline Print(const PrintInfoType info_type, const char* message, Any any) {
    std::cout << "[" << GetInfoTypeStr(info_type) << "] " << message << any << std::endl;
}

void inline Error(const PrintInfoType info_type, const char* message) {
    std::cerr << "[" << GetInfoTypeStr(info_type) << "] " << message << std::endl;
}

template<typename Any>
void inline Error(const PrintInfoType info_type, const char* message, Any any) {
    std::cerr << "[" << GetInfoTypeStr(info_type) << "] " << message << any << std::endl;
}