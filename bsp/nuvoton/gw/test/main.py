#!/usr/bin/env python
import cmd
import serial
import time
import os
from colorama import init, Fore, Back, Style
# 初始化（必须加）
init(autoreset=True)  # autoreset=True → 自动恢复默认颜色，不用手动重置


USE_LRC = True

class WBU:
    def __init__(self):
        # self.serial_port = "/dev/tty.usbmodem123456781" # console
        self.serial_port = "/dev/cu.wchusbserial141410"   # SVU
        self.baudrate = 19200
        self.timeout = 0.02
        self.ser = None

    def open_serial(self):
        if self.ser is None or not self.ser.is_open:
            self.ser = serial.Serial(
                port=self.serial_port,
                baudrate=self.baudrate,
                timeout=self.timeout
            )
        self.ser.reset_input_buffer()
        self.ser.reset_output_buffer()

    # ======================
    # ✅ LRC 校验函数（新增）
    # ======================
    def calculate_lrc(self, data):
        lrc = 0
        for b in data:
            lrc = (lrc + b) & 0xFF
        return (-lrc) & 0xFF

    # ======================
    # ✅ 改进：可选是否加 LRC
    # ======================
    def send(self, send_str, add_lrc=False):  # 👈 新增 add_lrc
        try:
            self.open_serial()
            raw_data = send_str.strip().encode("utf-8")

            # ==================================
            # 只有 add_lrc=True 才追加 LRC
            # ==================================
            if add_lrc:
                raw_data += b" "
                lrc = self.calculate_lrc(raw_data)
                lrc_hex_str = f"{lrc:02X}"  # 例如 0x98 → "98"
                lrc_bytes = lrc_hex_str.encode("utf-8")  # 转成 b"98"

                send_data = raw_data + lrc_bytes + b"\r\n"

                # print(f"🔵 LRC 数值: 0x{lrc:02X}")
                # print(f"🔵 LRC ASCII: {lrc_hex_str}")
            else:
                send_data = raw_data + b"\r\n"

            print(Fore.GREEN + f"✅ 发送字符串: {send_data.strip()}")
            # print(Fore.CYAN + f"🔵 LRC 校验值: {lrc:02X}")
            print(Fore.LIGHTBLACK_EX + f"🔵 HEX:    {send_data.hex(' ')}")
            print("-" * 80)
            self.ser.write(send_data)
            # if "wrp" in send_str:
            #     time.sleep(1)

            return True

        except Exception as e:
            print(Fore.RED + f"❌ 发送错误: {e}")
            return False

    # ======================
    # ✅ 单独接收函数
    # ======================
    def receive(self):
        try:
            response = b""

            while self.ser.in_waiting > 0:
                response += self.ser.read(self.ser.in_waiting)
                time.sleep(0.01)

            if response:
                response_str = response.decode("utf-8", errors="ignore").strip()
                print(Fore.YELLOW + "📥 串口返回：")
                print(Fore.WHITE + f"📄 字符串: {response_str}")
                print(Fore.LIGHTBLACK_EX + f"🔵 接收 HEX:    {response.hex(' ')}")
            else:
                print(Fore.RED + "⚠️ 未收到返回数据")

            print("-" * 80)
            return response

        except Exception as e:
            print(Fore.RED + f"❌ 接收错误: {e}")
            return b""

    def send_and_receive(self, send_str):
        self.send(send_str,USE_LRC)   # 调用发送
        time.sleep(0.3)
        return self.receive() # 调用接收

    def close(self):
        if self.ser and self.ser.is_open:
            self.ser.close()

class MyPrompt(cmd.Cmd):
    prompt = "> "
    intro = "✨ 串口调试终端 (支持 Tab补全、↑↓历史、clear清屏)\n支持命令：ver, sta, exit, clear"

    # 可补全的命令列表
    commands = ["ver", "sta", "exit", "clear"]

    def __init__(self):
        super().__init__()
        self.wbu = WBU()

    # ======================
    # ✅ 修复：直接回车不执行上条命令
    # ======================
    def emptyline(self):
        pass

    # ----------------------
    # Tab 自动补全
    # ----------------------
    def complete_default(self, text, line, begidx, endidx):
        if not text:
            return self.commands
        return [c for c in self.commands if c.startswith(text)]

    # ----------------------
    # 统一处理所有命令
    # ----------------------
    def default(self, line):
        line = line.strip()
        if not line:
            return

        if line == "clear":
            self.do_clear(None)
            return

        if line == "exit":
            self.do_exit(None)
            return

        # 其他命令直接发送串口
        self.wbu.send_and_receive(f"{line} \r\n")

    def _extract_value(self, resp_str):
        try:
            if "=" in resp_str:
                # 先按 = 分割
                part_after_eq = resp_str.split("=")[-1].strip()
                # 再按空格分割，取第一个数字（忽略后面的 LRC）
                value_str = part_after_eq.split()[0]
                return int(value_str)
        except:
            return None

    def do_auto(self, arg):
        # 默认测试1次
        test_times = 1
        try:
            if arg.strip():
                test_times = int(arg)
        except:
            print(Fore.RED + "❌ 输入格式：auto 或 auto 10")
            return

        print(Fore.MAGENTA + f"\n===== 开始自动读写测试（共 {test_times} 次）=====")
        fail_count = 0

        for i in range(1, test_times + 1):
            test_value = i  # 每次写入的值 = 当前次数
            print(Fore.CYAN + f"\n📌 第 {i}/{test_times} 次测试，写入值 = {test_value}")

            # 1. 写入 wrp 100 X
            resp1 = self.wbu.send_and_receive(f"wrp 100 {test_value}")
            val1 = self._extract_value(resp1.decode("utf-8", "ignore"))
            time.sleep(1)
            # 2. 读取 rdp 100
            resp2 = self.wbu.send_and_receive("rdp 100")
            val2 = self._extract_value(resp2.decode("utf-8", "ignore"))

            # 3. 判断
            if val1 == val2 and val1 == test_value:
                print(Fore.GREEN + f"✅ 第 {i} 次测试成功")
            else:
                print(Fore.RED + f"❌ 第 {i} 次测试失败 | 写入值：{test_value} | 读回：{val2}")
                fail_count += 1

        # 最终统计
        print(Fore.MAGENTA + "\n===== 测试完成 =====")
        print(f"总次数：{test_times} | 失败：{fail_count} | 成功：{test_times - fail_count}")
        if fail_count == 0:
            print(Fore.GREEN + "✅ 全部测试通过！")
        else:
            print(Fore.RED + "⚠️ 部分测试失败！")
        print("-" * 80)

    # ----------------------
    # 清屏命令
    # ----------------------
    def do_clear(self, arg):
        """清屏"""
        os.system("clear")

    # ----------------------
    # 退出
    # ----------------------
    def do_exit(self, arg):
        """退出终端"""
        self.wbu.close()
        print("👋 已退出")
        return True

if __name__ == "__main__":
    MyPrompt().cmdloop()