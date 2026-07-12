# wiliwili Android TV 移植规格

## Why
wiliwili 目前支持 Switch、PSVita、PS4、PC 四类平台，尚无 Android 支持。Android TV 在客厅场景下与 Switch/PS4 重合度高，用户群体大，移植可显著扩大覆盖范围。

## What Changes
- 新增 `PLATFORM_ANDROID` CMake 构建选项及 NDK 交叉编译配置
- 新增 borealis 的 Android 平台适配层（窗口、输入、生命周期）
- 新增 mpv Android NDK 编译脚本及 libmpv 预构建方案
- 新增 Android TV 遥控器/手柄按键映射
- 新增 Android APK 打包流程（Gradle + native 库）
- 新增 `__ANDROID__` 条件编译分支处理平台差异

## Impact
- Affected specs: 构建系统、平台抽象层、MPV 渲染后端、输入系统、配置存储路径
- Affected code:
  - `CMakeLists.txt` — 新增 PLATFORM_ANDROID 选项
  - `cmake/extra.cmake` — NDK 工具链配置
  - `library/CMakeLists.txt` — Android 依赖（cpr/curl/mbedtls）编译选项
  - `wiliwili/include/view/mpv_core.hpp` — Android EGL 渲染路径
  - `wiliwili/source/view/mpv_core.cpp` — Android mpv 初始化
  - `wiliwili/source/utils/config_helper.cpp` — Android 配置目录路径
  - `wiliwili/source/utils/version_helper.cpp` — 平台标识
  - `wiliwili/include/utils/image_helper.hpp` — Android 图片尺寸后缀
  - 新增 `scripts/android/` — 构建脚本与 Docker 配置
  - 新增 `android/` — Gradle 项目结构

## ADDED Requirements

### Requirement: Android NDK 交叉编译构建
系统 SHALL 提供 `PLATFORM_ANDROID` CMake 选项，支持使用 Android NDK 交叉编译 wiliwili 原生库。

#### Scenario: 构建成功
- **WHEN** 执行 `cmake -B build -DPLATFORM_ANDROID=ON -DANDROID_ABI=arm64-v8a`
- **THEN** CMake 成功配置并生成 Android NDK 构建文件，编译输出 `.so` 原生库

### Requirement: borealis Android 平台适配层
系统 SHALL 实现 borealis 的 Android 平台适配层，提供窗口创建（ANativeWindow + EGL）、主循环、输入处理和生命周期回调。

#### Scenario: 窗口创建与渲染
- **WHEN** Android Activity 启动并创建 Surface
- **THEN** borealis 通过 EGL 创建 OpenGL ES 上下文，nanovg 成功初始化并渲染 UI

#### Scenario: 生命周期管理
- **WHEN** Android Activity 进入 onPause 状态
- **THEN** 视频暂停播放，GPU 资源释放；onResume 时恢复

### Requirement: MPV Android 渲染
系统 SHALL 在 Android 平台上使用 mpv 的 OpenGL 渲染后端（通过 EGL），视频绘制到独立 framebuffer（`MPV_USE_FB` 模式）。

#### Scenario: 视频播放
- **WHEN** 用户打开一个视频
- **THEN** mpv 通过 EGL + OpenGL ES 渲染视频帧到 FBO，borealis 将其贴到 UI 指定区域

#### Scenario: 降级到软渲染
- **WHEN** OpenGL ES 3.0+ 不可用
- **THEN** 自动降级到 `MPV_SW_RENDER` CPU 软渲染模式

### Requirement: Android TV 遥控器输入
系统 SHALL 将 Android TV 遥控器按键映射为 borealis 导航操作（方向键、确认、返回）。

#### Scenario: 遥控器导航
- **WHEN** 用户按下遥控器 D-Pad 方向键
- **THEN** UI 焦点按方向移动到下一个可交互元素

#### Scenario: 返回键
- **WHEN** 用户按下遥控器返回键
- **THEN** 关闭当前 Activity/Fragment，返回上一页

### Requirement: Android APK 打包
系统 SHALL 提供 Gradle 构建脚本，将原生库、资源文件和 Java 启动代码打包为可安装的 APK。

#### Scenario: APK 生成
- **WHEN** 执行 Gradle 构建
- **THEN** 生成包含 arm64-v8a 和 armeabi-v7a 两种架构的 APK

### Requirement: Android 配置目录
系统 SHALL 使用 Android 应用私有目录 (`/data/data/<pkg>/files/wiliwili/`) 作为配置存储路径。

#### Scenario: 配置读取
- **WHEN** wiliwili 在 Android 上启动
- **THEN** `ProgramConfig::getConfigDir()` 返回 Android 应用私有目录下的 wiliwili 子目录

## MODIFIED Requirements

### Requirement: 版本平台标识
`APPVersion::getPlatform()` SHALL 在 `__ANDROID__` 定义时返回 `"android"`。

### Requirement: 图片尺寸后缀
`ImageHelper` SHALL 在 `__ANDROID__` 平台上使用与 PC 相同的图片尺寸后缀（`@672w_378h` 等）。

### Requirement: 默认硬解模式
`MPVCore::PLAYER_HWDEC_METHOD` SHALL 在 `__ANDROID__` 平台上默认为 `"mediacodec-copy"`。

## REMOVED Requirements

（无移除项）
