#pragma once

// =============================================================================
// 图片 / 纹理层（Sprint 2）
//  - WIC 解码 png/jpg（Windows 系统 API，禁止第三方库）
//  - OpenGL 纹理上传 + 缓存（TextureCache 作为 World 资源使用）
//  - 绘制助手：拉伸 / contain 留白 / cover 裁剪（表格缩略图 60×60 用 cover）
//  - 路径解析：资源相对路径 -> 绝对路径（cwd / exe 目录 / 源码根，兼容中文文件名）
// =============================================================================

#include <cstdint>
#include <filesystem>
#include <string>
#include <unordered_map>

#include "imgui.h"

#include "zzyl/layout/Layout.h"

namespace zzyl::render {

struct Texture {
    unsigned int id = 0;
    int width = 0;
    int height = 0;
    bool Valid() const { return id != 0; }
};

// 从文件加载（失败返回空 Texture 并打印一次警告，不抛异常）
Texture LoadTextureFromFile(const std::filesystem::path& path);
void DestroyTexture(Texture& texture);

// 资源相对路径解析：assets/icons/xxx.png / assets/images/xxx.jpg
std::filesystem::path ResolveAssetPath(const char* relativeUtf8);

// 绘制（只画，不持有时序状态）
void DrawImage(ImDrawList* drawList, const Texture& texture, const layout::Rect& target,
               ImU32 tint = IM_COL32_WHITE);
void DrawImageContain(ImDrawList* drawList, const Texture& texture, const layout::Rect& target,
                      ImU32 tint = IM_COL32_WHITE);  // 等比缩放，留白居中
void DrawImageCover(ImDrawList* drawList, const Texture& texture, const layout::Rect& target,
                    ImU32 tint = IM_COL32_WHITE);  // 等比缩放，裁剪填满

// 纹理缓存（World 资源；首次 Get 时加载，程序退出时 Clear）
class TextureCache {
public:
    const Texture& Get(const char* relativeUtf8);
    void Clear();

private:
    std::unordered_map<std::string, Texture> m_textures;
};

}  // namespace zzyl::render
