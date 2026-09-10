#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
===============================================================================
 GD32F470 测功机(Dynamometer) 中心服务端 (Center Server)
 支持 TCP_NODELAY 高性能推流接收、50Hz 遥测抖动统计与双向控制
===============================================================================
功能特性：
  1. 作为中心 TCP 服务端监听 0.0.0.0:8080，等待 GD32 节点主动上线连接（即插即用）
  2. 自动开启 socket.TCP_NODELAY，配合单片机 tcp_nagle_disable 彻底根除 40ms~200ms 抖动
  3. 50Hz 高频解析 0x55 0xAA 二进制协议遥测帧，实时统计包间隔 (dt ~ 20.0ms) 与传输抖动
  4. 交互式命令行控制下发：手动 DAC、恒扭矩闭环、恒功率闭环、PID 参数配置、紧急停机 (E-Stop)
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

class DynoCenterServer:
    def __init__(self, host: str = "0.0.0.0", port: int = 8080):
        self.host = host
        self.port = port
        self.server_sock = None
        self.client_sock = None
        self.client_addr = None
        self.running = False
        self.seq = 0
        
        # 统计指标与显示模式
        self.pkt_count = 0
        self.last_pkt_time = 0.0
        self.jitter_ms_list = []
        self.start_time = time.time()
        self.verbose = False  # False: 100ms平滑流模式, True: 50Hz全包逐帧输出模式

    def start(self):
        """启动 TCP 中心服务端"""
        self.server_sock = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        self.server_sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.server_sock.bind((self.host, self.port))
        self.server_sock.listen(5)
        self.running = True

        print(f"==================================================================")
        print(f"[*] GD32F470 测功机中心服务器已启动，监听 [{self.host}:{self.port}]...")
        print(f"[*] 等待 GD32F470 节点主动发起 TCP 连接上报 (即插即用)...")
        print(f"==================================================================")

        accept_thread = threading.Thread(target=self._accept_loop, daemon=True)
        accept_thread.start()

    def _accept_loop(self):
        """循环等待客户端连接"""
        while self.running:
            try:
                sock, addr = self.server_sock.accept()
                # 核心优化：上位机启用 TCP_NODELAY 禁用 Delayed ACK 干扰
                sock.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)

                print(f"\n[+] 检测到测功机节点上线接入！节点地址: {addr[0]}:{addr[1]}")
                print(f"[+] TCP_NODELAY 已开启，开始 50Hz 低延迟零抖动推流接收...\n")

                if self.client_sock:
                    try:
                        self.client_sock.close()
                    except Exception:
                        pass

                self.client_sock = sock
                self.client_addr = addr
                self.pkt_count = 0
                self.last_pkt_time = 0.0
                self.jitter_ms_list.clear()
                self.start_time = time.time()

                rx_thread = threading.Thread(target=self._rx_worker, args=(sock, addr), daemon=True)
                rx_thread.start()
            except Exception as e:
                if self.running:
                    print(f"[-] Accept 异常: {e}")
                break

    def _rx_worker(self, sock: socket.socket, addr):
        """流解析状态机"""
        state = 0
        cmd_id = 0
        seq_num = 0
        payload_len = 0
        rx_buf = bytearray()
        raw_frame = bytearray()

        first_pkt = True
        while self.running:
            try:
                data = sock.recv(2048)
                if not data:
                    print(f"\n[-] 测功机节点 [{addr[0]}:{addr[1]}] 已断开连接。", flush=True)
                    break

                if first_pkt:
                    first_pkt = False
                    print(f"[+] 首次接收到以太网数据流 ({len(data)} 字节)，正在解包校验...", flush=True)

                for b in data:
                    if state == 0:  # 等待 0x55
                        if b == HEADER_HIGH:
                            raw_frame = bytearray([b])
                            state = 1
                    elif state == 1:  # 等待 0xAA
                        if b == HEADER_LOW:
                            raw_frame.append(b)
                            state = 2
                        else:
                            state = 1 if b == HEADER_HIGH else 0
                    elif state == 2:  # 功能码 Cmd
                        cmd_id = b
                        raw_frame.append(b)
                        state = 3
                    elif state == 3:  # 帧序号 Seq
                        seq_num = b
                        raw_frame.append(b)
                        state = 4
                    elif state == 4:  # 长度高字节
                        payload_len = b << 8
                        raw_frame.append(b)
                        state = 5
                    elif state == 5:  # 长度低字节
                        payload_len |= b
                        raw_frame.append(b)
                        rx_buf.clear()
                        if payload_len > 512:
                            state = 0
                        elif payload_len == 0:
                            state = 7
                        else:
                            state = 6
                    elif state == 6:  # 收集 Payload
                        rx_buf.append(b)
                        raw_frame.append(b)
                        if len(rx_buf) >= payload_len:
                            state = 7
                    elif state == 7:  # CRC 低字节
                        raw_frame.append(b)
                        state = 8
                    elif state == 8:  # CRC 高字节
                        raw_frame.append(b)
                        state = 0

                        # CRC 校验
                        calc_crc = calc_crc16_modbus(raw_frame[:-2])
                        rx_crc = raw_frame[-2] | (raw_frame[-1] << 8)

                        if calc_crc == rx_crc:
                            self._handle_frame(cmd_id, seq_num, bytes(rx_buf))
                        else:
                            print(f"[!] CRC 校验失败! 计算:0x{calc_crc:04X}, 收到:0x{rx_crc:04X}")
            except Exception as e:
                print(f"[-] 数据接收异常: {e}")
                break

    def _handle_frame(self, cmd_id: int, seq_num: int, payload: bytes):
        """业务帧处理与性能统计"""
        now = time.time()
        dt_ms = (now - self.last_pkt_time) * 1000.0 if self.last_pkt_time > 0 else 20.0
        self.last_pkt_time = now

        if cmd_id == CMD_TELEMETRY_REPORT:
            self.pkt_count += 1
            if len(payload) >= 50:
                # 解析 50 字节完整遥测报文 (格式: <IBBfffffffffff)
                (ts, mode, alarm, torque, speed, mech_pwr,
                 dac_v, dc_v, dc_i, elec_v, elec_i, elec_pwr, pf, eff) = struct.unpack("<IBBfffffffffff", payload[:50])

                mode_str = MODE_NAMES.get(mode, f"未知({mode})")
                fps = self.pkt_count / max(0.001, (now - self.start_time))

                # 实时输出接收到的数据
                if self.verbose:
                    # 逐包详细输出模式
                    print(f"[RX #{self.pkt_count:05d} | Seq:{seq_num:03d} | Ts:{ts:8d}ms | 周期:{dt_ms:4.1f}ms] "
                          f"模式:{mode_str} | [CS1237扭矩]:{torque:6.2f}N.m | 转速:{speed:6.1f}RPM | 机械功率:{mech_pwr:6.1f}W | "
                          f"DAC:{dac_v:4.2f}V || [CS1238直流] V:{dc_v:6.2f}mV, I:{dc_i:6.2f}mV || [HLW8112交流] {elec_v:5.1f}V, {elec_i:5.2f}A, {elec_pwr:6.1f}W | 效率:{eff:4.1f}%", flush=True)
                else:
                    # 默认每 5 包 (约 100ms) 输出一次
                    if self.pkt_count % 5 == 0 or self.pkt_count <= 3:
                        print(f"[{ts:8d}ms | #{self.pkt_count:05d} | 周期:{dt_ms:4.1f}ms | 帧率:{fps:4.1f}Hz | {mode_str}]\n"
                              f"   -> [机械] CS1237扭矩:{torque:6.2f} N.m | 转速:{speed:6.1f} RPM | 机械功率:{mech_pwr:6.1f} W | DAC:{dac_v:4.2f} V\n"
                              f"   -> [直流] CS1238电压:{dc_v:6.2f} mV | CS1238电流:{dc_i:6.2f} mV | 直流功率:{(dc_v*dc_i/1000.0):6.2f} mW\n"
                              f"   -> [交流] HLW8112电压:{elec_v:5.1f} V | 电流:{elec_i:5.2f} A | 功率:{elec_pwr:6.1f} W | PF:{pf:4.2f} | 效率:{eff:4.1f}%\n", flush=True)
            elif len(payload) >= 42:
                # 兼容 42 字节旧版报文
                (ts, mode, alarm, torque, speed, mech_pwr,
                 dac_v, elec_v, elec_i, elec_pwr, pf, eff) = struct.unpack("<IBBfffffffff", payload[:42])
                mode_str = MODE_NAMES.get(mode, f"未知({mode})")
                print(f"[{ts:8d}ms | #{self.pkt_count:05d}] 扭矩:{torque:6.2f}N.m | DAC:{dac_v:4.2f}V | 交流:{elec_v:5.1f}V, {elec_i:5.2f}A", flush=True)
            else:
                print(f"[!] 收到 0x01 遥测帧但长度不足: {len(payload)} 字节", flush=True)


        elif cmd_id == CMD_ACK_RESPONSE:
            if len(payload) >= 2:
                reply_cmd, status = struct.unpack("<BB", payload[:2])
                status_desc = {0: "成功", 1: "模式非法", 2: "超出范围", 3: "校验错误"}.get(status, f"代码{status}")
                print(f"\n[ACK 应答] 功能码 0x{reply_cmd:02X} 应答状态: {status_desc} (Seq: {seq_num})\n", flush=True)
        else:
            print(f"[RX 其它帧] 功能码: 0x{cmd_id:02X}, 序号: {seq_num}, 长度: {len(payload)} 字节", flush=True)
            print(f"   Payload Hex: {' '.join(f'{b:02X}' for b in payload)}", flush=True)



    def send_cmd(self, cmd_id: int, payload: bytes):
        """打包并下发命令帧给 GD32 节点"""
        if not self.client_sock:
            print("[!] 当前无 GD32 节点连接，无法下发指令！")
            return

        self.seq = (self.seq + 1) & 0xFF
        header = struct.pack(">BBBBH", HEADER_HIGH, HEADER_LOW, cmd_id, self.seq, len(payload))
        frame_no_crc = header + payload
        crc = calc_crc16_modbus(frame_no_crc)
        full_frame = frame_no_crc + struct.pack("<H", crc)

        try:
            self.client_sock.sendall(full_frame)
            print(f"[TX] 指令 0x{cmd_id:02X} 已发送 ({len(full_frame)} 字节, Seq={self.seq})")
        except Exception as e:
            print(f"[-] 指令发送失败: {e}")

    def cmd_set_manual(self, dac_volts: float):
        """下发手动开环 DAC 指令 (Cmd 0x10)"""
        dac_volts = max(0.0, min(10.0, dac_volts))
        payload = struct.pack("<Bf", 1, dac_volts)
        print(f"[*] 设定工作模式: MANUAL 开环, DAC 目标电压 = {dac_volts:.2f} V")
        self.send_cmd(CMD_SET_MODE, payload)

    def cmd_set_const_torque(self, target_torque: float):
        """下发恒扭矩闭环指令 (Cmd 0x10)"""
        payload = struct.pack("<Bf", 2, target_torque)
        print(f"[*] 设定工作模式: CONST_TORQUE 恒扭矩, 目标扭矩 = {target_torque:.2f} N.m")
        self.send_cmd(CMD_SET_MODE, payload)

    def cmd_set_const_power(self, target_power: float):
        """下发恒功率闭环指令 (Cmd 0x10)"""
        payload = struct.pack("<Bf", 3, target_power)
        print(f"[*] 设定工作模式: CONST_POWER 恒功率, 目标功率 = {target_power:.2f} W")
        self.send_cmd(CMD_SET_MODE, payload)

    def cmd_estop(self):
        """下发急停指令 (Cmd 0x11)"""
        print("[!] 触发紧急停机 (E-STOP) 指令！")
        self.send_cmd(CMD_ESTOP, b"")

    def cmd_set_pid(self, loop_type: int, kp: float, ki: float, kd: float, max_dac: float = 10.0):
        """下发 PID 参数配置 (Cmd 0x12)"""
        payload = struct.pack("<Bffff", loop_type, kp, ki, kd, max_dac)
        loop_name = "恒扭矩环" if loop_type == 0 else "恒功率环"
        print(f"[*] 设置 {loop_name} PID: Kp={kp:.4f}, Ki={ki:.4f}, Kd={kd:.4f}, MaxDAC={max_dac:.2f}V")
        self.send_cmd(CMD_SET_PID, payload)


