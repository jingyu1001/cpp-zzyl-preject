# Sprint 4 任务单 · cpp-dev2 · M5 护理计划 + M6 护理等级 + M4 级联/上传

> 负责人：cpp-dev2
> 可写文件：`src/pages/NursingPlanModule.cpp`、`src/pages/NursingLevelModule.cpp`、`src/pages/CheckInFormModule.cpp`
> 契约（只读）：`BusinessServices.h`（`INursingPlanService`/`INursingLevelService`/`IFloorLayoutService`/`ICheckInService` 及 Record）、`include/zzyl/platform/FileDialog.h`、`ui::*`、`AppState`、`Routes`
> 必读：架构 v0.4 §3/§4、Sprint4 总览；Web 参考 `views/nursing/plan/index.vue`、`views/nursing/level/index.vue`、`views/nursing/checkIn`（级联/上传）
> 骨架现状：18/19/21 帧为占位页；SQLite v5 已灌 plans 3 / levels 3 / floors 11 / rooms 60 / beds 89；服务接口已就绪；`--empty-data` 空态可验。

## 1. M5 护理计划（`/nursing/plan`）
- 列表：搜索（名称 Text、状态 Select 启用/禁用）+ 重置；工具栏「新增」；表格（`ui::DrawDataTable`）：序号/名称/状态（StatusTag 启用绿·禁用灰）/创建时间/操作（编辑·删除·启用禁用），分页（共 3 条，10/页，贴底）；
- 表单弹窗/整页：护理计划名称*、状态*、排序* + **护理项目明细**（可增删行）：项目下拉（`DataServices.nursingProject->List()` 或 `ImageLibrary` 同名接口取项目名/id）、执行时间（HH:mm:ss）、执行周期（0 天/1 周/2 月下拉）、执行频次（数字）；保存 `Add/Update`（projects 随 record 传，服务内事务替换）；校验名称非空/排序必填/项目行至少 1 行可选；
- 删除确认框（`ui::DrawConfirmDialog`）+ 启用禁用 `SetStatus` + Toast + 刷新；重启后数据仍在（SQLite）。
- 字段映射：`NursingPlanRecord{planName,status,sortNo,projects[]}`；周期中文显示 天/周/月。

## 2. M6 护理等级（`/nursing/level`）
- 列表：搜索（等级名称、状态）；表格 7 列：序号/护理等级名称/执行护理计划(planName)/护理费用(元/月)/状态/等级说明/创建时间/操作（编辑·删除）；分页共 3 条（1500/2000/3000）；
- 表单：等级名称*、护理计划*（下拉 `nursingPlan->List()`）、护理费用*（数值>0）、状态、等级说明（多行）；保存 `Add/Update`；删除确认/启停/Toast/刷新/重启持久化；
- 字段：`NursingLevelRecord{name,price,planId,status,description}`。

## 3. M4 表单增强（`CheckInFormModule.cpp`）
### 3.1 床位级联
- 入住配置区：楼层下拉（`floorLayout->Floors()`）→ 房间下拉（`Rooms(floorId)`）→ 床位下拉（`Beds(roomId)`）；选择床位写 `record.bedId/bedNumber/floorId/roomId/floorName/roomCode`；
- 选择后按房型价格带出床位费：用房型名（room.typeName）在 `DataServices.roomType->List()` 匹配价格 → `record.bedFee`（只读显示）；匹配不到则 0 并允许手动？本期只读 0 提示；
- 下拉变更时清空下级选择；旧数据 fallback（无级联字段时床位号文本仍可显示）。
### 3.2 上传
- 基本信息三张图：一寸照片/身份证人像面/身份证国徽面 → 按钮调 `platform::PickImageFile("uploads/checkin")`；成功写 `imagePath/idCardPortraitImg/idCardNationalEmblemImg`，用 `TextureCache` + 100×100 预览；取消不报错；失败 Toast 红字；
- 签约办理 PDF 合同：`platform::PickPdfFile("uploads/contract")` → 写 `agreementPath`，显示文件名 +「[查看]」占位（不渲染 PDF）；
- 详情页对应字段展示（图片预览/PDF 文件名）。

