#include "zzyl/render/Texture.h"

#include <algorithm>
#include <cstdio>
#include <vector>

#include <windows.h>
#include <wincodec.h>
#include <wrl/client.h>

#include "glad/glad.h"

#ifndef ZZYL_SOURCE_DIR
#define ZZYL_SOURCE_DIR "."
#endif

namespace zzyl::render {
namespace {

using Microsoft::WRL::ComPtr;

ComPtr<IWICImagingFactory> g_factory;

bool EnsureWicFactory() {
    if (g_factory) {
        return true;
    }
    // 允许重复初始化：已初始化时返回 S_FALSE 或 RPC_E_CHANGED_MODE，均不影响使用
    ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    ComPtr<IWICImagingFactory> factory;
    const HRESULT hr = ::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                          IID_PPV_ARGS(&factory));
    if (FAILED(hr)) {
        std::fprintf(stderr, "[Texture] 无法创建 WIC 工厂（hr=0x%08X）\n", static_cast<unsigned>(hr));
        return false;
    }
    g_factory = factory;
    return true;
}

std::filesystem::path ExecutableDirectory() {
    wchar_t buffer[MAX_PATH * 4] = {};
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer, static_cast<DWORD>(std::size(buffer)));
    if (length == 0) {
        return {};
    }
    return std::filesystem::path(buffer).parent_path();
}

}  // namespace

std::filesystem::path ResolveAssetPath(const char* relativeUtf8) {
    if (relativeUtf8 == nullptr || relativeUtf8[0] == '\0') {
        return {};
    }
    const std::u8string utf8Path(reinterpret_cast<const char8_t*>(relativeUtf8));
    const std::filesystem::path relative(utf8Path);
    if (relative.is_absolute() && std::filesystem::exists(relative)) {
        return relative;
    }

    std::vector<std::filesystem::path> candidates;
    candidates.push_back(std::filesystem::current_path() / relative);
    const std::filesystem::path exeDir = ExecutableDirectory();
    if (!exeDir.empty()) {
        candidates.push_back(exeDir / relative);
        candidates.push_back(exeDir / ".." / relative);
        candidates.push_back(exeDir / ".." / ".." / relative);
    }
    candidates.push_back(std::filesystem::path(ZZYL_SOURCE_DIR) / relative);

    for (const auto& candidate : candidates) {
        std::error_code errorCode;
        if (std::filesystem::exists(candidate, errorCode) && !errorCode) {
            return candidate;
        }
    }
    return {};
}

Texture LoadTextureFromFile(const std::filesystem::path& path) {
    Texture texture;
    if (path.empty() || !EnsureWicFactory()) {
        return texture;
    }

    ComPtr<IWICBitmapDecoder> decoder;
    HRESULT hr = g_factory->CreateDecoderFromFilename(path.wstring().c_str(), nullptr, GENERIC_READ,
                                                      WICDecodeMetadataCacheOnDemand, &decoder);
    if (FAILED(hr)) {
        std::fprintf(stderr, "[Texture] 解码失败（打开文件）：%ls\n", path.wstring().c_str());
        return texture;
    }

    ComPtr<IWICBitmapFrameDecode> frame;
    hr = decoder->GetFrame(0, &frame);
    if (FAILED(hr)) {
        std::fprintf(stderr, "[Texture] 解码失败（取帧）：%ls\n", path.wstring().c_str());
        return texture;
    }

    UINT width = 0;
    UINT height = 0;
    frame->GetSize(&width, &height);

    ComPtr<IWICFormatConverter> converter;
    hr = g_factory->CreateFormatConverter(&converter);
    if (SUCCEEDED(hr)) {
        hr = converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone,
                                   nullptr, 0.0, WICBitmapPaletteTypeCustom);
    }
    if (FAILED(hr) || width == 0 || height == 0) {
        std::fprintf(stderr, "[Texture] 解码失败（格式转换）：%ls\n", path.wstring().c_str());
        return texture;
    }

    const UINT stride = width * 4u;
    std::vector<unsigned char> pixels(static_cast<std::size_t>(stride) * height);
    hr = converter->CopyPixels(nullptr, stride, static_cast<UINT>(pixels.size()), pixels.data());
    if (FAILED(hr)) {
        std::fprintf(stderr, "[Texture] 解码失败（CopyPixels）：%ls\n", path.wstring().c_str());
        return texture;
    }

    GLuint glTexture = 0;
    glGenTextures(1, &glTexture);
    glBindTexture(GL_TEXTURE_2D, glTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, static_cast<GLsizei>(width), static_cast<GLsizei>(height),
                 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);

    texture.id = glTexture;
    texture.width = static_cast<int>(width);
    texture.height = static_cast<int>(height);
    return texture;
}

