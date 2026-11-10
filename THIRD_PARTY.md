# 来源与鸣谢

## 模型与角色

| 内容 | 来源 |
| --- | --- |
| 原模型 | studio chakapo 的恶毒 VRChat 模型 |
| PMX 优化适配 | Aimidi，当前 PMX 注释署名包括骨骼、表情、物理与 PMX 适配 |
| 角色及原语音 | 《碧蓝航线》 |
| 游戏运行资源 | 《影之刃零》本地客户端 |

原模型商品页与适配者公开作品可用于核对署名。当前制作输入为 `LeMaline v1.0.pmx`，SHA256 为 `6293cc5262f29b65db7504adad4cdeca9517c6d420f24faa150736b74eb2984c`。精确原版 FBX 版本及适配授权链尚未取得完整证据。[studio chakapo](https://booth.pm/ja/items/3669060)、[Aimidi](https://www.bilibili.com/video/BV1XY411g7j9/)

本仓库的 MIT 许可覆盖自写实现与文档。模型及其修改版资产的使用和分发范围仍需依据各自条款判断。原 PMX 附有群内使用限制，原模型商品页附有再分发限制，代码开源许可不扩展这些素材的许可范围。

## 语音资料

恶毒语音下载索引取自 Fernando2603/AzurLane，制作时固定的提交为 `21366bf104dccc515d084b33d61d98ff994dc7f2`。日文台词语义核对另参考 AzurLaneTools/AzurLaneLuaScripts。仓库保留语音地址索引与剪辑参数，原始录音、剪辑音频、原游戏银行和媒体模板由制作侧准备。[语音资源仓库](https://github.com/Fernando2603/AzurLane)、[索引结构说明](https://github.com/Fernando2603/AzurLane/blob/main/docs/VOICELINE.md)、[台词脚本](https://github.com/AzurLaneTools/AzurLaneLuaScripts)

## 制作工具

| 工具 | 本次用途 |
| --- | --- |
| Blender、MMD Tools | PMX 导入、骨架处理、形态键与 FBX 导出 |
| Unreal Engine 5.7 | 编辑器侧资产生成、重定向、原生 RigidBody 与烹饪 |
| CUE4Parse / CUE4Parse-Conversion | 游戏资源结构调查与制作参考资源导出 |
| retoc | 语音遗留资产转换为 UE5.7 IoStore 容器 |
| NumPy、Pillow、SciPy、SoundFile、imageio-ffmpeg | 数据计算、贴图处理、音频解码与处理 |

依赖通过各自发行渠道安装，具体依赖声明见 Python requirements 和两个 .NET 项目文件。游戏字体、贴图与材质在运行时按原路径引用，编辑器中的预览副本由制作侧从本地资源建立。

## 前作与参考项目

人体重定向、资产生成和 Wwise PCM 封装沿用了 Hcxmmx 的 2B 项目。独立制作副本、显示入口和部分生成框架沿用了加菲猫项目。保留下来的前作编辑器辅助文件在工具目录中标注用途。

收尾时参考过 Ciel 项目的武器握持实现，最终保留了当前 LeMalin 的手臂比例与握持效果。该项目可作为其他模型比例适配的参考。[yzr-zero-mod](https://github.com/627157746/yzr-zero-mod)
