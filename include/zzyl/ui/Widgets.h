#pragma once

// =============================================================================
// 通用列表页组件契约（Sprint 2 · M0）
//
// 实现归属：cpp-dev1（`src/ui/Widgets.cpp`）。本头文件是**契约**，页面（dev2/dev3）
// 只依赖这里；实现内部可随意调整，但签名/结构体字段变更需先找架构师。
//
// 组件：搜索栏 / 工具栏 / 数据表格 / 分页 / 表单弹窗 / 确认框 / Toast / 状态标签
// 规格依据：UI复刻规格_Cpp_v0.2.md §3（1280×720 实测）
// =============================================================================

#include <string>
#include <vector>

#include "zzyl/layout/Layout.h"

namespace zzyl::render {
class TextureCache;
}

namespace zzyl::ui {

enum class Align { Left, Center, Right };

// ---- 搜索栏（§3.1） ----
enum class SearchFieldType { Text, Select, DateRange };

struct SearchField {
    std::string key;
    std::string label;   // 宽 68，右对齐
    std::string value;   // 当前值（Select 时为选中项文本）
    SearchFieldType type = SearchFieldType::Text;
    std::vector<std::string> options;  // Select 候选项
    float controlWidth = 240.0f;       // 控件宽（默认 240）
};

struct SearchBarResult {
    bool search = false;  // 点击搜索 / 回车
    bool reset = false;   // 点击重置
};

// 在 area 内绘制搜索栏；返回本帧交互结果。高度可用 SearchBarHeight() 预估。
SearchBarResult DrawSearchBar(const layout::Rect& area, std::vector<SearchField>& fields);
float SearchBarHeight(int fieldCount);

// ---- 工具栏（§3.2） ----
enum class ToolbarKind { Add, Edit, Delete, Import, Export };

struct ToolbarButton {
    ToolbarKind kind = ToolbarKind::Add;
    std::string label;  // 新增/修改/删除/导入/导出
    bool enabled = true;
};

struct ToolbarResult {
    int clicked = -1;  // buttons 下标；-1 无点击
};

ToolbarResult DrawToolbar(const layout::Rect& area, std::vector<ToolbarButton>& buttons);
float ToolbarHeight();

// ---- 数据表格（§3.3） ----
enum class CellKind {
    Text,       // 普通文本
    Image,      // 单元格文本为图片相对路径（assets/...），显示缩略图
    StatusTag,  // 单元格文本为 "启用"/"禁用"（或 1/0），显示状态标签
    Switch,     // 状态开关（40×20）
    Actions,    // 操作列：单元格文本用 "|" 分隔的动作名（如 "修改|删除|禁用"）
};

struct TableColumn {
    std::string key;
    std::string title;
    float width = 120.0f;  // 基准宽（按比例缩放由实现负责）
    Align align = Align::Center;
    CellKind kind = CellKind::Text;
};

struct TableRow {
    int id = 0;                     // 业务主键（勾选/操作/开关返回）
    std::vector<std::string> cells;  // 与 columns 一一对应
    bool enabled = true;             // Switch/StatusTag 用
    // Actions 列专用：cells[actionColumn] 里用 '|' 分隔本行动作按钮文字，
    // 例如 "修改|删除|禁用"（逐行动态标签，如启用/禁用随状态变化）。
    // 为空时回退到 TableDrawOptions.actionLabels。
};

struct TableDrawOptions {
    bool showCheckbox = true;  // 勾选列（宽 50）
    bool showIndex = false;    // 序号列（宽 50）
    std::vector<std::string> actionLabels;  // Actions 列的默认按钮（行内 cells 未提供时使用）
    float rowHeight = 49.0f;   // 数据行高（M4 列表传 60；0 表示用默认 49，向后兼容）
};

struct TableResult {
    int clickedRow = -1;       // 普通行点击（返回 TableRow.id）
    int clickedAction = -1;    // Actions 列点击：返回该行动作按钮下标（行内 '|' 分割后的下标）
    int actionRowId = -1;      // Actions 列点击所在行的业务 id（配合 clickedAction 使用）
    struct SwitchToggle {
        int rowId = 0;         // 被切换开关所在行 id
        bool enabled = false;  // 切换后的状态
    };
    std::vector<SwitchToggle> switchToggles;  // 本帧被切换的开关（可能多个）
    std::vector<int> checkedIds;              // 当前勾选的 id 列表（覆盖 checkedIds 入参）
};

// textures：图片列取纹理（可传 nullptr，图片列显示占位）
// 返回本帧交互：
//  - 普通行点击 → clickedRow；Actions 列点击 → clickedAction + actionRowId
//  - 状态开关切换 → switchToggles（同时返回 checkedIds 最新态）
TableResult DrawDataTable(const layout::Rect& area, const std::vector<TableColumn>& columns,
                          const std::vector<TableRow>& rows, std::vector<int>& checkedIds,
                          const render::TextureCache* textures, const TableDrawOptions& options);

// ---- 分页（§3.4） ----
struct PaginationState {
    int page = 1;
    int pageSize = 10;
    int total = 0;
};

struct PaginationResult {
    bool changed = false;  // 页码/每页条数变化（需要重新查询/刷新）
};

PaginationResult DrawPagination(const layout::Rect& area, PaginationState& state);
float PaginationHeight();

// ---- 表单弹窗（§3.5） ----
struct DialogState {
    bool open = false;
    std::string title;
    float width = 500.0f;  // 用户管理 600 / 房型、护理项目 500
};

// 弹窗结果：Ok=点击确定（组件不自动关闭，调用方校验成功后手动 state.open=false）；
// Cancel=取消/右上×（组件已自动关闭并清 state.open）；None=无操作。
enum class DialogResult { None, Ok, Cancel };

// 打开时返回 true（调用方只绘制内容区：标题栏 + 底部确定/取消由组件负责，
// 调用方不得再画 footer，避免重复）。无论返回 true/false，都必须配对调用 EndFormDialog()。
bool BeginFormDialog(const layout::Rect& pageArea, DialogState& state);

// 绘制底部按钮并返回结果。Ok 时弹窗保持 open（调用方可继续显示校验红字）；
// Cancel 时组件把 state.open 置 false。
DialogResult EndFormDialog();

// ---- 确认框（§3.6） ----
enum class ConfirmAction { None, Ok, Cancel };

ConfirmAction DrawConfirmDialog(const layout::Rect& pageArea, bool& open, const std::string& title,
                                const std::string& message);

// ---- Toast 消息提示（§3.7） ----
enum class ToastType { Success = 0, Warning = 1, Error = 2 };

struct ToastItem {
    std::string text;
    ToastType type = ToastType::Success;
    float remainingSeconds = 3.0f;
};

struct ToastState {
    std::vector<ToastItem> items;
};

void PushToast(ToastState& state, const std::string& text, ToastType type);
// 顶部居中绘制并推进计时（调用方传本帧 deltaTime，秒）
void DrawToasts(const layout::Rect& windowArea, ToastState& state, float deltaTime);

}  // namespace zzyl::ui
