# =============================================================================
# 中州养老 · C++ 转译端（OpenGL + GLFW + ImGui + glad）
# 架构说明见 docs/architecture/Cpp架构设计_v0.1.md
#
# 第三方库不复制进仓库，通过 ZZYL_DEPS_ROOT 指向本机依赖根目录。
# 默认值：C:/Users/ASUS/Desktop/C++
#
# 构建（VS 2026 生成器）：
#   cmake -S . -B build -G "Visual Studio 18 2026" -A x64
#   cmake --build build --config Release
# =============================================================================
cmake_minimum_required(VERSION 3.20)

project(ZzylCppTranspiler VERSION 0.1.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

if(NOT MSVC)
    message(FATAL_ERROR "本项目当前只支持 MSVC（Visual Studio 2026）。")
endif()

# ---------------------------------------------------------------------------
# 第三方依赖路径（可用 -DZZYL_DEPS_ROOT=... 覆盖）
# ---------------------------------------------------------------------------
set(ZZYL_DEPS_ROOT "C:/Users/ASUS/Desktop/C++" CACHE PATH "第三方库根目录")
set(ZZYL_GLFW_DIR   "${ZZYL_DEPS_ROOT}/glfw-3.4.bin.WIN64")
set(ZZYL_IMGUI_DIR  "${ZZYL_DEPS_ROOT}/imgui-1.92.2b/imgui-1.92.2b")
set(ZZYL_GLAD_DIR   "${ZZYL_DEPS_ROOT}/glad")
set(ZZYL_SQLITE_DIR "${ZZYL_DEPS_ROOT}/sqlite3-amalgamation-3540100/sqlite-amalgamation-3540100")
set(ZZYL_GLFW_LIB   "${ZZYL_GLFW_DIR}/lib-vc2022/glfw3.lib")

foreach(dep_file
        "${ZZYL_GLFW_DIR}/include/GLFW/glfw3.h"
        "${ZZYL_GLFW_LIB}"
        "${ZZYL_IMGUI_DIR}/imgui.h"
        "${ZZYL_IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
        "${ZZYL_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
        "${ZZYL_GLAD_DIR}/include/glad/glad.h"
        "${ZZYL_GLAD_DIR}/src/glad.c"
        "${ZZYL_SQLITE_DIR}/sqlite3.c"
        "${ZZYL_SQLITE_DIR}/sqlite3.h")
    if(NOT EXISTS "${dep_file}")
        message(FATAL_ERROR
            "缺少第三方依赖文件：${dep_file}\n"
            "请确认依赖路径，或用 -DZZYL_DEPS_ROOT=<C++依赖目录> 重新配置。")
    endif()
endforeach()

# ---------------------------------------------------------------------------
# 全局编译约定
# ---------------------------------------------------------------------------
if(MSVC)
    # 源码为 UTF-8（中文字符串字面量）：必须 /utf-8，否则 MSVC 会按 GBK 误解码。
    add_compile_options(/utf-8 /W3 /permissive- /Zc:__cplusplus)
    add_compile_definitions(
        NOMINMAX
        WIN32_LEAN_AND_MEAN
        _CRT_SECURE_NO_WARNINGS
        _WIN32_WINNT=0x0A00
        UNICODE
        _UNICODE
    )
endif()

# ---------------------------------------------------------------------------
# 第三方 target（源码编译 imgui / glad；glfw 使用官方预编译静态库）
# ---------------------------------------------------------------------------
add_library(imgui STATIC
    "${ZZYL_IMGUI_DIR}/imgui.cpp"
    "${ZZYL_IMGUI_DIR}/imgui_draw.cpp"
    "${ZZYL_IMGUI_DIR}/imgui_tables.cpp"
    "${ZZYL_IMGUI_DIR}/imgui_widgets.cpp"
    "${ZZYL_IMGUI_DIR}/backends/imgui_impl_glfw.cpp"
    "${ZZYL_IMGUI_DIR}/backends/imgui_impl_opengl3.cpp"
)
target_include_directories(imgui SYSTEM PUBLIC
    "${ZZYL_IMGUI_DIR}"
    "${ZZYL_IMGUI_DIR}/backends"
    "${ZZYL_GLFW_DIR}/include"
)

add_library(glad STATIC "${ZZYL_GLAD_DIR}/src/glad.c")
target_include_directories(glad SYSTEM PUBLIC "${ZZYL_GLAD_DIR}/include")

# SQLite3 amalgamation（Sprint 3；不复制大文件进仓库，引用绝对路径 + ZZYL_DEPS_ROOT）
add_library(sqlite3 STATIC "${ZZYL_SQLITE_DIR}/sqlite3.c")
target_include_directories(sqlite3 SYSTEM PUBLIC "${ZZYL_SQLITE_DIR}")
target_compile_definitions(sqlite3 PRIVATE
    SQLITE_THREADSAFE=1
    SQLITE_OMIT_LOAD_EXTENSION
    SQLITE_DQS=0
    SQLITE_DEFAULT_MEMSTATUS=0
)

add_library(glfw STATIC IMPORTED)
set_target_properties(glfw PROPERTIES
    IMPORTED_LOCATION "${ZZYL_GLFW_LIB}"
    INTERFACE_INCLUDE_DIRECTORIES "${ZZYL_GLFW_DIR}/include"
)

# ---------------------------------------------------------------------------
# 应用可执行文件
# ---------------------------------------------------------------------------
set(ZZYL_APP_SOURCES
    src/main.cpp

    # 核心 ECS
    src/core/SystemScheduler.cpp

    # 平台层：窗口 / ImGui / 截图
    src/platform/Window.cpp
    src/platform/ImGuiLayer.cpp
    src/platform/Screenshot.cpp

    # 布局 / 主题 / 绘制工具 / 图片纹理（WIC）
    src/layout/Layout.cpp
    src/render/Theme.cpp
    src/render/UiDraw.cpp
    src/render/Texture.cpp

    # 数据层（接口 + Mock 实现 + 菜单目录 + Sprint2 种子数据服务）
    src/data/MockServices.cpp
    src/data/MenuCatalog.cpp
    src/data/SeedServices.cpp
    src/data/SqliteData.cpp

    # 应用层
    src/app/App.cpp
    src/app/Systems.cpp
    src/app/Routes.cpp
    src/app/ModuleRegistry.cpp

    # 通用组件（cpp-dev1）
    src/ui/Widgets.cpp

    # 工具（csv 导出骨架，cpp-dev3 可选实现）
    src/utils/CsvExport.cpp

    # 模块（按文件归属拆给 3 名程序员）
    src/shell/ShellModule.cpp        # cpp-dev1
    src/pages/LoginModule.cpp        # cpp-dev2
    src/pages/SystemUserModule.cpp   # cpp-dev2（Sprint 2 M1）
    src/pages/DashboardModule.cpp    # cpp-dev3
    src/pages/RoomTypeModule.cpp     # cpp-dev3（Sprint 2 M2）
    src/pages/NursingProjectModule.cpp  # cpp-dev3（Sprint 2 M3）
    src/pages/CheckInListModule.cpp     # cpp-dev1（Sprint 3 M4 列表）
    src/pages/CheckInFormModule.cpp     # cpp-dev2（Sprint 3 M4 表单）
    src/pages/CheckInDetailModule.cpp   # cpp-dev2（Sprint 3 M4 详情）
)

add_executable(zzyl_desktop ${ZZYL_APP_SOURCES})

target_include_directories(zzyl_desktop PRIVATE include)

target_link_libraries(zzyl_desktop PRIVATE
    imgui
    glfw
    glad
    sqlite3
    opengl32
    user32
    gdi32
    shell32
    ole32
    windowscodecs   # WIC 图片解码（Sprint 2；Windows 系统组件，无第三方依赖）
)

# 资源根目录（Texture::ResolveAssetPath 的兜底候选）
target_compile_definitions(zzyl_desktop PRIVATE
    ZZYL_SOURCE_DIR="${CMAKE_SOURCE_DIR}"
)

# 运行期资源目录：把 assets/ 复制到 exe 旁边（图标 / 图片）
add_custom_command(TARGET zzyl_desktop POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:zzyl_desktop>/assets"
    COMMENT "复制 assets/ 到输出目录"
)

set_target_properties(zzyl_desktop PROPERTIES
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    OUTPUT_NAME "zzyl_desktop"
)

# 便于在 IDE 中直接看到头文件
target_sources(zzyl_desktop PRIVATE
    include/zzyl/version.h
)
