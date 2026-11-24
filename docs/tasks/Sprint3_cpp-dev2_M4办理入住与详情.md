# Sprint 3 任务单 · cpp-dev2 · M4 办理入住表单 + 入住详情

> 负责人：cpp-dev2
> 可写文件：`src/pages/CheckInFormModule.cpp`、`src/pages/CheckInDetailModule.cpp`（仅这两个）
> 契约（只读）：`include/zzyl/data/BusinessServices.h`（`ICheckInService`/`CheckInRecord`）、`include/zzyl/app/AppState.h`（`CheckInSelection`）、`include/zzyl/app/Routes.h`、`include/zzyl/ui/Widgets.h`
> 必读：架构 v0.3 §3.3/§3.4/§5/§7；Sprint3 总览；Web 截图 `04_入退管理-入住办理.png`、`36_入住详情（隐藏）.png`；M4 规格 `Sprint3_M4入住办理规格_v0.1.md`（到位后按字段对齐）
> 骨架现状：两个页面为占位卡片；SQLite `check_ins` 已灌 5 条；路由 `/enterQuit/checkInForm`、`/enterQuit/checkInInfo` 可达。

## 1. 交付目标
### A. 办理入住表单（`/enterQuit/checkInForm`）
整页表单（多分区，卡片分组；底部 提交/取消）：
1. **基本信息**：老人姓名*、身份证号*（18 位校验）、性别、年龄、联系电话；
2. **家属信息**：家属姓名、关系、电话、地址（可先单条；多条可留 TODO 注明）；
3. **入住配置**：房型、床位号*、护理等级名称、入住开始日期*、预计退住日期、押金、费用；
4. **合同**：合同编号、签约日期、备注（上传按钮可占位提示「二期」）；
- 提交：本地校验（必填/身份证/日期顺序）→ `services->checkIn->Add(record, &err)` → 成功 Toast「办理成功」+ 清空 + 入队 `Navigate /enterQuit/checkIn`；失败 Toast 红字；
- 取消：`Navigate /enterQuit/checkIn`；
- `CheckInRecord` 字段不足时（规格要求在 `CheckInRecord` 加字段）**报架构加契约/迁移，不要自己改数据头**；可先放在页面 State 的扩展字段并在汇报标注。

### B. 入住详情（`/enterQuit/checkInInfo`）
- 读 `CheckInSelection.id` → `services->checkIn->Get(id, &record, &err)`；无选中 id 时提示「请从列表选择记录」并给返回按钮；
- 分区展示同表单（只读），「编辑」→ 可编辑态 → 保存：`Update` → Toast「保存成功」→ 回只读；
- 返回列表按钮：`Navigate /enterQuit/checkIn`。

## 2. 数据/状态
- 页面私有资源（各自在 Register 函数中 `world.EmplaceResource<...>()`）：FormState（字段缓冲/错误）、DetailState（record/编辑态/缓冲）；
- 只经 `DataServices.checkIn` 接口读写；不得 include `SqliteData.h/SeedServices.h`；
- 写库即持久化（SQLite），重启后数据仍在（验收点）。

## 3. 硬约束
- 只改上述 2 个文件；不改 CMake/契约头/数据层/他人页面；不 commit；
- 布局由 `pageViewport` 推导；表单分区高度/间距比例化；弹窗/Toast 用 `ui::` 组件；
- 字体作用域 RAII；中文文案；可见调试先向 cpp-control 申请；
- 身份证/日期校验用本地逻辑；提交前必须校验，失败保持页面并显示红字。

## 4. 自测与验收
```bat
cmake -S . -B build-dev2 -G "Visual Studio 18 2026" -A x64
cmake --build build-dev2 --config Release --parallel
build-dev2\Release\zzyl_desktop.exe --auto-login           :: 列表→发起入住申请→填表提交→列表 6 条
build-dev2\Release\zzyl_desktop.exe --selftest artifacts\selftest_dev2
```
- 表单四分区完整、必填/身份证/日期校验红字；提交成功 → 列表新增 6 条（dev1 列表页需可用；未合入前可用 SQLite 查询/重启验证持久化）；
- 详情：从列表「查看」进入 → 字段回显 → 编辑保存 → 重启仍在；
- 1024×640 / 1920 全屏不超屏；截图 `10_checkin_form_1280x720.png`、`11_checkin_detail_1280x720.png`；
- 汇报 lead（抄架构师）。

## 5. Bug 归属
- 表单/详情页问题 → **cpp-dev2**；
- 数据接口/字段缺失/迁移 → **cpp-arch**；
- 通用组件问题 → cpp-dev1（先报架构）。

## 6. v4 契约对齐（2026-11-24 晚 · M4 规格 v0.1 已到位，必读）
1. **`CheckInRecord` 已扩展为 4 分区 30 字段**（见 `BusinessServices.h`，架构 v0.3 §3.5 有对照表）：
   - 基本信息 10：`elderName/idCard/birthday/age/sex/phone/address/imagePath/idCardPortraitImg/idCardNationalEmblemImg`（图片本期**不做真实上传**，按钮占位提示「二期」；种子路径空→显示占位）；
   - 家属：`families[] { name, phone, kinship }`；**存储为独立子表 `check_in_family`**，`Get/List` 自动带出、`Add/Update` 事务替换——页面只需读写 `record.families`，不用 JSON；
   - 入住配置 10：`startDate/endDate/feeStartDate/feeEndDate/nursingLevelId/nursingLevelName/bedNumber/bedId/deposit/nursingFee/bedFee/otherFees/insurancePayment/governmentSubsidy`；**全部必填**；押金默认 3000；护理费/床位费只读由等级/房型带出；
   - 签约办理 5：`contractName/signDate/thirdPartyName/thirdPartyPhone/agreementPath`（合同名称/签约日期必填；PDF 上传本期占位）；
   - 状态：`status` int（0 已入住/1 已退住）、`createTime`、`remark`。
2. **护理等级下拉**：`services->checkIn->NursingLevels()` 返回 3 条（76/77/78 → 1500/2000/3000）；Web 接口缺失已本地化。
3. **床位级联简化（本期）**：本地无 floor/room/bed 表 → 床位号用**文本输入**（如 104-1），不做三级联动；Sprint 4 接床表后再升级。
4. 提交：`Add(record,&err)` → Toast「办理成功」→ 回列表；编辑：`Update`；详情：`Get(CheckInSelection.id)`。旧库已由架构迁移 v4，重建 `build-dev2` 后即可编译。
