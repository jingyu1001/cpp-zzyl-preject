// =============================================================================
// CSV 导出工具（Sprint 3 可选能力；实现归属 cpp-dev3）
//
// 规格（Sprint3 总览 / dev3 任务单）：
//  - UTF-8 BOM（Excel 打开中文不乱码）
//  - 逗号/引号/换行转义（RFC 4180：含特殊字符的字段加双引号，内部引号翻倍）
//  - 输出到 `exports/<name>_<timestamp>.csv`（目录不存在自动创建）
//  - 失败返回 ok=false + error，不抛异常；页面据此 Toast 提示
// =============================================================================

#include "zzyl/utils/CsvExport.h"

#include <cstdio>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <string>
#include <system_error>
#include <vector>

#if defined(_WIN32)
#include <windows.h>
#endif

namespace zzyl::utils {
namespace {

// 可执行文件所在目录（exports 固定放在 exe 旁边，双击/调试均一致）
std::filesystem::path ExecutableDirectory() {
#if defined(_WIN32)
    wchar_t buffer[MAX_PATH];
    const DWORD length = ::GetModuleFileNameW(nullptr, buffer, MAX_PATH);
    if (length > 0 && length < MAX_PATH) {
        return std::filesystem::path(buffer).parent_path();
    }
#endif
    std::error_code errorCode;
    const std::filesystem::path current = std::filesystem::current_path(errorCode);
    return errorCode ? std::filesystem::path(".") : current;
}

std::string TimestampText() {
    const std::time_t now = std::time(nullptr);
    std::tm local{};
#if defined(_WIN32)
    localtime_s(&local, &now);
#else
    localtime_r(&now, &local);
#endif
    char buffer[32];
    std::strftime(buffer, sizeof(buffer), "%Y%m%d_%H%M%S", &local);
    return std::string(buffer);
}

// RFC 4180：含逗号/引号/换行时用双引号包裹，内部双引号翻倍
std::string EscapeField(const std::string& value) {
    if (value.find_first_of(",\"\r\n") == std::string::npos) {
        return value;
    }
    std::string escaped;
    escaped.reserve(value.size() + 2);
    escaped.push_back('"');
    for (char character : value) {
        if (character == '"') {
            escaped += "\"\"";
        } else {
            escaped.push_back(character);
        }
    }
    escaped.push_back('"');
    return escaped;
}

void WriteRow(std::ofstream& stream, const std::vector<std::string>& fields) {
    for (std::size_t index = 0; index < fields.size(); ++index) {
        if (index > 0) {
            stream << ',';
        }
        stream << EscapeField(fields[index]);
    }
    stream << "\r\n";
}

}  // namespace

std::string ResolveExportDirectory() {
    const std::filesystem::path directory = ExecutableDirectory() / "exports";
    std::error_code errorCode;
    std::filesystem::create_directories(directory, errorCode);
    if (errorCode) {
        return std::string();
    }
    return directory.string();
}

CsvExportResult ExportCsv(const std::string& relativeName,
                          const std::vector<std::string>& headers,
                          const std::vector<std::vector<std::string>>& rows) {
    CsvExportResult result;
    if (relativeName.empty()) {
        result.error = "导出文件名不能为空";
        return result;
    }
    const std::string directoryText = ResolveExportDirectory();
    if (directoryText.empty()) {
        result.error = "无法创建 exports 目录";
        return result;
    }
    const std::filesystem::path filePath =
        std::filesystem::path(directoryText) / (relativeName + "_" + TimestampText() + ".csv");
    std::ofstream stream(filePath, std::ios::binary | std::ios::trunc);
    if (!stream) {
        result.error = "无法写入导出文件";
        return result;
    }
    stream << "\xEF\xBB\xBF";  // UTF-8 BOM
    WriteRow(stream, headers);
    for (const std::vector<std::string>& row : rows) {
        WriteRow(stream, row);
    }
    stream.flush();
    if (!stream) {
        result.error = "写入导出文件失败";
        return result;
    }
    result.ok = true;
    result.path = filePath.string();
    return result;
}

}  // namespace zzyl::utils
