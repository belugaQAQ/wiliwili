# Tasks

- [ ] Task 1: 初始化项目并集成 M3E
  - [ ] SubTask 1.1: 创建项目目录（如 `projects/baba-is-you/`），初始化 `package.json`。
  - [ ] SubTask 1.2: 安装 Vite（或等效构建工具）、TypeScript、`@m3e/web`、`@m3e/icons`。
  - [ ] SubTask 1.3: 配置 `vite.config.ts`、tsconfig、入口 `index.html` 与 `src/main.ts`。
  - [ ] SubTask 1.4: 在入口中导入 M3E 组件并验证按钮、对话框可正常渲染。

- [ ] Task 2: 定义游戏核心类型与关卡数据格式
  - [ ] SubTask 2.1: 定义 `ObjectType`、`TextType`、`Direction`、`Cell`、`Level` 等核心类型/接口。
  - [ ] SubTask 2.2: 设计关卡数据文件格式（JSON/TOML/文本），至少包含格子坐标、对象类型、文本块信息。
  - [ ] SubTask 2.3: 实现关卡加载器，将关卡数据解析为运行时对象。

- [ ] Task 3: 实现游戏世界与规则引擎
  - [ ] SubTask 3.1: 实现 `World` 类，管理格子、对象堆叠、移动、推动链。
  - [ ] SubTask 3.2: 实现规则扫描与解析：识别横向/纵向连续的 `NOUN IS PROPERTY` / `NOUN IS NOUN` / `AND` 规则。
  - [ ] SubTask 3.3: 实现属性系统：YOU、WIN、STOP、PUSH、DEFEAT、SINK、HOT、MELT、OPEN、SHUT、MOVE、PULL。
  - [ ] SubTask 3.4: 实现对象转换：`NOUN IS NOUN` 将源对象实时转换为目标对象。
  - [ ] SubTask 3.5: 实现胜负判定与失败检测（无 YOU 对象时失败）。

- [ ] Task 4: 实现输入、撤销与重置
  - [ ] SubTask 4.1: 实现键盘输入处理（方向键/WASD、空格撤销、R 重置）。
  - [ ] SubTask 4.2: 实现状态快照与多步撤销栈。
  - [ ] SubTask 4.3: 实现鼠标/触控点击相邻格子移动（可选）。
  - [ ] SubTask 4.4: 实现重置当前关卡功能。

- [ ] Task 5: 实现渲染器
  - [ ] SubTask 5.1: 实现基于 DOM 或 Canvas 的游戏画面渲染（文本块、对象、网格）。
  - [ ] SubTask 5.2: 添加对象动画与规则变化反馈。
  - [ ] SubTask 5.3: 实现相机/视口自适应，适配不同屏幕尺寸。

- [ ] Task 6: 使用 M3E 构建 UI 与流程
  - [ ] SubTask 6.1: 实现主菜单界面（开始游戏、关卡选择、操作说明）。
  - [ ] SubTask 6.2: 实现关卡选择界面，展示关卡列表与通关状态。
  - [ ] SubTask 6.3: 实现游戏内 HUD（当前关卡、撤销、重置、返回菜单）。
  - [ ] SubTask 6.4: 使用 M3E 对话框实现胜利/失败提示。
  - [ ] SubTask 6.5: 实现操作说明页。

- [ ] Task 7: 设计并内置多关卡
  - [ ] SubTask 7.1: 设计 5 个以上关卡，覆盖基础、属性转换、危险、开放关闭等机制。
  - [ ] SubTask 7.2: 将关卡数据写入可扩展文件。
  - [ ] SubTask 7.3: 验证每个关卡可正常加载、可解、通关判定正确。

- [ ] Task 8: 测试、验证与收尾
  - [ ] SubTask 8.1: 编写核心规则引擎单元测试（规则解析、移动、属性生效、撤销）。
  - [ ] SubTask 8.2: 运行全关卡可解性测试。
  - [ ] SubTask 8.3: 验证 M3E UI 各页面无样式/交互错误。
  - [ ] SubTask 8.4: 更新 README，说明启动方式、操作说明、关卡数量。

# Task Dependencies
- Task 3 依赖 Task 2
- Task 4 依赖 Task 3
- Task 5 依赖 Task 3
- Task 6 依赖 Task 2 与 Task 5
- Task 7 依赖 Task 2
- Task 8 依赖 Task 3、Task 4、Task 6、Task 7
