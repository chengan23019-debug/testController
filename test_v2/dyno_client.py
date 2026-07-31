#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
===============================================================================
 GD32F470 测功机(Dynamometer) TCP 高频数据实时接收与控制 Python 客户端
===============================================================================
功能特性：
  1. TCP 连接 GD32F470 控制器 (默认 192.168.31.119:8080)
  2. 后台线程以 50Hz 实时接收、解包并校验 0x55 0xAA 自定义二进制协议数据帧
  3. 实时输出机械参量(扭矩/转速/功率)、控制量(DAC电压)及被测电机电参量(电压/电流/功率/效率)
  4. 支持命令行下发指令：手动设置 DAC、恒扭矩控制、恒功率控制、紧急停止 (E-Stop)
===============================================================================
"""

import socket
import struct
import threading
import time
import sys

# 协议常量
HEADER_HIGH = 0x55
HEADER_LOW  = 0xAA

CMD_TELEMETRY_REPORT = 0x01
CMD_SET_MODE         = 0x10
CMD_ESTOP            = 0x11
CMD_SET_PID          = 0x12
CMD_ACK_RESPONSE     = 0x80

MODE_NAMES = {
    0: "IDLE (空闲/停止)",
    1: "MANUAL (开环手动DAC)",
    2: "CONST_TORQUE (恒扭矩闭环)",
    3: "CONST_POWER (恒功率闭环)",
    4: "ESTOP (急停锁定)"
}

def calc_crc16_modbus(data: bytes) -> int:
    """计算 CRC16-Modbus 校验和"""
    crc = 0xFFFF
    for byte in data:
        crc ^= byte
        for _ in range(8):
            if crc & 0x0001:
                crc = (crc >> 1) ^ 0xA001
            else:
                crc >>= 1
    return crc

class DynoClient:
    def __init__(self, host: str = "192.168.31.119", port: int = 8080):
        self.host = host
        self.port = port
        self.sock = None
        self.running = False
        self.seq = 0
        self.rx_thread = None
        
        # 接收统计
        self.pkt_count = 0
        self.start_time = time.time()

    def connect(self) -> bool:
        """连接 GD32 TCP 服务器"""
        try:
            print(f"[*] 正在连接测功机控制器 [{self.host}:{self.port}]...")
            self.sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
            self.sock.settimeout(5.0)
            self.sock.connect((self.host, self.port))
            self.sock.settimeout(None)
            self.running = True
            
            # 启动后台数据流接收线程
            self.rx_thread = threading.Thread(target=self._rx_worker, daemon=True)
            self.rx_thread.start()
            print(f"[+] 成功连通 GD32 测功机控制器！数据上报监听中...\n")
            return True
        except Exception as e:
            print(f"[-] 连接失败: {e}")
            return False

    def disconnect(self):
        """断开连接"""
        self.running = False
        if self.sock:
            try:
                self.sock.close()
            except Exception:
                pass
        print("[*] TCP 连接已关闭。")

    def _rx_worker(self):
        """后台流解析状态机"""
        state = 0
        cmd_id = 0
        seq_num = 0
        payload_len = 0
        rx_buf = bytearray()
        raw_frame = bytearray()

        while self.running:
            try:
                data = self.sock.recv(512)
                if not data:
                    print("[-] 服务器断开连接。")
                    self.running = False
                    break

                for b in data:
                    if state == 0:  # 搜寻 0x55
                        if b == HEADER_HIGH:
                            raw_frame = bytearray([b])
                            state = 1
                    elif state == 1:  # 搜寻 0xAA
                        if b == HEADER_LOW:
                            raw_frame.append(b)
                            state = 2
                        else:
                            state = 1 if b == HEADER_HIGH else 0
                    elif state == 2:  # Cmd ID
                        cmd_id = b
                        raw_frame.append(b)
                        state = 3
                    elif state == 3:  # Seq
                        seq_num = b
                        raw_frame.append(b)
                        state = 4
                    elif state == 4:  # Len 高字节
                        payload_len = b << 8
                        raw_frame.append(b)
                        state = 5
                    elif state == 5:  # Len 低字节
                        payload_len |= b
                        raw_frame.append(b)
                        rx_buf = bytearray()
                        if payload_len > 256:
                            state = 0  # 超长包无效，丢弃
                        elif payload_len == 0:
                            state = 7  # 无 payload，读 CRC
                        else:
                            state = 6
                    elif state == 6:  # 读取 Payload
                        rx_buf.append(b)
                        raw_frame.append(b)
                        if len(rx_buf) >= payload_len:
                            state = 7
                    elif state == 7:  # CRC 低字节
                        raw_frame.append(b)
                        crc_low = b
                        state = 8
                    elif state == 8:  # CRC 高字节
                        raw_frame.append(b)
                        crc_high = b
                        state = 0  # 完成解包，重置状态

                        # CRC16 校验 (除最后 2 字节 CRC 外的所有内容)
                        calc_crc = calc_crc16_modbus(raw_frame[:-2])
                        rx_crc = (crc_high << 8) | crc_low

                        if calc_crc == rx_crc:
                            self._parse_payload(cmd_id, seq_num, rx_buf)
                        else:
                            print(f"[!] CRC 校验失败: 计算值 0x{calc_crc:04X} != 接收值 0x{rx_crc:04X}")

            except Exception as e:
                if self.running:
                    print(f"[-] 接收错误: {e}")
                break

    def _parse_payload(self, cmd_id: int, seq: int, payload: bytes):
        """解析并输出数据包"""
        if cmd_id == CMD_TELEMETRY_REPORT:
            # 50Hz 实时数据包格式: <I B B f f f f f f f f f (42 Bytes)
            if len(payload) >= 42:
                self.pkt_count += 1
                (ts, mode_val, alarm, 
                 torque, speed, mech_pwr, 
                 dac_v, 
                 elec_v, elec_i, elec_pwr, pf, eff) = struct.unpack("<IBBfffffffff", payload[:42])

                mode_str = MODE_NAMES.get(mode_val, f"未知({mode_val})")
                
                # 格式化终端输出
                output = (
                    f"\r[50Hz 包#{self.pkt_count:05d} | {ts}ms] "
                    f"模式: {mode_str:<22} | "
                    f"扭矩: {torque:7.3f} N.m | "
                    f"转速: {speed:6.1f} RPM | "
                    f"机械功率: {mech_pwr:7.2f} W | "
                    f"DAC控制: {dac_v:5.3f} V || "
                    f"电机电压: {elec_v:5.1f} V | "
                    f"电流: {elec_i:6.3f} A | "
                    f"电功率: {elec_pwr:7.2f} W | "
                    f"效率: {eff:5.1f} %"
                )
                print(output, end="", flush=True)

        elif cmd_id == CMD_ACK_RESPONSE:
            if len(payload) >= 2:
                reply_cmd, status = struct.unpack("<BB", payload[:2])
                status_str = "成功" if status == 0 else f"错误码({status})"
                print(f"\n[ACK 应答] 针对功能码 0x{reply_cmd:02X} 的指令执行结果: {status_str}")

    def send_cmd(self, cmd_id: int, payload: bytes = b""):
        """组包并向 GD32 下发控制指令"""
        if not self.sock or not self.running:
            print("[-] 未连接控制器！")
            return

        self.seq = (self.seq + 1) & 0xFF
        header = struct.pack(">BBBBH", HEADER_HIGH, HEADER_LOW, cmd_id, self.seq, len(payload))
        frame_no_crc = header + payload
        crc = calc_crc16_modbus(frame_no_crc)
        frame = frame_no_crc + struct.pack("<H", crc)

        try:
            self.sock.sendall(frame)
        except Exception as e:
            print(f"[-] 指令发送失败: {e}")

    def set_mode_manual_dac(self, dac_voltage: float):
        """下发指令：切为开环手动模式，直接输出指定 DAC 电压(V)"""
        print(f"\n[*] 下发指令 -> 切换开环手动模式，目标 DAC 电压 = {dac_voltage:.3f} V")
        payload = struct.pack("<Bf", 1, dac_voltage)
        self.send_cmd(CMD_SET_MODE, payload)

    def set_mode_const_torque(self, target_torque_nm: float):
        """下发指令：切为恒扭矩闭环模式，设定目标扭矩(N.m)"""
        print(f"\n[*] 下发指令 -> 切换恒扭矩模式，目标扭矩 = {target_torque_nm:.3f} N.m")
        payload = struct.pack("<Bf", 2, target_torque_nm)
        self.send_cmd(CMD_SET_MODE, payload)

    def set_mode_const_power(self, target_power_w: float):
        """下发指令：切为恒功率闭环模式，设定目标功率(W)"""
        print(f"\n[*] 下发指令 -> 切换恒功率模式，目标功率 = {target_power_w:.2f} W")
        payload = struct.pack("<Bf", 3, target_power_w)
        self.send_cmd(CMD_SET_MODE, payload)

    def set_mode_idle(self):
        """下发指令：切为空闲/停止模式 (DAC = 0V)"""
        print("\n[*] 下发指令 -> 切换空闲/停止模式 (DAC = 0V)")
        payload = struct.pack("<Bf", 0, 0.0)
        self.send_cmd(CMD_SET_MODE, payload)

    def emergency_stop(self):
        """下发指令：紧急停止 E-Stop (立即强行切 0V)"""
        print("\n[!] 🚨 触发紧急停止 (E-STOP) 指令！")
        self.send_cmd(CMD_ESTOP, b"")


def main():
    # GD32F470 控制器默认 IP 及端口
    IP = "192.168.31.119"
    PORT = 8080

    if len(sys.argv) > 1:
        IP = sys.argv[1]

    client = DynoClient(IP, PORT)
    if not client.connect():
        return

    print("==========================================================================")
    print("                    GD32F470 测功机交互式测试控制台")
    print("==========================================================================")
    print(" 输入命令说明：")
    print("   0           - 停止/切为空闲模式 (DYNO_MODE_IDLE)")
    print("   m <V>       - 开环手动模式 (例如: m 1.5 设置 DAC 输出 1.5V)")
    print("   t <Nm>      - 恒扭矩模式   (例如: t 5.0 设置目标扭矩 5.0 N.m)")
    print("   p <W>       - 恒功率模式   (例如: p 500 设置目标功率 500 W)")
    print("   estop / e   - 🚨 紧急停止 (E-Stop)")
    print("   q / quit    - 退出脚本")
    print("==========================================================================")

    try:
        while client.running:
            cmd_input = input().strip().lower()
            if cmd_input in ['q', 'quit', 'exit']:
                break
            elif cmd_input == '0':
                client.set_mode_idle()
            elif cmd_input in ['e', 'estop']:
                client.emergency_stop()
            elif cmd_input.startswith('m '):
                try:
                    v = float(cmd_input.split()[1])
                    client.set_mode_manual_dac(v)
                except ValueError:
                    print("格式错误！示例: m 1.5")
            elif cmd_input.startswith('t '):
                try:
                    t_nm = float(cmd_input.split()[1])
                    client.set_mode_const_torque(t_nm)
                except ValueError:
                    print("格式错误！示例: t 5.0")
            elif cmd_input.startswith('p '):
                try:
                    p_w = float(cmd_input.split()[1])
                    client.set_mode_const_power(p_w)
                except ValueError:
                    print("格式错误！示例: p 500")
            else:
                print("未知的命令，请参照上方帮助输入。")

    except KeyboardInterrupt:
        print("\n捕获 Ctrl+C，正在退出...")
    finally:
        client.disconnect()

if __name__ == "__main__":
    main()
