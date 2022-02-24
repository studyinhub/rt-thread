# /bin/python3
# -*- coding: UTF-8 -*-
# V1.0
# 1.使用多线程修复串口收发与mainloop响应用户事件（输入）冲突
# 2.修复启停数据帧与读写数据帧同时占用串口。
# 3.增加frame

from syslog import LOG_DEBUG
import serial
import serial.tools.list_ports
import time
import tkinter as tk    # 测试 from~~~
import threading
import sys

from tkinter import ttk
import datetime
import logging
import logging.handlers

# FORMAT = ('%(asctime)-15s %(threadName)-15s'
#           ' %(levelname)-8s %(module)-15s:%(lineno)-8s %(message)s')

LOG_FORMAT = "%(asctime)s.%(msecs)03d - %(levelname)s - %(message)s"
# DATE_FORMAT = "%m/%d/%Y %H:%M:%S %p"
DATE_FORMAT="%H:%M:%S"
logging.basicConfig(format=LOG_FORMAT,datefmt=DATE_FORMAT)

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

rf_handler = logging.handlers.TimedRotatingFileHandler('./test/all.log', when='midnight', interval=1, backupCount=7, atTime=datetime.time(0, 0, 0, 0))
rf_handler.setFormatter(logging.Formatter("%(asctime)s - %(levelname)s - %(message)s"))

f_i_handler = logging.FileHandler('./test/info.log')
f_i_handler.setLevel(logging.INFO)
f_i_handler.setFormatter(logging.Formatter("%(asctime)s - %(levelname)s - %(message)s"))

f_handler = logging.FileHandler('./test/error.log')
f_handler.setLevel(logging.ERROR)
f_handler.setFormatter(logging.Formatter("%(asctime)s - %(levelname)s - %(filename)s[:%(lineno)d] - %(message)s"))

# logger.addHandler(rf_handler)
logger.addHandler(f_i_handler)
logger.addHandler(f_handler)

log_d=logger.debug
log_i=logger.info
log_w=logger.warning
log_e=logger.error
log_c=logger.critical


# logging.debug("This is a debug log.")
# logging.info("This is a info log.")
# logging.warning("This is a warning log.")
# logging.error("This is a error log.")
# logging.critical("This is a critical log.")

isDev = True
devCom = "/dev/cu.wchusbserial14220"


# 基础界面
window = tk.Tk()
window.geometry('800x400')
window.title('半导体串口测试工具')
#目录
menubar = tk.Menu(window)
filemenu = tk.Menu(menubar, tearoff=0)                                      # 在原窗口
editmenu = tk.Menu(menubar, tearoff=0)
socketmenu=tk.Menu(menubar, tearoff=0)
menubar.add_cascade(label='EHS015.PIO', menu=filemenu)              # [kæˈskeɪd] 小瀑布，子项
menubar.add_cascade(label='HS008H.LAM.STRIKER', menu=editmenu)              # unknown option "-lable"
menubar.add_cascade(label='socket',menu=socketmenu)
window.config(menu=menubar)

