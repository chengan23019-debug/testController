# GD32F470 测功机 Python 上位机通信与开发指南文档

本文档详细记录了针对基于 **GD32F470** 主控芯片的测功机（Dynamometer）系统，使用 **Python 语言** 开发上位机通信客户端的架构设计、二进制协议规约、Python SDK 接口使用说明以及扩展二次开发指南。

---

## 一、 系统架构与通信流向

```mermaid
graph LR
    subgraph 下位机 (GD32F470 控制器)
        Sensor[CS1238 扭矩 / HLW8112 电参量 / 转速] --> GlobalData[全局数据中心 app_dyno_global]
        GlobalData --> CtrlTask[100Hz 加载控制任务 (控制 DAC 输出)]
        GlobalData --> LwIP[LwIP TCP Server (Port 8080)]
    end

    subgraph 上位机 (PC / Python 客户端)
        LwIP <== 50Hz 主动数据流 (0x01) ==> PyRx[Python 后台接收解包线程]
        PyRx --> PyGUI[Python 终端 / Qt / Matplotlib 动态曲线]
        PyGUI == 模式切换 / 目标设定 / 急停指令 (0x10/0x11) ==> LwIP
    end
```

---

## 二、 二进制通信协议详解 (Protocol Specification)

通信采用二进制紧凑帧格式，兼顾高效数据压缩、低 CPU 开销与 CRC16 校验防护。

### 2.1 帧结构定义 (Frame Structure)

| 字节偏移 (Bytes) | 字段名 | 数据类型 | 字节序 | 示例值 | 说明 |
| :---: | :--- | :---: | :---: | :---: | :--- |
| `0 ~ 1` | **Header** | `uint16_t` | 大端 (Big-Endian) | `0x55 0xAA` | 帧同步头 (固定值) |
| `2` | **Cmd ID** | `uint8_t` | - | `0x01` | 功能码 |
| `3` | **Seq** | `uint8_t` | - | `0x01 ~ 0xFF` | 帧序号（递增，用于丢包检测） |
| `4 ~ 5` | **Length** | `uint16_t` | 大端 (Big-Endian) | `0x00 0x2A` | Payload 实际长度 (42 字节) |
| `6 ~ N` | **Payload** | `bytes` | 小端 (Little-Endian) | `[...]` | 业务有效载荷 |
| `N+1 ~ N+2` | **CRC16** | `uint16_t` | 小端 (Little-Endian) | `0x12 0x34` | CRC16-Modbus 校验和 |

---

### 2.2 核心 Payload 数据格式

#### 1. `0x01` - 50Hz 高频测试数据主动上报帧（MCU $\rightarrow$ Python 上位机）
* **Payload 尺寸**：42 字节 (`struct` 格式字符: `<IBBfffffffff`)

| 相对偏移 | 变量名 | 类型 | C 类型 | 单位 | 说明 |
| :---: | :--- | :---: | :---: | :---: | :--- |
| `0 ~ 3` | `timestamp_ms` | `uint32` | `uint32_t` | ms | 系统运行毫秒时间戳 |
| `4` | `work_mode` | `uint8` | `uint8_t` | - | 工作模式 (`0`:Idle, `1`:Manual, `2`:Const_T, `3`:Const_P, `4`:EStop) |
| `5` | `alarm_flags` | `uint8` | `uint8_t` | - | 报警位集合 (`Bit0`:超扭矩, `Bit1`:超速, `Bit3`:急停) |
| `6 ~ 9` | `torque_nm` | `float` | `float` | N·m | 实时实际扭矩 (CS1238 采样) |
| `10 ~ 13` | `speed_rpm` | `float` | `float` | RPM | 实时实际转速 |
| `14 ~ 17` | `mech_power_w` | `float` | `float` | W | 计算机械功率 $P = T \times n / 9.549$ |
| `18 ~ 21` | `dac_voltage` | `float` | `float` | V | 当前制动器 DAC 输出控制电压 |
| `22 ~ 25` | `elec_voltage` | `float` | `float` | V | HLW8112 采样交流电压 |
| `26 ~ 29` | `elec_current` | `float` | `float` | A | HLW8112 采样交流电流 |
| `30 ~ 33` | `elec_power` | `float` | `float` | W | HLW8112 采样输入电功率 |
| `34 ~ 37` | `power_factor` | `float` | `float` | - | 功率因数 |
| `38 ~ 41` | `efficiency` | `float` | `float` | % | 测功机总效率 $= (P_{mech} / P_{elec}) \times 100\%$ |

#### 2. `0x10` - 下发工作模式与目标设定值（Python 上位机 $\rightarrow$ MCU）
* **Payload 尺寸**：5 字节 (`struct` 格式字符: `<Bf`)
  - `target_mode` (`uint8`): 目标模式 (1: 手动开环 DAC, 2: 恒扭矩, 3: 恒功率)
  - `target_value` (`float`): 目标设定值 (DAC 电压 V / 扭矩 N·m / 功率 W)

