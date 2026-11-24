#pragma once

// =============================================================================
// CSV 导出工具（Sprint 3 可选；实现归属 cpp-dev3）
//  - UTF-8 BOM、逗号转义、写 exports/<name>_<timestamp>.csv（自动创建目录）
//  - 当前为骨架空实现：函数返回 false 并写 error，页面可置灰按钮/Toast「二期支持」
// =============================================================================

#include <string>
#include <vector>

namespace zzyl::utils {

struct CsvExportResult {
    bool ok = false;
    std::string path;   // 成功时的输出文件路径
    std::string error;  // 失败原因
};

// relativeName：不含扩展名/目录的文件名（如 "checkin_list"）
CsvExportResult ExportCsv(const std::string& relativeName, const std::vector<std::string>& headers,
                          const std::vector<std::vector<std::string>>& rows);

// exports 目录绝对路径（exe 目录/exports；不存在则创建）
std::string ResolveExportDirectory();

}  // namespace zzyl::utils