class EHS015:
    def __init__(self,window):
        self.ser=0
        self.comlist=[]
        self.line=b''
        self.ready = 0
        self.starts = 0
        self.startxet=tk.StringVar()
        self.startxet.set('停机')    
        self.openornot = tk.StringVar()
        self.openornot.set('串口未开')
        # frame 建立
        self.window=window

        # frm = ttk.Frame(self.window, padding=30)
        # frm.grid()
        # ttk.Label(frm, text="Hello World!").grid(column=0, row=0)
        # ttk.Button(frm, text="Quit", command=self.window.destroy).grid(column=1, row=0)

        self.frame= tk.Frame(self.window).place(rely=.5, relx=0.5, x=-100, y=-100)
        # 数据标签
        self.a=tk.Label(self.frame, text='PV').place(relx=0.0, rely=0.1, relheight=0.05, relwidth=0.1) 
        tk.Label(self.frame, text='流量').place(relx=0.0, rely=0.2, relheight=0.05, relwidth=0.1)
        tk.Label(self.frame, text='压力').place(relx=0.0, rely=0.3, relheight=0.05, relwidth=0.1)
        tk.Label(self.frame, text='电阻率').place(relx=0.0, rely=0.4, relheight=0.05, relwidth=0.1)
        # sv输入
        tk.Label(self.frame, text='SV').place(relx=0.0, rely=0.5, relheight=0.05, relwidth=0.1)
        self.en = tk.Entry()
        self.en.place(relx=0.2, rely=0.5, relheight=0.05, relwidth=0.1)  
        #启停按钮
        tk.Label(self.frame, text='启停').place(relx=0.0, rely=0.6, relheight=0.05, relwidth=0.1)
        tk.Button(self.frame, width=10, text='启停',textvariable=self.startxet, command=self.start).place(relx=0.2, rely=0.6, relheight=0.05, relwidth=0.1)
        # com选择
        self.listb = tk.Listbox()
        self.listb.place(relx=1, rely=0.1, relheight=0.5, relwidth=0.4, anchor='ne') 

        port_list = list(serial.tools.list_ports.comports())
        
        print("plist:",port_list)
        if len(port_list) == 0:
            print('无可用串口')
        else:
            for i in range(0,len(port_list)):
                print(port_list[i])

        self.comlist = []
        for i in port_list:
            self.comlist.append(str(i))
            
        for i in self.comlist:
            self.listb.insert(len(self.comlist)-1,i)

        # 打开串口按钮
        tk.Button(self.frame, width=10, textvariable=self.openornot, command=self.comopen).place(relx=1.0, rely=0.0, relheight=0.05, relwidth=0.2, anchor='ne')  
        self.gothread=threading.Thread(target=self.go)

        if isDev:
            self.gothread.start()

    # 打开/关闭com
    def opencom(self):
        if isDev:
            for item in self.comlist:
                if item.find(devCom) >=0:
                   comvar = item.split('-')[0].rstrip()
        else:
            comvar=self.listb.get(self.listb.curselection()).split('-')[0].rstrip()

        print("comvar:",comvar)

        if self.ser == 0:
            # self.ser = serial.Serial(comvar, 9600, 7, 'E', 1, timeout=2)
            self.ser = serial.Serial(comvar, 9600, 8, 'N', 1, timeout=10)
            print("open success")    
            self.openornot.set('串口已开')   

    # 读写
    def go(self):
        svset= tk.IntVar
        pv=tk.IntVar()
        flow=tk.IntVar()
        pre=tk.IntVar()
        resis=tk.IntVar() 
        self.opencom() 

        while self.ser.is_open:
            ascframe = ':01170000000D0000000000DB\r\n'.encode('ascii') 
            log_i("send:%s"%ascframe)
            self.ser.write(ascframe)   # 向端口些数据 字符串必须译码

            self.line = self.ser.readline()  
            log_i("recv:%s"%self.line)
            # 递归读取串口                                 
            if len(self.line) > 0:
                pv_f=int(self.line[7:11].decode(),16)
                pv.set(pv_f/10)   # 数据流解码成字符串再转int
                flow_f=int(self.line[11:15].decode(),16)
                flow.set(flow_f/10)
                pre_f=int(self.line[15:19].decode(),16)
                pre.set(pre_f/10)
                resis_f=int(self.line[19:23].decode(),16)
                resis.set(resis_f/10)
                tk.Label(self.frame, textvariable=pv).place(relx=0.2, rely=0.1, relheight=0.05, relwidth=0.1)  
                tk.Label(self.frame, textvariable=flow).place(relx=0.2, rely=0.2, relheight=0.05, relwidth=0.1)
                tk.Label(self.frame, textvariable=pre).place(relx=0.2, rely=0.3, relheight=0.05, relwidth=0.1)  
                tk.Label(self.frame, textvariable=resis).place(relx=0.2, rely=0.4, relheight=0.05, relwidth=0.1)
                log_d("update UI")
                # 先降低一下读取频率
                if isDev:
                    time.sleep(20) 
                # 读不到为0
            else:
                tk.Label(self.frame, text='##').place(relx=0.2, rely=0.1, relheight=0.05, relwidth=0.1)  
                tk.Label(self.frame, text='##').place(relx=0.2, rely=0.2, relheight=0.05, relwidth=0.1)
                tk.Label(self.frame, text='##').place(relx=0.2, rely=0.3, relheight=0.05, relwidth=0.1)  
                tk.Label(self.frame, text='##').place(relx=0.2, rely=0.4, relheight=0.05, relwidth=0.1)
                print(2,time.time)
            # sv输入
            if len(self.en.get()) > 0:
                self.svset = str(hex(int(float(self.en.get())*10)))[2:]
                self.svset = self.svset.upper()
                self.svset = (4-len(svset))*'0' + svset        
                self.svset = ':011700000000000B000102'+svset+'E5'+'\r\n'          # 校验后续增加,多个输入格式转换考虑写模块
                self.ser.write(self.svset.encode('ascii'))                             # 向端口些数据 字符串必须译码
                self.ser.readline()
                print(3,time.time(),self.line)            
            # 启停
            if self.starts:
                self.ser.write(':011700000000000C0001020001D8\r\n'.encode(
                        'ascii'))  # 向端口些数据 字符串必须译码
                self.ser.readline()            
            else:
                pass
                # self.ser.write(':011700000000000C0001020000D9\r\n'.encode(
                #         'ascii'))  # 向端口些数据 字符串必须译码
                # self.ser.readline()
        else:
            print('串口未开')
        print(4,time.time())
        # frame.after(6000,go)

    # 启停机组
    
    def start(self):
        if self.starts:
            self.startxet.set('启动')
            self.starts = 0
        else:
            self.startxet.set('停止')
            self.starts = 1

    # 关闭串口    
    def closecom(self):
        self.ser.close()  # 关闭端串口
        self.openornot.set('串口关闭')


    # 打开串口
    def comopen(self):
        if self.ready:
            self.ready = 0
            self.closecom()  # Com.closecom 错误 没有实例化，不能直接调用类方法
        else:
            self.ready = 1
            self.opencom()
            print(self.gothread.is_alive())
            if self.gothread.is_alive() == 0:
                self.gothread.start()

main=EHS015(window) 
window.mainloop()

if main.ser:
    if main.ser.is_open:
        main.closecom()

sys.exit() 
