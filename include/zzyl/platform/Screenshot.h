#pragma once

#include <string>

namespace zzyl::platform {

// 读取当前 GL_BACK 帧缓冲并写 24bpp BMP（OpenGL 原点在左下，BMP 正向高度即自底向上存储，
// 因此行序无需翻转）。用于自测截图与视觉验收，不参与业务渲染。
// 返回 false 时 error 写入原因。
bool CaptureFramebufferToBmp(int width, int height, const std::string& path, std::string* error);

}  // namespace zzyl::platform
