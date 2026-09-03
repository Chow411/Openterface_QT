# Linux 设备枚举问题总结

## 背景

Windows 在 commit `7b36466` 中修复了 USB 3.0 跨控制器设备配对问题：当串口芯片 (1A86:FE0C) 和复合设备 (345F:2132) 枚举到不同 USB 控制器时，原有的 port chain 邻近性匹配失效。Windows 通过新增 `mergeUsb30CompanionDevices()` 实现了基于 VID/PID 的精确配对。

Linux 存在类似场景，但使用了不同的应对机制（启发式规则 + 枚举顺序修复），存在潜在风险。

---

## 问题 1：多设备跨控制器枚举错误配对

**严重程度**: 🔴 高

**位置**: `device/platform/LinuxDeviceManager.cpp`
- `arePortChainsRelatedLinux()` 行 1396-1404
- `associateSerialDevicesLinux()` 行 966-1054

**现象**:

当多台 Openterface 设备的串口和复合设备分别枚举到不同 USB 控制器时，`arePortChainsRelatedLinux()` 中 "不同 bus = 可能相关" 的规则会将任意跨 bus 的设备标记为相关，导致初始配对错误。虽然有基于 USB 枚举顺序 (busnum*1000+devnum) 的冲突修复机制，但该修复依赖 "同一设备的 companion 和 serial 枚举键差值最小" 的假设。当不同 USB 控制器的 busnum 差异压过 devnum 差异时，修复会失败，导致设备交叉配对。

**失败场景**:

```
USB Controller A (bus 3):
  设备 B 的 companion (devnum=5)
  设备 A 的 companion (devnum=15)

USB Controller B (bus 5):
  设备 A 的 serial (devnum=10)
  设备 B 的 serial (devnum=20)

枚举键: B_comp=3005, A_comp=3015, A_ser=5010, B_ser=5020

修复过程:
  B_companion (3005) → delta 最近的是 A_serial (5010) → 错误配对 ✗
  A_companion (3015) → 剩余 B_serial (5020) → 错误配对 ✗
```

**后果**: 键盘/鼠标操作跟随了错误的设备，屏幕画面跟随另一个设备。

---

## 问题 2：缺乏跨控制器精确配对机制

**严重程度**: ⚠️ 中

**位置**: `device/platform/LinuxDeviceManager.cpp` — `discoverDevicesBlocking()`

**现象**:

Windows 修复后有一个专门的 `mergeUsb30CompanionDevices()` pass，通过 VID/PID 精确匹配来合并跨控制器的设备对。Linux 没有对应机制，完全依赖：

1. `findSerialPortByCompanionDeviceLinux()` 的三级拓扑规则
2. `associateSerialDevicesLinux()` 的枚举顺序修复

这套机制是"先启发式匹配，再修复冲突"，不如 Windows 的"按 VID/PID 精确匹配"干净可靠。

**根因**: KVMGO 的串口芯片 (1A86:FE0C) 和复合设备 (345F:2132) 的 VID/PID 组合是唯一的，可以直接按 VID/PID 配对，不需要依赖 USB 拓扑。

---

## 问题 3：Gen3 VID/PID 常量可能不正确

**严重程度**: ⚠️ 待确认

**位置**:
- `device/platform/DeviceConstants.h` 行 27-28
- `device/platform/LinuxDeviceManager.cpp` 行 612-615

**现象**:

```cpp
// DeviceConstants.h
static const QString OPENTERFACE_VID_V3 = "345F";
static const QString OPENTERFACE_PID_V3 = "2109";  // ← 注意

// LinuxDeviceManager.cpp discoverGeneration3DevicesLinux()
qCDebug() << "Looking for V3 companion devices with VID/PID 345F:2109";
findUdevDevicesByVidPid("usb", OPENTERFACE_VID_V3, OPENTERFACE_PID_V3);
```

Gen2 复合设备使用 `345F:2132`，Gen3 搜索的是 `345F:2109`。Windows 侧的修复代码明确使用 `345F:2132` 作为 KVMGO 复合设备的 PID。

**需要确认**: Gen3 硬件实际使用的 PID 是 `2109` 还是 `2132`？如果是 `2132`，则 Gen3 设备发现会搜索错误的 VID/PID，完全找不到设备。

---

## 问题 4：单设备跨控制器依赖宽松规则碰巧正确

**严重程度**: 🟡 低（功能正确但实现脆弱）

**位置**: `arePortChainsRelatedLinux()` 行 1396-1404

**现象**:

当只有一台设备且串口和复合在不同 bus 上时，Rule 1 (期望端口+1) 和 Rule 2 (同 hub) 都会失败，最终靠 Rule 3 的 "不同 bus = 相关" 碰巧正确配对。这依赖于系统中只有一个串口芯片的事实。

**风险**: 代码逻辑上是"错误的方法碰巧得到正确的结果"，没有明确的文档说明这种设计意图，后续维护者可能修改或删除这个规则。

---

## 建议修复方案

参考 Windows 的 `mergeUsb30CompanionDevices()`，在 Linux 的 `discoverDevicesBlocking()` 末尾增加跨控制器配对 pass：

```
discoverDevicesBlocking()
  ├─ discoverGeneration1DevicesLinux()
  ├─ discoverGeneration2DevicesLinux()
  ├─ discoverGeneration3DevicesLinux()
  └─ mergeUsb30CompanionDevicesLinux()  ← 新增
       │
       ├─ 将设备分为 "仅 serial" 和 "仅 composite"
       ├─ 按 VID/PID 精确匹配 (1A86:FE0C ↔ 345F:2132)
       ├─ 合并 HID/camera/audio 到 serial 设备
       └─ 移除多余的 composite 条目
```

**优点**:
- 不依赖 USB 拓扑或枚举顺序假设
- 利用 KVMGO VID/PID 唯一性，配对精确
- 与 Windows 修复逻辑对齐，降低维护成本

---

## 影响范围

| 文件 | 修改内容 |
|------|----------|
| `device/platform/LinuxDeviceManager.h` | 新增 `mergeUsb30CompanionDevicesLinux()` 声明 |
| `device/platform/LinuxDeviceManager.cpp` | 新增实现，在 `discoverDevicesBlocking()` 中调用 |
| `device/platform/DeviceConstants.h` | 确认 Gen3 PID 是否正确 |
