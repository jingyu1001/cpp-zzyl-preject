#pragma once

// =============================================================================
// 平台层：Windows 文件选择 + 导入（Sprint 4）
//  - GetOpenFileNameW 打开系统对话框（中文路径/UTF-8）
//  - 选中后复制到 <exe>/assets/uploads/（自动建目录），返回相对路径供 TextureCache/WIC 加载
//  - 图片：png/jpg/jpeg/bmp/webp；合同：pdf（仅保存文件名/链接，不渲染内容）
// =============================================================================

#include <cstddef>
#include <string>

namespace zzyl::platform {

struct FilePickResult {
    bool ok = false;
    bool canceled = false;
    std::string relativePath;  // 如 assets/uploads/20261124_213000_一寸照.png
    std::string absolutePath;  // 磁盘绝对路径
    std::string fileName;      // 原始文件名
    std::string error;
    bool isPdf = false;
};

// filter：Win32 过滤串，如 L"图片(*.png;*.jpg)\0*.png;*.jpg\0所有文件(*.*)\0*.*\0\0"
// subdir：assets 下子目录（默认 uploads）；maxBytes=0 不限制大小（默认 5MB，上传规格见 M4 §6）
FilePickResult PickAndImportFile(const wchar_t* filter, const std::string& subdir = "uploads",
                                 std::size_t maxBytes = 5 * 1024 * 1024);

// 仅图片/仅 PDF 的便捷封装：图片默认 5MB（房型/项目）；入住照片传 2MB；PDF 传 2MB
FilePickResult PickImageFile(const std::string& subdir = "uploads",
                             std::size_t maxBytes = 5 * 1024 * 1024);
FilePickResult PickPdfFile(const std::string& subdir = "uploads",
                           std::size_t maxBytes = 2 * 1024 * 1024);

// 用系统默认程序打开文件（PDF「查看」链接）；relativePath 以 assets 为根
bool OpenFileWithShell(const std::string& relativePath);

// 把相对路径解析为绝对路径（assets 根）
std::string ResolveAssetAbsolutePath(const std::string& relativePath);

// assets 根目录（exe 目录；开发时回退源码目录）
std::string AssetsRootDirectory();

}  // namespace zzyl::platform
