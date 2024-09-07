cmake_minimum_required(VERSION 3.8)

# サポートされている場合は、MSVC コンパイラのホット リロードを有効にします。
if (POLICY CMP0141)
  cmake_policy(SET CMP0141 NEW)
  set(CMAKE_MSVC_DEBUG_INFORMATION_FORMAT "$<IF:$<AND:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>,$<$<CONFIG:Debug,RelWithDebInfo>:EditAndContinue>,$<$<CONFIG:Debug,RelWithDebInfo>:ProgramDatabase>>")
endif()

project("rtcamp10")
set(CMAKE_CXX_STANDARD 20)

add_subdirectory(external/DirectX-Headers)

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

add_executable(${PROJECT_NAME} ${SOURCES})

include_directories(include)

target_compile_definitions(${PROJECT_NAME} PRIVATE
                           RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
                           )

# シェーダーコンパイルのためのDLLへのパス
set(DXCOMPILER_LIB_PATH "${CMAKE_SOURCE_DIR}/external/libs/dxcompiler.lib")
set(DXCOMPILER_DLL_PATH "${CMAKE_SOURCE_DIR}/external/libs/dxcompiler.dll")
target_link_libraries(${PROJECT_NAME} PRIVATE "${DXCOMPILER_LIB_PATH}")

# 実行時にライブラリをコピーする
add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXCOMPILER_DLL_PATH}"
        $<TARGET_FILE_DIR:${PROJECT_NAME}>)