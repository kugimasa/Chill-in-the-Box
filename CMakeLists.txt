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

# ImGui
set(IMGUI_SOURCE_DIR ${CMAKE_SOURCE_DIR}/external/imgui)
set(IMGUI_BACKENDS_DIR ${IMGUI_SOURCE_DIR}/backends)
set(IMGUI_SOURCES
    ${IMGUI_SOURCE_DIR}/imgui.cpp
    ${IMGUI_SOURCE_DIR}/imgui_draw.cpp
    ${IMGUI_SOURCE_DIR}/imgui_tables.cpp
    ${IMGUI_SOURCE_DIR}/imgui_widgets.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_dx12.cpp
    ${IMGUI_BACKENDS_DIR}/imgui_impl_win32.cpp
)

add_executable(${PROJECT_NAME} ${SOURCES} ${IMGUI_SOURCES})

include_directories(include)
include_directories(external/DirectX-Headers/include/directx)
include_directories(${IMGUI_SOURCE_DIR})
include_directories(${IMGUI_BACKENDS_DIR})

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(${PROJECT_NAME} PRIVATE
                               RESOURCE_DIR="${CMAKE_CURRENT_SOURCE_DIR}/resources"
                               )
    # シェーダーコンパイルのためのDLLへのパス
    set(DXCOMPILER_LIB_DIR "${CMAKE_SOURCE_DIR}/external/libs/dxcompiler.lib")
    target_link_libraries(${PROJECT_NAME} PRIVATE "${DXCOMPILER_LIB_DIR}")

    # 実行時にライブラリをコピーする
    set(DXCOMPILER_DLL_DIR "${CMAKE_SOURCE_DIR}/external/libs/dxcompiler.dll")
    set(DXIL_DLL_DIR "${CMAKE_SOURCE_DIR}/external/libs/dxil.dll")
    add_custom_command(
        TARGET ${PROJECT_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${DXCOMPILER_DLL_DIR}"
        "${DXIL_DLL_DIR}"
        "${CMAKE_BINARY_DIR}"
    )
else()
    target_compile_definitions(${PROJECT_NAME} PRIVATE
                               RESOURCE_DIR="${CMAKE_BINARY_DIR}/resources"
                               )

    message("シェーダーコンパイル")
    find_program(DXC_COMPILER dxc)
    if(NOT DXC_COMPILER)
        message(FATAL_ERROR "DXCコンパイラが見つかりません")
    endif()
    function(compile_shader hlsl_file output_file profile)
        add_custom_command(
            OUTPUT ${output_file}
            COMMAND ${DXC_COMPILER} -T ${profile} -E main -Fo ${output_file} ${hlsl_file}
            DEPENDS ${hlsl_file}
            COMMENT "HLSLコンパイル: ${hlsl_file}"
            VERBATIM
        )
    endfunction()

    # シェーダーリスト
    set(SHADER_DIR "${CMAKE_SOURCE_DIR}/resources/shader")
    set(HLSL_FILES
        "${SHADER_DIR}/path_tracer.hlsl"
    )

    foreach(HLSL_FILE ${HLSL_FILES})
        get_filename_component(SHADER_NAME ${HLSL_FILE} NAME_WE)
        set(SHADER_OUTPUT "${CMAKE_BINARY_DIR}/resources/shader/${SHADER_NAME}.dxlib")
        compile_shader(${HLSL_FILE} ${SHADER_OUTPUT} "lib_6_4")
        add_custom_target(${SHADER_NAME}_shader ALL DEPENDS ${SHADER_OUTPUT})
        add_dependencies(${PROJECT_NAME} ${SHADER_NAME}_shader)
    endforeach()

endif()
