运行测试

1. 安装 uv：
   curl -LsSf https://astral.sh/uv/install.sh | sh

   powershell
   irm https://astral.sh/uv/install.ps1 | iex

2. 进入文件夹：
   cd 你的项目目录

3. 安装依赖：
   uv sync

4. 运行：
   修改 self.serial_port 为你的 COM 口
   ```python
   class WBU:
   def **init**(self):
      # self.serial_port = "/dev/tty.usbmodem123456781" # console
      self.serial_port = "/dev/cu.wchusbserial141410" # SVU
   ```
   uv run python main.py