void DestroyTexture(Texture& texture) {
    if (texture.id != 0) {
        GLuint glTexture = texture.id;
        glDeleteTextures(1, &glTexture);
        texture.id = 0;
        texture.width = 0;
        texture.height = 0;
    }
}

namespace {

void DrawQuad(ImDrawList* drawList, const Texture& texture, const layout::Rect& target, ImVec2 uv0,
              ImVec2 uv1, ImU32 tint) {
    if (drawList == nullptr || !texture.Valid() || target.Empty()) {
        return;
    }
    const ImTextureRef textureRef(static_cast<ImTextureID>(texture.id));
    drawList->AddImage(textureRef, ImVec2(target.x, target.y),
                       ImVec2(target.Right(), target.Bottom()), uv0, uv1, tint);
}

}  // namespace

void DrawImage(ImDrawList* drawList, const Texture& texture, const layout::Rect& target, ImU32 tint) {
    DrawQuad(drawList, texture, target, ImVec2(0.0f, 0.0f), ImVec2(1.0f, 1.0f), tint);
}

void DrawImageContain(ImDrawList* drawList, const Texture& texture, const layout::Rect& target,
                      ImU32 tint) {
    if (!texture.Valid() || target.Empty() || texture.width <= 0 || texture.height <= 0) {
        return;
    }
    const float scale = std::min(target.w / static_cast<float>(texture.width),
                                 target.h / static_cast<float>(texture.height));
    const float width = static_cast<float>(texture.width) * scale;
    const float height = static_cast<float>(texture.height) * scale;
    const layout::Rect fitted{target.CenterX() - width * 0.5f, target.CenterY() - height * 0.5f,
                              width, height};
    DrawImage(drawList, texture, fitted, tint);
}

void DrawImageCover(ImDrawList* drawList, const Texture& texture, const layout::Rect& target,
                    ImU32 tint) {
    if (!texture.Valid() || target.Empty() || texture.width <= 0 || texture.height <= 0) {
        return;
    }
    const float textureAspect = static_cast<float>(texture.width) / static_cast<float>(texture.height);
    const float targetAspect = target.w / target.h;
    ImVec2 uv0(0.0f, 0.0f);
    ImVec2 uv1(1.0f, 1.0f);
    if (textureAspect > targetAspect) {
        // 纹理更宽：裁剪左右
        const float visible = targetAspect / textureAspect;
        uv0.x = (1.0f - visible) * 0.5f;
        uv1.x = 1.0f - uv0.x;
    } else {
        // 纹理更高：裁剪上下
        const float visible = textureAspect / targetAspect;
        uv0.y = (1.0f - visible) * 0.5f;
        uv1.y = 1.0f - uv0.y;
    }
    DrawQuad(drawList, texture, target, uv0, uv1, tint);
}

const Texture& TextureCache::Get(const char* relativeUtf8) {
    static const Texture kEmpty;
    if (relativeUtf8 == nullptr || relativeUtf8[0] == '\0') {
        return kEmpty;
    }
    const std::string key(relativeUtf8);
    auto it = m_textures.find(key);
    if (it != m_textures.end()) {
        return it->second;
    }
    const std::filesystem::path resolved = ResolveAssetPath(relativeUtf8);
    Texture texture = LoadTextureFromFile(resolved);
    auto [inserted, ok] = m_textures.emplace(key, texture);
    (void)ok;
    return inserted->second;
}

void TextureCache::Clear() {
    for (auto& [path, texture] : m_textures) {
        DestroyTexture(texture);
    }
    m_textures.clear();
}

}  // namespace zzyl::render
