# GuidePath AutoTurn 1.1

基于DLmember的模组加载器 https://github.com/DLmember/DLmodloader 开发。
Hero 经过 GuidePathTap(Clone) 时调用游戏原生转向，每个点在一局内最多触发一次。

## 使用

1. 通过游戏目录的 DLModLoader.exe 启动，进入关卡后手动开始；自动转向默认开启。
2. F8：切换自动转向。
3. F9：强制激活当前已加载场景内名为 HintPath 或 HintPath(Clone) 的对象及其父节点，并查找 `HintPathsToggle` 脚本，重新执行其 `Start()`，确保 `button.onClick.AddListener(ButtonClicked)` 存在。包含未激活对象；关卡未开始、暂停或 F8 关闭时也可用（需要游戏继续调用 Hero.Update）。这是一次性激活，若关卡再次隐藏它，可再按 F9。
4. 热键只在游戏窗口位于前台时生效，按住不重复触发。
5. 日志：DLMods/loader.log。卸载时退出游戏并移走 guide_path_auto_turn.dll。

## 本次增加

- 检测完全按世界坐标 XZ 平面进行，忽略 Y 高度，水平容差 0.18。
- 每次检测到关卡开始或玩家对象更换时，自动转向保护 300 毫秒（真实时间，暂停不冻结此计时）。保护期间更新位置，不积累待转向。
- 出生位置重叠的引导点以及保护期间接触的引导点标记为已处理，避免保护结束后补转；隐藏后才发现的出生点也跳过。
- 保护只限制模组自动转向，不禁止玩家手动输入。

## 实现与限制

- 针对 GameAssembly.dll SHA-256：3C77806C205D92B837F152C447F9FDA708EA940E755B003B2BAB90587F09CE6D。
- 不修改磁盘上的游戏文件；挂接 Hero.Update，在 Unity 主线程执行。
- 保留原有 GameCharacter.ChangeDirection(false) 的输入状态检查。
- 引导点仍按原版方式从一个可见 GuidePathTap(Clone) 所在根节点收集，每 60 帧刷新位置；多个独立引导根节点、移动引导点和极密转弯仍需实测。
- XZ 相同但高度不同的点会视为同一平面上的接触，这是忽略 Y 的预期行为。
- F9 激活父节点可能同时显示这些父节点下的其他对象；不会强制修改 HintPath 所有子节点各自的 activeSelf。
- 每个引导点一局一次；重新开始重置。仅有引导点的路段自动转向。

## 构建与验证

运行 build.cmd 使用本机 VS 2019 x64 编译器构建并安装 DLL。
旧版源码、说明和 DLL 备份在 backup-v1.0。退出游戏后可用备份 DLL 回滚。
用户已确认 1.0 版实际加载和转向正常。1.1 的构建、独立逻辑测试及 DLL 入口检查结果见 verification.txt；游戏内 F9、重开保护与实际转向需要加载新版后验证。