def interactive_cli(server: DynoCenterServer):
    """交互式控制台菜单"""
    time.sleep(0.5)
    print("\n==================================================================")
    print(" 测功机中心服务器控制指令说明:")
    print("   m <volts>       : 开环手动设定 DAC 电压 (0~10V)      [示例: m 2.5]")
    print("   t <torque>      : 恒扭矩闭环控制 (N.m)               [示例: t 15.0]")
    print("   p <power>       : 恒功率闭环控制 (W)                 [示例: p 500.0]")
    print("   pid <0/1> <kp> <ki> <kd> : 配置 PID 参数             [示例: pid 0 0.05 0.01 0.001]")
    print("   v               : 切换全包输出模式 (Verbose 50Hz逐包 vs 默认平滑流)")
    print("   e               : 紧急停机 (Emergency Stop)")
    print("   q               : 退出服务器程序")
    print("==================================================================\n")

    while server.running:
        try:
            line = input().strip()
            if not line:
                continue

            parts = line.split()
            cmd = parts[0].lower()

            if cmd == 'q':
                server.running = False
                print("[*] 正在退出中心服务器...")
                sys.exit(0)
            elif cmd == 'v':
                server.verbose = not server.verbose
                print(f"[*] 全包输出模式切换为: {'开启 (逐包50Hz打印)' if server.verbose else '关闭 (平滑流打印)'}")
            elif cmd == 'm':
                if len(parts) >= 2:
                    server.cmd_set_manual(float(parts[1]))
                else:
                    print("[!] 语法错误: m <voltage_0_to_10V>")
            elif cmd == 't':
                if len(parts) >= 2:
                    server.cmd_set_const_torque(float(parts[1]))
                else:
                    print("[!] 语法错误: t <target_torque_Nm>")
            elif cmd == 'p':
                if len(parts) >= 2:
                    server.cmd_set_const_power(float(parts[1]))
                else:
                    print("[!] 语法错误: p <target_power_W>")
            elif cmd == 'pid':
                if len(parts) >= 5:
                    server.cmd_set_pid(int(parts[1]), float(parts[2]), float(parts[3]), float(parts[4]))
                else:
                    print("[!] 语法错误: pid <0:扭矩/1:功率> <kp> <ki> <kd>")
            elif cmd == 'e':
                server.cmd_estop()
            else:
                print(f"[!] 未知指令 '{cmd}', 请输入 m / t / p / pid / e / q")
        except (ValueError, IndexError) as e:
            print(f"[!] 参数解析错误: {e}")
        except (EOFError, KeyboardInterrupt):
            server.running = False
            break

if __name__ == "__main__":
    port = 8080
    if len(sys.argv) > 1:
        port = int(sys.argv[1])

    server = DynoCenterServer(host="0.0.0.0", port=port)
    server.start()
    interactive_cli(server)
