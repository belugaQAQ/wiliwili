# 网页版 Baba Is You（M3E UI）Spec

## Why
用户希望使用 M3E（Material 3 Expressive）作为 UI 框架，以多文件结构制作一个网页版 Baba Is You 解谜游戏，包含多关卡且不简化核心机制，用于在浏览器中直接运行和体验。

## What Changes
- 新建独立的网页游戏项目，采用多文件模块化结构（HTML / CSS / TypeScript 或 JavaScript）。
- 使用 M3E Web Components 构建菜单、关卡选择、对话框、设置等 UI。
- 实现 Baba Is You 的完整核心机制：规则文本推动、规则解析、对象属性系统、胜负判定、撤销/重做。
- 内置多个可玩关卡，支持关卡切换与通关状态记录。
- 支持键盘（方向键 / WASD / 撤销 / 重置）和鼠标/触控操作。
- **BREAKING**: 本项目为全新代码，不依赖现有 wiliwili 代码库。

## Impact
- Affected specs: 无（新建项目）。
- Affected code: 新增 `projects/baba-is-you/` 或仓库根目录下的网页游戏文件；不修改现有 wiliwili 源码。

## ADDED Requirements

### Requirement: 项目结构与 M3E 集成
系统 SHALL 使用 npm / Vite 或原生 ESM 构建一个多文件网页项目，并集成 `@m3e/web` 组件库用于非游戏画布的 UI（菜单、按钮、对话框、关卡列表等）。

#### Scenario: 项目启动
- **WHEN** 用户运行 `npm install && npm run dev`
- **THEN** 项目成功启动本地服务器，M3E 组件正常渲染，无样式或加载错误。

### Requirement: 游戏核心机制
系统 SHALL 完整实现 Baba Is You 的核心规则系统，不简化：
- 规则由可推动的文本块构成：`<NOUN> IS <PROPERTY>` 和 `<NOUN> IS <NOUN>`。
- 支持 `AND` 连接多个属性或名词。
- 支持的方向性规则仅作为可选扩展，核心必须实现非方向性规则。
- 文本块本身具有 `PUSH` 属性（可推动）。
- 对象属性实时生效：YOU、WIN、STOP、PUSH、DEFEAT、SINK、HOT、MELT、OPEN、SHUT、MOVE、PULL 等。
- 胜负判定：当任意 `YOU` 对象与任意 `WIN` 对象重叠时进入胜利状态；当没有 `YOU` 对象存在时进入失败状态（可提示重试）。
- 对象转换：`NOUN IS NOUN` 规则实时将前一种对象转换为后一种对象。
- 规则失效：当文本块被推开导致规则不再连续时，对应效果立即消失。

#### Scenario: 推动规则并改变胜负条件
- **GIVEN** 关卡中存在 `BABA IS YOU` 和 `FLAG IS WIN`
- **WHEN** 玩家推动 `BABA` 文本块，使 `BABA IS YOU` 不再连续
- **THEN** Baba 不再受玩家控制；若重新排回 `BABA IS YOU`，Baba 恢复受控。

#### Scenario: 改变规则通关
- **GIVEN** 关卡中存在 `WALL IS STOP` 和 `FLAG IS WIN`，Baba 被墙挡住
- **WHEN** 玩家推动文本块形成 `WALL IS YOU` 并取消 `BABA IS YOU`
- **THEN** 玩家控制墙移动，并可通过墙的移动触碰胜利目标。

### Requirement: 对象与关卡
系统 SHALL 提供多种原始对象类型和多个内置关卡：
- 对象类型：Baba、Flag、Wall、Rock、Water、Skull、Grass、Flower、Door、Key 等。
- 至少 5 个内置关卡，难度递增，覆盖基础规则操作、属性转换、危险对象、开放/关闭机制。
- 关卡数据采用可扩展的 JSON / TOML / 纯文本格式，便于新增关卡。

#### Scenario: 加载关卡
- **WHEN** 玩家在关卡选择界面点击关卡
- **THEN** 游戏加载对应关卡布局并显示在屏幕上。

### Requirement: 输入与撤销
系统 SHALL 支持：
- 键盘方向键 / WASD 移动。
- 空格键或按钮撤销上一步（支持多步撤销）。
- R 键或按钮重置当前关卡。
- 鼠标/触控点击相邻格子移动（可选，作为辅助输入）。

#### Scenario: 撤销操作
- **WHEN** 玩家移动后按下撤销键
- **THEN** 游戏状态回退到移动前，包括规则变化引起的属性变化。

### Requirement: UI 与流程
系统 SHALL 使用 M3E 组件实现：
- 主菜单（开始游戏、关卡选择、操作说明）。
- 关卡选择界面（显示关卡列表和通关状态）。
- 游戏内 HUD（当前关卡、撤销/重置按钮、返回菜单）。
- 胜利/失败对话框。
- 操作说明页。

#### Scenario: 通关后进入下一关
- **WHEN** 玩家达成胜利条件
- **THEN** 显示胜利对话框，提供“下一关”和“返回关卡选择”选项。

## MODIFIED Requirements
无。

## REMOVED Requirements
无。
