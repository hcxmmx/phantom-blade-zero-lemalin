# 工具入口

这些脚本随制作阶段逐步建立。按 [制作流程](../docs/制作流程.md) 分阶段运行，输入资源应先准备好。

## 当前主要入口

| 阶段 | 脚本 |
| --- | --- |
| PMX 导入及材质预览 | `import_preview.py` |
| 真实骨骼筛选、权重映射及厘米制 FBX | `prepare_game_model.py` |
| 眨眼、表情、眼神形态键 | `prepare_lively_model.py` |
| 辫子骨骼与 PMX 物理清单 | `investigate_hair.py` |
| UE 编辑器模块构建 | `build_editor.ps1` |
| 在制作项目内运行 UE Python | `run_unreal.ps1` |
| 初次全模导入和 IK Rig / Retargeter | `ue_build_prototype.py` |
| 基础材质与显示覆盖 | `ue_prepare_game.py` |
| 法线/AO 与明暗档位 | `prepare_surface_detail.py`、`ue_apply_surface_detail.py`、`prepare_sun_comparison.py`、`ue_prepare_sun_comparison.py` |
| 黑色风格及最终丝袜 | `prepare_black_style.py`、`ue_build_black_style.py`、`prepare_sheer_stockings.py`、`ue_build_sheer_stockings.py` |
| 眼睛表面参数与虹膜图封装 | `prepare_eye_finish.py`、`prepare_eye_gem.ps1`、`ue_build_eye_finish.py` |
| 动态、表情、面板与最终显示覆盖 | `ue_build_lively.py` |
| 游戏 UI 参考导出和编辑器预览资源 | `NativeUiProbe/`、`ue_prepare_native_ui.py` |
| 语音下载、试听与事件替换 | `investigate_lemalin_voice.py`、`build_lemalin_voice_audition.py`、`build_lemalin_voice.py` |
| 少量原版响度样本提取 | `VoiceLoudnessProbe/` |
| 烹饪和白名单 IoStore 打包 | `cook.ps1`、`package_test.py` |
| 从已验收构建整理网盘安装包 | `package_player_release.py` |

## 历史诊断与实验入口

`audit_*`、`diagnose_*`、`preview_*`、`render_hair_probe.py`、`visualize_hair_investigation.py`、`verify_*` 与 UE 捕获/探针脚本用于当时的问题定位，通常读取特定版本的报告或参考动作。`verify_package.py` 中包含旧 Test 版本的资产数断言，使用前需要结合自己的产物调整。

`ue_build_sun_comparison.py`、眼睛/黑色/丝袜构建脚本可能在末尾调用动态生成器。批量准备材质时可参考制作流程，最后统一生成玩家面板。

HDR 实验源文件保留在源码快照中，当前玩家构建使用底色补偿方案。旧版图、预览与前作编辑器辅助代码也保留在 `HcxEditor` 模块，函数声明和实现之间存在编译依赖。

`deploy_test.ps1`、`deploy_voice.ps1`、`restore_previous.ps1` 和 `read_back_diagnostics.py` 保留了制作机的路径及备份约定，供参考原迭代过程。正式安装采用玩家说明中的手动文件夹方式。移植这些部署脚本时，需要重新配置自己的游戏路径、原有文件清单及备份位置。

## 环境变量

| 名称 | 作用 |
| --- | --- |
| `UNREAL_ROOT` | UE 5.7 安装目录，供构建包装脚本与 `package_test.py` 使用 |
| `LEMALIN_PLAYER_RELEASE=1` | 生成精简的玩家面板和玩家保存槽 |
| `LEMALIN_MATERIALS_ONLY=1` | 批量准备外观材质，延后重建动态和面板 |
| `LEMALIN_REUSE_LIVELY_MESH=1` | 复用已有动态网格，只重建蓝图等资源 |
| `LEMALIN_SKIP_VERIFICATION=1` | 跳过生成器中的离线运行检查，用于已确认功能的快速迭代 |
| `LEMALIN_REFERENCE_ROOT` | 自备参考工作目录，保留本次沿用的 2B 相对输入结构 |
| `PBZ_PAKS` | 两个 CUE4Parse 探针使用的原游戏 `Content/Paks` 目录 |
| `LEMALIN_ROOT` | NativeUiProbe 的输出工作目录，默认当前工作目录 |
| `RETOC` | 语音打包使用的 retoc 可执行文件路径 |

UE Python 的工作根目录由当前 `.uproject` 位置推导。Blender 和普通 Python 脚本由各自脚本路径推导工作根目录。Blender 5.2 的 MMD Tools 安装路径目前沿用 Windows 默认扩展目录，安装位置不同时需要调整相应 `parser` / `addon` 路径。