#### 3. `0x11` - 紧急停止 (E-Stop)（Python 上位机 $\rightarrow$ MCU）
* **Payload 尺寸**：0 字节。MCU 收到后立即将 DAC 强制置 0V，切入 `DYNO_MODE_ESTOP` 锁定状态。

#### 4. `0x80` - 指行确认应答包 ACK（MCU $\rightarrow$ Python 上位机）
* **Payload 尺寸**：2 字节 (`struct` 格式字符: `<BB`)
  - `reply_cmd_id` (`uint8`): 被响的功能码
  - `status_code` (`uint8`): `0`: 成功, `1`: 模式非法, `2`: 超限拒绝, `3`: CRC/长度错误

---

## 三、 Python SDK (`dyno_client.py`) 代码解析

通信客户端 [dyno_client.py](file:///d:/code/testController/testController/test_v2/dyno_client.py) 包含了面向对象的 `DynoClient` 类，其核心逻辑如下：

### 3.1 核心解包与状态机代码

```python
import socket
import struct
import threading

class DynoClient:
    def _rx_worker(self):
        """后台流解包状态机：处理半包、黏包与 CRC 校验"""
        state = 0
        rx_buf = bytearray()
        raw_frame = bytearray()

        while self.running:
            data = self.sock.recv(512)
            if not data: break

            for b in data:
                if state == 0:  # 搜寻 0x55
                    if b == 0x55: raw_frame = bytearray([b]); state = 1
                elif state == 1:  # 搜寻 0xAA
                    if b == 0xAA: raw_frame.append(b); state = 2
                    else: state = 1 if b == 0x55 else 0
                elif state == 2: cmd_id = b; raw_frame.append(b); state = 3
                elif state == 3: seq_num = b; raw_frame.append(b); state = 4
                elif state == 4: payload_len = b << 8; raw_frame.append(b); state = 5
                elif state == 5:
                    payload_len |= b; raw_frame.append(b); rx_buf = bytearray()
                    state = 6 if payload_len > 0 else 7
                elif state == 6:  # 读取 Payload
                    rx_buf.append(b); raw_frame.append(b)
                    if len(rx_buf) >= payload_len: state = 7
                elif state == 7: raw_frame.append(b); crc_low = b; state = 8
                elif state == 8:
                    raw_frame.append(b); crc_high = b; state = 0
                    calc_crc = calc_crc16_modbus(raw_frame[:-2])
                    rx_crc = (crc_high << 8) | crc_low
                    if calc_crc == rx_crc:
                        self._parse_payload(cmd_id, seq_num, rx_buf)
```

---

## 四、 Python 上位机 API 使用指南

### 4.1 基础调用示例

```python
from dyno_client import DynoClient

# 1. 实例化客户端 (指定 GD32 控制器 IP 及端口)
client = DynoClient(host="192.168.31.119", port=8080)

# 2. 连接控制器
if client.connect():
    # 3.1 切换为手动开环模式，设置 DAC 输出 1.5V
    client.set_mode_manual_dac(dac_voltage=1.5)
    
    # 3.2 切换为恒扭矩模式，设置目标扭矩为 5.0 N.m
    # client.set_mode_const_torque(target_torque_nm=5.0)

    # 3.3 切换为恒功率模式，设置目标功率为 300 W
    # client.set_mode_const_power(target_power_w=300.0)

    # 3.4 触发急停
    # client.emergency_stop()

    # 3.5 切为空闲/停止模式 (DAC=0V)
    # client.set_mode_idle()

    # 4. 断开连接
    # client.disconnect()
```

---

## 五、 上位机扩展二次开发指南 (如 Matplotlib 绘图 / PyQt GUI)

如果您希望进一步开发图形界面（GUI）：

### 5.1 数据保存到 CSV 文件
可以在 `_parse_payload()` 函数的 `CMD_TELEMETRY_REPORT` 分支中追加以下代码：
```python
with open("dyno_test_log.csv", "a") as f:
    f.write(f"{ts},{mode_val},{torque:.4f},{speed:.2f},{mech_pwr:.2f},{dac_v:.3f},{elec_v:.2f},{elec_i:.3f},{elec_pwr:.2f}\n")
```

### 5.2 对接 PyQt5 / PySide6 图形界面
建议使用信号槽（`pyqtSignal`）将解析得到的数据传导至 Qt UI 主线程，使用 `pyqtgraph` 或 `Matplotlib` 绘制 50Hz 实时扭矩-转速-功率三曲线。

---

## 六、 部署与调试步骤

1. **GD32 烧录与启动**：在 Keil 中重新全编译并烧录至 GD32F470 开发板，确保网线连接至同一局域网。
2. **确认 IP 地址**：通过 GD32 串口 1（115200 波特率）打印日志，确认以太网 `LINK UP` 并获取 IP（如 `192.168.31.119`）。
3. **运行 Python 脚本**：
   ```bash
   python dyno_client.py 192.168.31.119
   ```
4. **控制测试**：在终端输入 `m 2.0` 测试 DAC 是否输出 2.0V 电压，输入 `e` 测试急停功能。
