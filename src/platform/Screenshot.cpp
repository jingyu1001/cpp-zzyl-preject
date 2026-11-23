#include "zzyl/platform/Screenshot.h"

#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <vector>

#include "glad/glad.h"

namespace zzyl::platform {
namespace {

void WriteU16(std::FILE* file, std::uint16_t value) {
    const unsigned char bytes[2] = {static_cast<unsigned char>(value & 0xFF),
                                    static_cast<unsigned char>((value >> 8) & 0xFF)};
    std::fwrite(bytes, 1, 2, file);
}

void WriteU32(std::FILE* file, std::uint32_t value) {
    const unsigned char bytes[4] = {static_cast<unsigned char>(value & 0xFF),
                                    static_cast<unsigned char>((value >> 8) & 0xFF),
                                    static_cast<unsigned char>((value >> 16) & 0xFF),
                                    static_cast<unsigned char>((value >> 24) & 0xFF)};
    std::fwrite(bytes, 1, 4, file);
}

}  // namespace

bool CaptureFramebufferToBmp(int width, int height, const std::string& path, std::string* error) {
    if (width <= 0 || height <= 0) {
        if (error) {
            *error = "帧缓冲尺寸为 0（窗口最小化？）";
        }
        return false;
    }

    const std::size_t pixelBytes = static_cast<std::size_t>(width) * static_cast<std::size_t>(height) * 3u;
    std::vector<unsigned char> rgb(pixelBytes);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadBuffer(GL_BACK);
    glFinish();
    glReadPixels(0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, rgb.data());
    const GLenum glError = glGetError();
    if (glError != GL_NO_ERROR) {
        if (error) {
            char buffer[96];
            std::snprintf(buffer, sizeof(buffer), "glReadPixels 失败, GL error=0x%04X", glError);
            *error = buffer;
        }
        return false;
    }

    std::error_code ec;
    const std::filesystem::path outputPath(path);
    if (outputPath.has_parent_path()) {
        std::filesystem::create_directories(outputPath.parent_path(), ec);
    }

    std::FILE* file = std::fopen(path.c_str(), "wb");
    if (file == nullptr) {
        if (error) {
            *error = "无法写入截图文件：" + path;
        }
        return false;
    }

    const std::uint32_t rowBytes = static_cast<std::uint32_t>(width) * 3u;
    const std::uint32_t padding = (4u - (rowBytes % 4u)) % 4u;
    const std::uint32_t imageBytes = (rowBytes + padding) * static_cast<std::uint32_t>(height);
    const std::uint32_t fileBytes = 14u + 40u + imageBytes;

    // BITMAPFILEHEADER
    std::fputc('B', file);
    std::fputc('M', file);
    WriteU32(file, fileBytes);
    WriteU16(file, 0);
    WriteU16(file, 0);
    WriteU32(file, 14u + 40u);
    // BITMAPINFOHEADER
    WriteU32(file, 40u);
    WriteU32(file, static_cast<std::uint32_t>(width));
    WriteU32(file, static_cast<std::uint32_t>(height));  // 正高度 = 自底向上
    WriteU16(file, 1);
    WriteU16(file, 24);
    WriteU32(file, 0);  // BI_RGB
    WriteU32(file, imageBytes);
    WriteU32(file, 2835);
    WriteU32(file, 2835);
    WriteU32(file, 0);
    WriteU32(file, 0);

    const unsigned char zeros[3] = {0, 0, 0};
    for (int row = 0; row < height; ++row) {
        const unsigned char* src = rgb.data() + static_cast<std::size_t>(row) * static_cast<std::size_t>(width) * 3u;
        for (int column = 0; column < width; ++column) {
            // BMP 为 BGR 顺序
            const unsigned char bgr[3] = {src[column * 3 + 2], src[column * 3 + 1], src[column * 3 + 0]};
            std::fwrite(bgr, 1, 3, file);
        }
        if (padding > 0) {
            std::fwrite(zeros, 1, padding, file);
        }
    }
    std::fclose(file);
    return true;
}

}  // namespace zzyl::platform
