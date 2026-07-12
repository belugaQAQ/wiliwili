# Tasks

## 阶段一：基础构建环境

- [x] Task 1: 配置 Android NDK 交叉编译 CMake 工具链
  - [x] 1.1 在根 `CMakeLists.txt` 中新增 `PLATFORM_ANDROID` 选项及 `cmake_dependent_option` 定义
  - [x] 1.2 在 `cmake/extra.cmake` 中添加 Android NDK 配置（API level、ABI、stl 选择）
  - [x] 1.3 在 `library/CMakeLists.txt` 中添加 `PLATFORM_ANDROID` 分支，配置 cpr/curl 使用 mbedtls 后端
  - [x] 1.4 添加 `ANDROID` 编译宏定义 `-D__ANDROID__`

- [x] Task 2: 编译 libmpv for Android NDK
  - [x] 2.1 创建 `scripts/android/mpv/` 目录，编写 mpv 交叉编译脚本（基于 NDK + meson）
  - [x] 2.2 创建 `scripts/android/ffmpeg/` 目录，编写 ffmpeg 交叉编译脚本
  - [x] 2.3 验证编译产物：libmpv.so + libavcodec.so 等
  - [x] 2.4 （可选）创建 Docker 构建镜像 `scripts/android/Dockerfile`

- [x] Task 3: 创建 Gradle 项目结构
  - [x] 3.1 创建 `android/` 目录，包含 `build.gradle`、`settings.gradle`、`app/build.gradle`
  - [x] 3.2 创建 Java 启动 Activity（`MainActivity.java`），继承 SDL Activity 或自定义 NativeActivity
  - [x] 3.3 配置 `AndroidManifest.xml`（声明为 Leanback/TV 应用、权限、横屏模式）
  - [x] 3.4 配置 Gradle CMake 集成，自动编译 native 库

## 阶段二：平台适配层

- [x] Task 4: 实现 borealis Android 平台适配层
  - [x] 4.1 实现 `brls::Platform` 子类：窗口创建（ANativeWindow + EGL）、主循环、生命周期回调
  - [x] 4.2 实现 `brls::InputManager` 子类：将 Android KeyEvent 映射为 borealis 按键事件
  - [x] 4.3 实现 `brls::VideoContext` 子类：EGL 上下文创建与交换、全屏切换
  - [x] 4.4 实现平台注册逻辑：在 `main.cpp` 中根据 `__ANDROID__` 宏选择 Android 平台实现

- [x] Task 5: MPV Android 渲染适配
  - [x] 5.1 在 `mpv_core.hpp` 中添加 `__ANDROID__` 条件分支，选择 OpenGL ES + EGL 渲染路径
  - [x] 5.2 在 `mpv_core.cpp` 中实现 Android EGL context 获取逻辑（替代 GLFW/SDL 的 proc-address 获取）
  - [x] 5.3 配置硬解码默认值为 `mediacodec-copy`
  - [x] 5.4 处理 Android Surface 生命周期：Surface 创建/销毁时 mpv render context 的重连

## 阶段三：wiliwili 业务层适配

- [x] Task 6: 适配 wiliwili 平台相关代码
  - [x] 6.1 修改 `config_helper.cpp`：Android 配置目录路径（`/data/data/<pkg>/files/wiliwili/`）
  - [x] 6.2 修改 `version_helper.cpp`：`getPlatform()` 返回 `"android"`
  - [x] 6.3 修改 `image_helper.hpp`：Android 使用 PC 相同的图片尺寸后缀（确认无需修改）
  - [x] 6.4 修改 `setting_activity.cpp`：Android 平台隐藏不适用设置项（如窗口置顶、NSP 转发等）
  - [x] 6.5 修改 `mpv_core.hpp`：默认硬解模式为 `mediacodec-copy`
  - [x] 6.6 修改 `crash_helper.cpp`：Android 崩溃处理（使用 logcat 或 tombstone）

- [x] Task 7: Android TV 输入与焦点优化
  - [x] 7.1 映射 Android TV 遥控器按键（D-Pad、确认、返回、菜单）到 borealis 导航
  - [x] 7.2 映射 Android 手柄按键（通过 Android Controller API）到游戏操作
  - [x] 7.3 适配焦点导航样式（确保 D-Pad 导航视觉反馈明显）

## 阶段四：打包与验证

- [x] Task 8: APK 打包与资源集成
  - [x] 8.1 将 `resources/` 目录集成到 APK assets 中
  - [x] 8.2 将编译的 native 库（libwiliwili.so、libmpv.so 等）打包到 APK 的 `lib/<abi>/` 目录
  - [x] 8.3 添加 Android TV 启动器图标和横幅图
  - [x] 8.4 配置 ProGuard 规则和签名

- [ ] Task 9: 集成测试与优化
  - [ ] 9.1 在 Android TV 模拟器上验证启动、UI 渲染、视频播放
  - [ ] 9.2 在实体 Android TV 设备上验证遥控器导航、视频流畅度
  - [ ] 9.3 性能优化：内存占用、渲染帧率、视频解码效率
  - [ ] 9.4 验证应用生命周期：后台切换、低内存回收后恢复

# Task Dependencies
- Task 2 依赖 Task 1（需要 CMake 配置确定 NDK 参数）
- Task 3 依赖 Task 1（Gradle 集成 CMake）
- Task 4 依赖 Task 3（需要 NativeActivity/Surface 环境）
- Task 5 依赖 Task 2 + Task 4（需要 libmpv.so + EGL 上下文）
- Task 6 依赖 Task 4（需要平台宏 `__ANDROID__` 生效）
- Task 7 依赖 Task 4（需要输入系统就绪）
- Task 8 依赖 Task 5 + Task 6 + Task 7（所有组件就绪后打包）
- Task 9 依赖 Task 8（需要可安装的 APK）