## 4. 硬约束
- 只改上述 3 个文件；不改 CMake/契约头/数据层/他人页面；不 commit；
- 不直接 include `SqliteData.h`；页面只经 `DataServices`；上传只经 `platform` API；
- 布局比例化；字体作用域 RAII；可见调试先申请 cpp-control，结束释放；
- 提交前校验：必填/身份证 18 位/日期顺序/手机 11 位/金额数字。

## 5. 自测与验收
```bat
cmake -S . -B build-dev2 -G "Visual Studio 18 2026" -A x64
cmake --build build-dev2 --config Release --parallel
build-dev2\Release\zzyl_desktop.exe --auto-login
build-dev2\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev2
```
- M5：3 条、搜索、增改删/启停、项目明细保存、重启仍在；
- M6：3 条、计划下拉、费用/状态、CRUD、重启仍在；
- M4：级联三下拉联动+床位费带出；图片选择后 `assets/uploads/` 有文件并显示；PDF 文件名/查看占位；
- 帧 18/19/21 显示真实内容；1024/1920 不超屏；
- 汇报 lead（抄架构师）。

## 6. Bug 归属
- M5/M6/M4 级联/上传 UI → **cpp-dev2**；
- 数据服务/字段/迁移/FileDialog 平台 → **cpp-arch**；
- 通用组件问题 → cpp-dev1（先报架构）。

## 7. 规格对齐差异清单（Sprint4 功能规格 v0.1，必读）
### M5 护理计划
- 表格**5 列**：序号(50)/名称(auto)/状态(auto)/创建时间(180)/操作(**280**，fixed right)；行操作 **4 个**：修改/删除/查看/启用禁用；当 `record.levelRefCount>0`（被护理等级引用）时「修改/删除」**禁用**（灰色不可点）；状态：1 启用绿标签 / 0 禁用红标签（Web 字典禁用=2 为历史值，以 DB 0 为准）。
- 弹窗 **1000×614**（label-width 140）：顶部 3 字段：护理计划名称*、状态*（单选 启用=1/禁用=0，默认 1）、排序*（数字 min1 max999999，默认1）；查看模式全禁用、隐藏操作列与底部按钮。
- 项目子表 **5 列**：护理项目名称（下拉=所有护理项目 9 条）/期望服务时间 HH:mm:ss/执行周期（天0/周1/月2）/执行频次（min1 max7 默认1）/操作（+ 新增、− 删除，仅剩 1 行不显示）；表头高 50 背景 #F3F8F9，表体高 **300 可滚动**，行高 50；提交前**项目去重校验**（projectId），重复提示「请勿选择重复的护理项目」。
- 数据：`NursingPlanRecord{planName,status,sortNo,projects[],levelRefCount}`；保存用 `Add/Update`（projects 随 record 事务替换）；种子 3 计划 + 6 项目明细已灌。
### M6 护理等级
- 表格 **8 列**：序号(100)/护理等级名称(150)/执行护理计划/护理费用(元/月)/状态(120)/等级说明/创建时间(180)/操作；操作：修改/删除/启用禁用；状态 1 启用绿 / 0 禁用红。
- 弹窗 **500×432**（label-width 80）、**5 字段全必填**：名称*、护理计划*（下拉 `nursingPlan->List()`）、护理费用*（数字 step100）、状态*（单选 0/1）、等级说明*（输入）；提交校验 5 项非空，成功 Toast+刷新。
### M4 级联 / 上传
- 级联：楼层→房间→**只列空闲床位**（`floorLayout->AvailableBeds(roomId)`，bed_status=0）；选中后写 `floorId/roomId/floorName/roomCode/bedId/bedNumber`；床位费取 **`RoomRecord.price`**（服务已 join room_types；type_name 未匹配时为 0，需提示）；
- 上传：入住 3 图 `PickImageFile("uploads/checkin", 2*1024*1024)`（png/jpg，<2MB）；合同 `PickPdfFile("uploads/contract", 2*1024*1024)`（≤2MB），只读显示「{老人姓名}的入住合同.pdf」+「查看」链接（`platform::OpenFileWithShell(path)`）；取消静默、超限/失败 Toast 红字。
- 图片只读态 100×100（WIC `DrawImageContain`）；无图显示「暂无图片」。
