# /bin/python3
# -*- coding: UTF-8 -*-
# V1.0
# 1.使用多线程修复串口收发与mainloop响应用户事件（输入）冲突
# 2.修复启停数据帧与读写数据帧同时占用串口。
# 3.增加frame

from glob import glob
import os
import shutil
import platform
from os.path import abspath
from inspect import getsourcefile
from re import I
# from syslog import LOG_DEBUG
# from tabnanny import check
# from turtle import st
# from wsgiref import validate
# from xmlrpc.client import Boolean
# from xxlimited import Null
import serial
import serial.tools.list_ports
import time
import threading
import sys
import json 

import tkinter as tk    # 测试 from~~~
import tkinter.font as tf
from tkinter import END, Variable, ttk
from tkinter.messagebox import askyesno,showwarning

import datetime
import logging
import logging.handlers
import queue

import textwrap

from pprint import pprint
import requests

from pymodbus.version import version
from pymodbus.server.asynchronous import StartSerialServer,StopServer

from pymodbus.device import ModbusDeviceIdentification
from pymodbus.datastore import ModbusSequentialDataBlock
from pymodbus.datastore import ModbusSlaveContext, ModbusServerContext

from pymodbus.transaction import ModbusRtuFramer

from custom_message import CustomModbusRequest

from twisted.internet.task import LoopingCall
from twisted.internet import reactor

from time import sleep
from threading import Thread

import webbrowser

if platform.system() == 'Darwin':
    import numpy as np
    import matplotlib
    matplotlib.use('TkAgg')
    from matplotlib.backends.backend_tkagg import FigureCanvasTkAgg
    import matplotlib.pyplot as plt
    from matplotlib.figure import Figure
    from matplotlib.widgets import Button
    plt.rcParams['font.sans-serif'] = ['SimHei'] # 步骤一（替换sans-serif字体）
    plt.rcParams['axes.unicode_minus'] = False


# FORMAT = ('%(asctime)-15s %(threadName)-15s'
#           ' %(levelname)-8s %(module)-15s:%(lineno)-8s %(message)s')

LOG_FORMAT = "%(asctime)s.%(msecs)03d - %(levelname)s - %(message)s"
# DATE_FORMAT = "%m/%d/%Y %H:%M:%S %p"
DATE_FORMAT="%H:%M:%S"
logging.basicConfig(format=LOG_FORMAT,datefmt=DATE_FORMAT)

logger = logging.getLogger()
logger.setLevel(logging.DEBUG)

# ----------------------------------------------------------------------- #
# This will send the error messages in the specified namespace to a file.
# The available namespaces in pymodbus are as follows:
# ----------------------------------------------------------------------- #
# * pymodbus.*          - The root namespace
# * pymodbus.server.*   - all logging messages involving the modbus server
# * pymodbus.client.*   - all logging messages involving the client
# * pymodbus.protocol.* - all logging messages inside the protocol layer
# ----------------------------------------------------------------------- #
# 关闭 pymodbus 的 日志输出，只输出错误
logging.getLogger('pymodbus').setLevel(logging.ERROR)

if platform.system() == 'Windows':
    cur_path = abspath(getsourcefile(lambda:0))
    end = cur_path.rindex('\\')
    cur_path = cur_path[:end]+'\\'
else:
    cur_path = os.path.split(os.path.realpath(__file__))[0]

print("cur_path:",cur_path)

rf_handler = logging.handlers.TimedRotatingFileHandler(cur_path+'/all.log', when='midnight', interval=1, backupCount=7, atTime=datetime.time(0, 0, 0, 0))
rf_handler.setFormatter(logging.Formatter("%(asctime)s - %(levelname)s - %(message)s"))

f_i_handler = logging.FileHandler(cur_path+'/info.log')
f_i_handler.setLevel(logging.INFO)
f_i_handler.setFormatter(logging.Formatter("%(asctime)s - %(levelname)s - %(message)s"))

f_handler = logging.FileHandler(cur_path+'/error.log')
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

import zipfile

def unzip(toUnzipFile):
    myzip = zipfile.ZipFile(toUnzipFile)
    myfilelist=myzip.namelist()
    target_dir = "./"
    flag = '/'
    for name in  myfilelist:
        mylist = name.split('/')
        # print(mylist)
        mylist.pop()
        tmp_dir = flag.join(mylist)
        base_dir = "%s%s" % (target_dir,tmp_dir)
        if os.path.isdir(base_dir):
            pass
        else:
            os.makedirs(base_dir)
        
        if not os.path.isdir(target_dir+name):
            f_handle = open(target_dir+name,"wb")
            f_handle.write(myzip.read(name))
    f_handle.close()

def is_number(s):
    try:
        float(s)
        return True
    except ValueError:
        pass
 
    try:
        import unicodedata
        unicodedata.numeric(s)
        return True
    except (TypeError, ValueError):
        pass
 
    return False

cond= threading.Condition() # 条件锁

# 如果要监控 text 内容发生改变，如果通过 bind key 的方式，每次都会滞后一次操作
# 但如果通过下面的方法，每次都会调用两次
# https://code.activestate.com/recipes/464635-call-a-callback-when-a-tkintertext-is-modified/
class ModifiedMixin:
    '''
    Class to allow a Tkinter Text widget to notice when it's modified.

    To use this mixin, subclass from Tkinter.Text and the mixin, then write
    an __init__() method for the new class that calls _init().

    Then override the beenModified() method to implement the behavior that
    you want to happen when the Text is modified.
    '''

    def _init(self):
        '''
        Prepare the Text for modification notification.
        '''

        # Clear the modified flag, as a side effect this also gives the
        # instance a _resetting_modified_flag attribute.
        self.clearModifiedFlag()

        # Bind the <<Modified>> virtual event to the internal callback.
        self.bind_all('<<Modified>>', self._beenModified)

    def _beenModified(self, event=None):
        '''
        Call the user callback. Clear the Tk 'modified' variable of the Text.
        '''

        # If this is being called recursively as a result of the call to
        # clearModifiedFlag() immediately below, then we do nothing.
        if self._resetting_modified_flag: return

        # Clear the Tk 'modified' variable.
        self.clearModifiedFlag()

        # Call the user-defined callback.
        self.beenModified(event)

    def beenModified(self, event=None):
        '''
        Override this method in your class to do what you want when the Text
        is modified.
        '''
        pass

    def clearModifiedFlag(self):
        '''
        Clear the Tk 'modified' variable of the Text.

        Uses the _resetting_modified_flag attribute as a sentinel against
        triggering _beenModified() recursively when setting 'modified' to 0.
        '''

        # Set the sentinel.
        self._resetting_modified_flag = True

        try:

            # Set 'modified' to 0.  This will also trigger the <<Modified>>
            # virtual event which is why we need the sentinel.
            self.tk.call(self._w, 'edit', 'modified', 0)

        finally:
            # Clean the sentinel.
            self._resetting_modified_flag = False


class delay_win(tk.Toplevel):
    def __init__(self,parent,msg,delay=2000):
        super().__init__()
        self.title(u'延时窗口')
        self.parent = parent

        sw = self.parent.winfo_screenheight()
        sh = self.parent.winfo_screenheight()
        w = sw*0.7
        h = sh*0.6

        x = (sw-w)/2
        y = (sh-h)/2

        self.wm_attributes('-topmost',1)
        # self.overrideredirect(True)

        # self.geometry('300x200+{}+{}'.format(x,y))
        self.geometry('%dx%d+%d+%d'%(w,h,x,y))
        # self.overrideredirect(False)

        # self.tk.call("::tk::unsupported::MacWindowStyle", "style", self._w, "plain", "none")
        # self.overrideredirect(1)
        # self.overrideredirect(0) #added for a toggle effect, not fully sure why it's like this on Mac OS
        # self.wm_attributes('-fullscreen',1)
        richText=tk.Text(self,width=380)
        richText.place(x=10,y=10,width=180,height=180)
        richText.insert('0.0',msg)
        self.after(delay,self.destroy)


class myTopLevel(tk.Toplevel):
    def __init__(self,parent):
        super().__init__()
        self.title(u'myTopLevel'+platform.system())
        self.resizable(False,False) 
        self.parent = parent
        self.parent.attributes('-topmost', False)
        self.state('zoomed')
        pw = parent.winfo_screenwidth()
        ph = parent.winfo_screenheight()
        w = pw*0.8
        h = ph*0.9

        x = (pw - w)/2
        y = (ph - h)/2 
        self.geometry('%dx%d+%d+%d'%(w,h,x,y))

        # self.attributes("-toolwindow", 1) # Only Windows
        # self.overrideredirect(True)
        # self.overrideredirect(False)
        # https://wiki.tcl-lang.org/page/wm+attributes
        self.wm_attributes("-alpha", 0.98)
        self.wm_attributes('-topmost',1)
        # self.wm_attributes('-transparent',True) 
        # self.lift()
        # self.withdraw()
        # self.grid()
        # self.transient(self._parent)
        # self.grab_set()
        # self.form.initial_focus()
        # self._parent.center(self)
        # self.deiconify()
        # self._parent._parent.wait_window(self)

        self.protocol("WM_DELETE_WINDOW", self.destroy)
        self.bind("<Escape>", self.close_win)


        # https://stackoverflow.com/questions/18089068/tk-tkinter-detect-application-lost-focus#:~:text=self.focus_get%20%28%29%20will%20return%20the%20object%20that%20has,event%29%3A%20if%20self.focus_get%20%28%29%20%21%3D%20self.menu%3A%20self.destroy_menu%20%28event%29
        # self.bind("<FocusIn>", self.got_focus)
        # self.bind("<FocusOut>", self.lost_focus)
        self.setup_ui()
        self.config(background="lightGray")

    def setup_ui(self):
        pass

    def got_focus(self,event):
        log_d("got focus:%s",event)

    def lost_focus(self,event):
        log_d("lost focus:%s",event)
        # self.destroy()
    def close_win(self,event):
        print("toplev close_win")
        self.destroy()

class ButtonHandler:
    def __init__(self):
        self.flag =True
        self.range_s, self.range_e, self.range_step =0,1,0.005
        #线程函数，用来更新数据并重新绘制图形
    def threadStart(self,l):
        while self.flag:
            sleep(0.02)
            self.range_s += self.range_step
            self.range_e += self.range_step
            t = np.arange(self.range_s, self.range_e, self.range_step)
            ydata = np.sin(4*np.pi*t)
            #更新数据
            l.set_xdata(t-t[0])
            l.set_ydata(ydata)
            #重新绘制图形
            plt.draw()
    def Start(self, event):
        self.flag =True
        #创建并启动新线程
        print("启动")
        t =Thread(target=self.threadStart)
        t.start()
    def Stop(self, event):
        print("停止")
        self.flag =False

# 关于 matplot 有两种
# 一种是嵌入到 tkinter ，一种是不嵌入到 tkinter

class MYPLOT():
    def __init__(self) -> None:
        # 独立出来的一个窗口
        f, ax = plt.subplots()
        
        plt.subplots_adjust(bottom=0.2)

        range_start, range_end, range_step =0,1,0.005
        t = np.arange(range_start, range_end, range_step)
        s = np.sin(4*np.pi*t)

        l,= plt.plot(t, s, lw=2)

        callback =ButtonHandler()

        axprev = plt.axes([0.81,0.05,0.1,0.075])
        bprev =Button(axprev,'Stop')
        bprev.on_clicked(callback.Stop)

        axnext = plt.axes([0.7,0.05,0.1,0.075])
        bnext =Button(axnext,'Start')
        bnext.on_clicked(callback.Start(l))
        plt.show()

    def plot_close():
        plt.close()
    
class Win2(myTopLevel):
    def __init__(self,*args, **kwargs):
        super().__init__(self)
        self.title(u'matplot'+platform.system())
  
    def setup_ui(self):
        # 将 tk figure 嵌入到 tkinter 中，速度明显要慢许多
        # tk_f = Figure(figsize=(2.52, 2.56), dpi=100)#figsize定义图像大小，dpi定义像素
        self.flag = True

        tk_f = plt.figure(figsize=(10,10),facecolor="gray")

        # ax=tk_f.add_axes([0.1,0.1,0.8,0.8],polar=True)
        self.canvs = FigureCanvasTkAgg(tk_f, self) #f是定义的图像，root是tkinter中画布的定义位置
        self.canvs.get_tk_widget().pack(side=tk.TOP, fill=tk.BOTH, expand=1)

        # 同时创建四个子图
        # ax = [tk_f.add_subplot(2, 2, x+1) for x in range(4)]

        # self.f_plot1 = tk_f.add_subplot(211)#定义画布中的位置 row col index

        # x = [1, 2, 3, 4, 5, 6, 7, 8, 9, 10] #关于数据的部分可以提取出来
        # y = [3, 6, 9, 12, 15, 18, 21, 24, 27, 30]
        # self.f_plot1.clear()
        # self.ax1 = self.f_plot1.plot(x, y)[0] # 开始画图，并取得数据元素，更新这个 ax1 ，可以更新画图
        # # 可以多次调用 plot 函数来画多条曲线
        # self.canvs.draw()


        self.f_plot2 = tk_f.add_subplot(111)#定义画布中的位置
        self.f_plot2.set_title('read 32 registers round time')

        self.start()

        self.canvs.draw()

        tb1=tk.Button(self, text=u"确定", command=self.ok)
        tb2=tk.Button(self, text=u"取消", command=self.cancel)
        tb1.place(relx=0.85,rely=0.92)
        tb2.place(relx=0.9,rely=0.92)

    def get_time_list(self):
        fp =open(cur_path+"/info.log")
        recvTime = 0
        invList = []
        for line in fp.readlines():
            if line == "\r\n":
                continue
            timeStr = line[:23]
            # print(timeStr)
            if "send" in line:
                sendTime = timeStr
                continue
            elif "recv" in line:
                recvTime = timeStr
                if recvTime:
                    delta = [recvTime,sendTime]
                    # print(delta)
                    t1 =  datetime.datetime.strptime(delta[0], "%Y-%m-%d %H:%M:%S,%f")
                    t2 =  datetime.datetime.strptime(delta[1], "%Y-%m-%d %H:%M:%S,%f")
                    diff = t1 -t2
                    print(diff.microseconds/1000)
                    invList.append(diff.microseconds/1000)
        fp.close()
        return invList
    
    def threadStart(self):
        print("启动测试线程")
        while self.flag:
            print('thread')
            self.range_start += self.range_step
            self.range_end += self.range_step
            t = np.arange(self.range_start, self.range_end, self.range_step)
            ydata = np.sin(4*np.pi*t)
            #更新数据
            self.ax2.set_xdata(t-t[0])
            self.ax2.set_ydata(ydata)
            self.canvs.draw()
            #重新绘制图形
            sleep(0.01)
            
    def start(self):
        # 静态绘制线
        yData = self.get_time_list()
        print("len:",len(yData))
        # xData = range(0,len(ydata))
        xData = np.arange(0, len(yData), 1)
        print(xData,yData)
        self.f_plot2.plot(xData, yData,lw=2)[0] 
        self.canvs.draw()    
        # 动态绘制钱

        # self.range_start, self.range_end, self.range_step =0,1,0.005
        # xData = np.arange(self.range_start, self.range_end, self.range_step)
        # yData = np.sin(4*np.pi*xData)
        # self.f_plot2.clear()
        # self.ax2 = self.f_plot2.plot(xData, yData,lw=2)[0]
        
        # t =Thread(target=self.threadStart)
        # t.start()
        
    def ok(self):
        print('ok')
        self.close_win()
    
    def cancel(self):
        print('cancel')
        self.close_win()

    def close_win(self,event=None):
        print('close win')
        self.flag = False
        self.destroy()


class My_Text(ModifiedMixin,tk.Text):
        def __init__(self, *a, **b):

            # Create self as a Text.
            tk.Text.__init__(self, *a, **b)

            # Initialize the ModifiedMixin.
            self._init()
            self.isValid = False

        def beenModified(self, event=None):
            '''
            Override this method do do work when the Text is modified.
            '''
            print('self.isValid',self.isValid)

            self.isValid = self.check_json_format()

        
        def check_json_format(self):
            cur_txt = self.get('1.0',END)
            if isinstance(cur_txt, str):    # 首先判断变量是否为字符串
                try:
                    json.loads(cur_txt)
                except ValueError:
                    return False
                return True
            else:
                return False

class WinSerialConfig(myTopLevel):
    def __init__(self,parent,configs):
        super().__init__(parent)
        self.parent = parent
        print("init1")
        self.title(u'串口配置'+platform.system())
        # self.attributes('-topmost', True)
    
    def lost_focus(self,event):
        log_d("lost focus:%s",event)

    def setup_ui(self):
        # self.json_obj = self.parent.fconfig
        self.json_obj = JSON_CONFIG.load()
        self.text = My_Text(self, width=300, height=500,fg="yellow", bg="gray",insertbackground="white")
        self.text.place(relx=0, rely=0, relheight=1, relwidth=0.5)
        self.text.insert('insert',json.dumps(self.json_obj, indent=4,ensure_ascii=False))

        # self.text1 = tk.Text(self, width=300, height=100,fg="yellow", bg="black",insertbackground="white")
        # self.text1.place(relx=0.5, rely=0, relheight=1, relwidth=0.5)
        # self.text1.insert('insert',json.dumps(json_obj, indent=4,ensure_ascii=False))
        
        # global flagWin1
        # global comList
        # global comvar

        # comvar = ''
        # flagWin1=True

        # self.code1 = tk.IntVar()
        # self.code2 = tk.IntVar()

        # self.code1.set(8)
        # self.code2.set(9600)

        # tl1=tk.Label(self, text=u'数据位', width=8, anchor=tk.E)
        # tl2=tk.Label(self, text=u'波特率', width=8, anchor=tk.E)
        # tl1.place(relx=0.1,rely=0.05)
        # tl2.place(relx=0.1,rely=0.15)

        # te1=tk.Entry(self, textvariable=self.code1, width=20)
        # te2=tk.Entry(self, textvariable=self.code2, width=20)
        # te1.place(relx=0.25,rely=0.05)
        # te2.place(relx=0.25,rely=0.15)

        # self.listb = tk.Listbox(self)
        # self.listb.place(relx=0.25,rely=0.25, relheight=0.65, relwidth=0.5) 
        # for i in comlist:
        #     self.listb.insert(len(comlist)-1,i)

      
        tb1=tk.Button(self, text=u"确定", command=self.ok)
        tb2=tk.Button(self, text=u"取消", command=self.cancel)
        tb1.place(relx=0.85,rely=0.92)
        tb2.place(relx=0.9,rely=0.92)
        # te1.focus()




    def ok(self):
        print("ok isValid",self.text.isValid)

        if not self.text.isValid:
            print("self.isValid:",self.text.isValid)
            self.attributes('-topmost', False)
            showwarning("提示","检测到 JSON 格式不对，请检查")
            self.attributes('-topmost', True)
            self.modified = False
        else:
            JSON_CONFIG.store(self.text.get('1.0', tk.END))
            self.modified = True
            self.exit1()

        # print("sle",self.listb.curselection())

        # if isDev:
        #     for item in comlist:
        #         if item.find(devCom) >=0:
        #            comvar = item.split('-')[0].rstrip()
        # else:

        # index = self.listb.curselection()
        # if not index:
        #     if isDev:
        #         index = 7
        #     else:
        #         index = 0
        # else:
        #     index =self.listb.curselection()

        # comvar=self.listb.get(index).split('-')[0].rstrip()

        # print("comvar:",comvar)

        # self.Retcode = [self.code1.get(), self.code2.get(),comvar]
        # self.exit1()

    def exit1(self):
        self.parent.attributes('-topmost', True)
        global flagWin1
        flagWin1=False
        print('exitWin1')
        self.destroy()

    def cancel(self):
        self.Retcode = None
        self.exit1()

class pymodbus_async_server():
    def __init__(self):
       pass
    
    def updating_writer(self,a):
        # log_d("updating the context")
        context = a[0]
        register = 3
        slave_id = 0x01
        address = 0x00
        values = context[slave_id].getValues(register, address, count=5)
        values = [v + 1 for v in values]
        # log_d("new values: " + str(values))
        context[slave_id].setValues(register, address, values)

    def run_async_server(self,serial_config):
        store = ModbusSlaveContext(
        di=ModbusSequentialDataBlock(0, [1]*100),
        co=ModbusSequentialDataBlock(0, [2]*200),
        hr=ModbusSequentialDataBlock(1, [12,34,56,78]+[22]*196),
        ir=ModbusSequentialDataBlock(0, [17]*100))
        store.register(CustomModbusRequest.function_code, 'cm',
                   ModbusSequentialDataBlock(0, [17] * 100))
        context = ModbusServerContext(slaves=store, single=True)

        identity = ModbusDeviceIdentification()
        identity.VendorName = 'Pymodbus'
        identity.ProductCode = 'PM'
        identity.VendorUrl = 'http://github.com/riptideio/pymodbus/'
        identity.ProductName = 'Pymodbus Server'
        identity.ModelName = 'Pymodbus Server'
        identity.MajorMinorRevision = version.short()

        time = 2  # 5 seconds delay
        loop = LoopingCall(f=self.updating_writer, a=(context,))
        loop.start(time, now=False) # initially delay by time

        print("serial_config",serial_config)

        if platform.system() == "Darwin":
            StartSerialServer(context, identity=identity,
                        port=serial_config['Port'], 
                        baudrate=serial_config['BaudRate'],
                        framer=ModbusRtuFramer,timeout=.005, 
                        defer_reactor_run=True,
                        console=False)
            reactor.run()
        else:
            StartSerialServer(context, identity=identity,
                      port=serial_config['Port'], framer=ModbusRtuFramer,timeout=.005, baudrate=serial_config['BaudRate'])


class JSON_CONFIG:

    if platform.system() == 'Darwin':
        configpath = cur_path+"/config.json"
    else:
        configpath = cur_path+"config.json"
    
    print("configpath:",configpath)

    def __init__(self):
        super().__init__()
        print("JSON_CONFIG init")

    @classmethod
    def store(self,data):
        with open(self.configpath, 'w') as json_file:
            if type(data) == 'obj':
                fileData = json.dumps(data, indent=4,ensure_ascii=False)
            else:
                fileData =data
            json_file.write(fileData)
    
    @classmethod
    def load(cls):
        print("self.configpath:",cls.configpath)
        if not os.path.exists(cls.configpath):
            log_w("没有找到配置文件")
            with open(cls.configpath, 'w') as json_file:
                #  Port="COM6", BaudRate="9600", ByteSize="8", Parity="N", Stopbits="1")

                if platform.system() == 'Darwin':
                    asc_232_port = "/dev/cu.wchusbserial14220"
                    asc_485_port = "/dev/cu.wchusbserial142320"
                    rtu_485_port = "/dev/cu.wchusbserial14210"
                    enableRTU = True
                else:
                    asc_232_port = "COM5"
                    asc_485_port = "COM6"
                    rtu_485_port = "COM8"
                    enableRTU = False

                init_config = {
                "enableRTU":enableRTU,
                "serial":{
                    "ASC_232":{
                        "Port":asc_232_port,
                        "BaudRate":9600,
                        "ByteSize":8,
                        "Parity":"N",
                        "Stopbits":1,
                        "Enabled":True
                    },
                    "ASC_485":{
                        "Port":asc_485_port,
                        "BaudRate":9600,
                        "ByteSize":8,
                        "Parity":"N",
                        "Stopbits":1,
                        "Enabled":False
                    },
                    "RTU_485":{
                        "Port":rtu_485_port,
                        "BaudRate":9600,
                        "ByteSize":8,
                        "Parity":"N",
                        "Stopbits":1,
                        "Enabled":True
                    }
                },
                "test_frame":[
                {
                    "name":"读取13",
                    "frame":":01170000000D0000000000DB\r\n",
                    "cycle":False,
                    "cycle_int":0.4
                },
                {
                    "name":"读取15",
                    "frame":":01170000000F0000000000D9\r\n",
                    "cycle":False,
                    "cycle_int":0.8
                },
                {
                    "name":"读取32",
                    "frame":":0117000000200000000000C8\r\n",
                    "cycle":True,
                    "cycle_int":1.0
                },
                {
                    "name":"启动",
                    "frame":":011700000000000C0001020001D8\r\n",
                    "cycle":False,
                    "cycle_int":0.4
                },
                {
                    "name":"状态",
                    "frame":":0117000400040000000000E0\r\n",
                    "cycle":False,
                    "cycle_int":0.4
                },
                {   
                    "name":"停止",
                    "frame":":011700000000000C0001020000D9\r\n",
                    "cycle":False,
                    "cycle_int":0.4
                },
                {   
                    "name":"设温度",
                    "frame":":011700000000000B00010200F0EA\r\n",
                    "cycle":False,
                    "cycle_int":0.4
                }
            ],
            # self.SLAVE_ADDR= 1
            # self.FUN_CODE = 23
            # self.RD_REG_START=4
            # self.RD_REG_CNT=3
            # self.WR_REG_START=0x0B
            # self.WR_REG_CNT=2
            # self.WR_BYTES_CNT=4
            # self.WR_DATA='0x9B 0x01'
            "my_entry_list":[
            {
                "label":"从机地址",
                "value":1
            },
            {
                "label":"功能码",
                "value":23
            },
            {
                "label":"读起始地址",
                "value":0
            },
            {
                "label":"读寄存器数",
                "value":13
            },
            {
                "label":"写起始地址",
                "value":0
            },
            {
                "label":"写寄存器数",
                "value":0
            },
            {
                "label":"写字节数",
                "value":0
            },
            {
                "label":"数据",
                "value":  ''
            }
        ]
            } 
                try:
                    json_file.write(json.dumps(init_config, indent=4,ensure_ascii=False))     
                except:
                    print("写入配置文件失败")
        else:
            print("已经存在配置文件")
        with open(cls.configpath) as json_file:
            try:
                data = json.load(json_file)
            except:
                showwarning("Warning","读取配置文件失败，请删除 config.json 然后重试")
                sys.exit()
                data = {}
            return data
        
    @staticmethod
    def set(data_dict):
        json_obj = JSON_CONFIG.load()
        for key in data_dict:
            json_obj[key] = data_dict[key]
        JSON_CONFIG.store(json_obj)
        print(json.dumps(json_obj, indent=4))
    
class MySerial:
    @staticmethod
    def connect (serial_config):
        # log_d("serial_config %s",serial_config)
        # print(dict(serial_config))
        try:
            myserial = serial.Serial(
                                    serial_config['Port'],
                                    serial_config['BaudRate'],
                                    serial_config['ByteSize'],
                                    serial_config['Parity'],
                                    serial_config['Stopbits']
                                    ,timeout=0.3) 
            
            print("myserial:",myserial.is_open)
            while not myserial.is_open:
                log_d("等待打开...")
                time.sleep(1)
                log_d("%s 打开成功",myserial.port)
            return myserial
        except:
            log_e("连接串口失败")
            return False

        # if self.ser:
        #     if self.ser.is_open:
        #         self.closecom()
        # try:
        #     # self.ser = serial.Serial(comvar, 9600, 7, 'E', 1, timeout=2)
        #     self.ser = serial.Serial(comvar, 9600, 8, 'N', 1, timeout=0.3)
        #     # self.openornot.set(comvar)
        # except(IOError):
        #     log_d("self.ser:%d",self.ser)
        #     # showwarning('Warnning','串口打开失败',parent=self) 
        #     # self.setup_serial_config()
        # # self.insert_text("串口%s打开成功\r\n"%comvar,'green') 
    
    def disconnect(serial):
        if serial and serial.is_open:
            try:
                serial.flush()
                serial.close()
                if not serial.closed:
                    log_d("%s 关闭成功",serial.port)
            except:
                log_d("%s 关闭失败",serial.port)
        pass

# 直接继承 tk.Tk 实例
class EHS015(tk.Tk):
    def __init__(self):
        # 初始化父类，然后本类就具备了父类的属性和方法
        super().__init__()
        global flagWin1,flagWin2
        global comlist
        global serial_config
        global comvar

        global g_myserials
        g_myserials = []

        global isSerialConfigured 

        comvar = ''

        flagWin1=flagWin2=False

        self.ser=0
        self.comlist=[]
        self.line=b''
        self.ready = 0
        self.starts = 0
        self.lock = threading.Lock()
        self.readIntval=0.4 # 每 2s 读一次
        self.is_running = True
        self.is_stopped = False
        self.is_sending = False
        self.last_send = time.time()
        self.single_shot_time = time.time()
        self.send_queue = queue.Queue()
        # 解决 mac 平台最小化后，无法恢复
        if platform.system() == 'Darwin':
            self.createcommand('tk::mac::ReopenApplication', self.deiconify)

        self.bind("<Escape>", self.close_win)
        # self.bind('Meta_L',lambda event:print("command is pressed"))
        # self.bind("<F3>", self.fullscreen_toggle)
        # self.bind("<Escape>", self.fullscreen_cancel)
        # self.bind('Alt_L',lambda event:print("Alt_L is pressed"))
        # self.bind('Control_L',lambda event:print("Control_L is pressed"))
        # self.bind('super_L',lambda event:print("a is pressed"))
        # self.bind('Shift_L',lambda event:print("a is pressed"))
        # self.bind('Caps_Lock',lambda event:print("a is pressed"))
        # self.bind('<Meta_L><w>', lambda event:self.close_win())


        # prompt = '      Press any key      '
        # label1 = tk.Label(self.tkFrame, text=prompt, width=len(prompt), bg='yellow')
        # label1.place(relx=0.01)
        # def key(event):
        #     if event.char == event.keysym:
        #         msg = 'Normal Key %r' % event.char
        #     elif len(event.char) == 1:
        #         msg = 'Punctuation Key %r (%r)' % (event.keysym, event.char)
        #     else:
        #         msg = 'Special Key %r' % event.keysym
        #     label1.config(text=msg)
        # self.bind_all('<Key>', key)

        # 测试帧字典 列表：一个包含多个字典的 list
        self.fconfig = JSON_CONFIG.load()

        # print(config_data)


        # 无边框
        # self.overrideredirect(False)
        # self.overrideredirect(True)


        # 基础界面
        sw = self.winfo_screenwidth()
        sh = self.winfo_screenheight()

        w = sw*1
        h = sh*.9

        x = (sw-w)/2
        y = (sh-h)/2
        self.wm_attributes('-topmost',True)
        self.geometry("%dx%d+%d+%d" % (w, h, x, y))
        self.state('zoomed')

        mygreen = "#d2ffd2"
        # self.configure(bg="#292929")
        # self.configure(bg=mygreen)
        # self.title(os.path.basename(os.path.splitext(__file__)[0])+" "+platform.system())
        self.title(" "+platform.system())
        # self.iconify() # 最小化

        # self.wm_attributes("-transparent", True)
        self.protocol('WM_DELETE_WINDOW', self.close_win)

        #目录
        # menubar = tk.Menu(self)
        # filemenu = tk.Menu(menubar, tearoff=0)    
        # filemenu.add_command(label='串口设置', command=self.setup_serial_config)  
        # menubar.add_cascade(label='EHS015.PIO', menu=filemenu)              # [kæˈskeɪd] 小瀑布，子项


        # editmenu = tk.Menu(menubar, tearoff=0)
        # socketmenu=tk.Menu(menubar, tearoff=0)
        # menubar.add_cascade(label='HS008H.LAM.STRIKER', menu=editmenu)              # unknown option "-lable"
        # menubar.add_cascade(label='socket',menu=socketmenu)
        # self.config(menu=menubar)

        self.startxet=tk.StringVar()
        self.startxet.set('开机')   
        self.btn2Text=tk.StringVar()
        self.btn2Text.set('开始循环')
        self.openornot = tk.StringVar()
        self.openornot.set('串口未打开')

        self.create_styles()
        self.create_frames()
        self.create_widgets()

        self.insert_text("载入配置文件\r\n")
        self.insert_text(json.dumps(self.fconfig, indent=4,ensure_ascii=False)+"\r\n")

        while True:
            port_list = list(serial.tools.list_ports.comports())
            if len(port_list):
                pass
            if len(port_list) == 0:
                # showwarning('Warnning','未找到任何串口',parent=self)
                self.insert_text("未检测到任何串口接入\r\n","tag_red") 
                ret = tk.messagebox.askretrycancel('提示','未检测到任何串口')
                if not ret:
                    # threading.Timer(1, self.setup_serial_config()).start()
                    self.insert_text("注意！没有接入串口将无法通信\r","tag_red")
                    break;
            else:
                # for i in range(0,len(port_list)):
                #     print(port_list[i])
                
                comlist = []
                for item in port_list:
                    portname = str(item).split('-')[0].rstrip()
                    # self.insert_text('找到'+portname+"\r")
                    comlist.append(portname)

                self.comlist = comlist
                pprint(comlist)


                #     for item in comlist:
                #         if item.find(devCom) >=0:
                #            comvar = item.split('-')[0].rstrip()

                self.insert_text("检查配置中的串口是否存在\r\n")
                valid = 0
                for i in self.fconfig['serial']:
                    port = self.fconfig['serial'][i]['Port']
                    myserial = False
                    if port in comlist:
                        print("matched",port,i)
                        self.insert_text("Machted " +i +' ' + port + "\r\n")
                        if not i == 'RTU_485':
                            myserial = MySerial.connect(self.fconfig['serial'][i])
                            if myserial:
                                self.tkSerials[valid][1].set('Connected')
                                valid+=1
                    else:
                        print("Not Machted",port,i)
                        self.insert_text("Not Machted " +i +' ' + port + "\r")

                    # 整合这 3 个串口的全局变量，便于后续程序修改配置或者 GUI 操作能够生效
                    # 比如 GUI 关闭 ASC_485 接口
                    g_myserials.append({
                        "enabled":self.fconfig['serial'][i]['Enabled'],
                        "port":self.fconfig['serial'][i],
                        "if":i.split('_')[0],
                        "type":i.split('_')[1],
                        "hw":myserial
                    })

                print(valid)
                break;

        self.create_threads()


        # self.centerWindow()
        # delay_win(self,"hello",2000)
        # 调用后主窗口失去了置顶功能
        # self.wm_transient(delay_win(self,"hello",2000))

    def create_styles(self):

        frame_style = ttk.Style()
        frame_style.theme_use('default')
        frame_style.configure("TFrame",
                    background='red',
                    foreground='gold',
                    borderwidth='1',
                    relief='raised')

        title_style = ttk.Style()
        # title_style.configure("TLabel", font='serif 20',background="lightGray",foreground="yellow",relheight=1.0)

        # 创建主题
        mygreen = "#d2ffd2"
        myred = "#dd0202"
        style = ttk.Style()
        style.theme_create( "yummy", parent="alt", settings={
        "TNotebook": {"configure": {"tabmargins": [2, 5, 2, 0] } },
        "TNotebook.Tab": {
            "configure": {"padding": [5, 1], "background": mygreen },
            "map":       {"background": [("selected", myred)],
                          "expand": [("selected", [1, 1, 1, 0])] } } } )
        # style.theme_use("yummy")


    def create_frames(self):
        
        # 底部 frame
         frame = tk.Frame(self,bg='gray').place(relheight=1,relwidth=1, relx=0,rely=0,x=0,y=0)
         self.tkFrame= frame

         self.lt_f_relw = 0.15
         self.ct_f_relw = 0.45
         self.rt_f_relw = 0.4

        #  3 3 4 布局
        # 左边 frame
         self.ltFrame= tk.Frame(self,bg='lightGray').place(relheight=1,relwidth=self.lt_f_relw, relx=0,rely=0,x=0,y=0)
        
        # 中间的 frame
         self.ctFrame= tk.Frame(self,bg='gray').place(relheight=1,relwidth=self.ct_f_relw, relx=0.15,rely=0,x=0,y=0)

        # 右边的 frame
         self.rtFrame= tk.Frame(self,bg='gray').place(relheight=1,relwidth=self.rt_f_relw, relx=0.6,rely=0,x=0,y=0) 

        #  frame = ttk.Frame(self.tkFrame,width=100,height=20,padding=(0,0,0,0)) # 左上右下的距离
        #  frame.grid(row=0, column=0,sticky=('N','E','W','S'))
        #  frame.focus()
        #  self.ttkFrame = frame

        #  self.rowconfigure(2,weight=1)
        #  self.columnconfigure(0,weight=1)
        #  ttk.Label(frame,style="title_style.TLabel",text="").pack(fill='x')

    def create_widgets(self):
         # 数据标签


        self.tkIntval = tk.IntVar()
        self.send_queue_size = tk.IntVar()

        
        lable_relwidth= 0.042
        lable_relheight = 0.03

        left_col1_relx = 0
        left_col2_relx = left_col1_relx + lable_relwidth +0.001
        left_col3_relx = left_col2_relx + +lable_relwidth +0.001
        left_col4_relx = left_col3_relx + +lable_relwidth +0.00

        label_rely = 0.05


        # 左边
        tk.Label(self.ltFrame,bg="lightGray",fg="black", text='PV').place(relx=left_col1_relx, rely=label_rely, relheight=lable_relheight, relwidth=lable_relwidth) 
        tk.Label(self.ltFrame,bg="lightGray",fg="black", text='流量').place(relx=left_col1_relx, rely=2*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)
        tk.Label(self.ltFrame,bg="lightGray",fg="black", text='压力').place(relx=left_col1_relx, rely=3*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)
        tk.Label(self.ltFrame,bg="lightGray",fg="black", text='电阻率').place(relx=left_col1_relx, rely=4*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)
        tk.Label(self.ltFrame,bg="lightGray",fg="black", text='SV').place(relx=left_col1_relx, rely=5*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)
        tk.Label(self.ltFrame,bg="lightGray",fg="black", text='启停').place(relx=left_col1_relx, rely=6*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)

        self.svset= tk.IntVar
        self.pv=tk.IntVar()
        self.flow=tk.IntVar()
        self.pre=tk.IntVar()
        self.resis=tk.IntVar() 

        self.pv.set("##")
        self.flow.set("##")
        self.pre.set("##")
        self.resis.set("##")
        
        tk.Label(self.ltFrame, bg="black",fg="white",text='##',textvariable=self.pv).place(relx=left_col2_relx, rely=label_rely, relheight=lable_relheight, relwidth=lable_relwidth)  
        tk.Label(self.ltFrame, bg="black",fg="white",text='##',textvariable=self.flow).place(relx=left_col2_relx, rely=2*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)
        tk.Label(self.ltFrame, bg="black",fg="white",text='##',textvariable=self.pre).place(relx=left_col2_relx, rely=3*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)  
        tk.Label(self.ltFrame, bg="black",fg="white",text='##',textvariable=self.resis).place(relx=left_col2_relx, rely=4*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)
        self.en = tk.Entry()
        self.en.place(relx=left_col2_relx, rely=5*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)  
        tk.Button(self.ltFrame, width=10, text='启动',textvariable=self.startxet, command=self.start).place(relx=left_col2_relx, rely=6*label_rely, relheight=lable_relheight, relwidth=lable_relwidth)


        self.tkSerials = []
        idx =0
        for i in self.fconfig['serial']:
            idx+=1
            port = i
            enable = self.fconfig['serial'][i]['Enabled']

            if platform.system()=="Darwin":
                portname = self.fconfig['serial'][i]['Port'][-4:]
            else:
                portname = self.fconfig['serial'][i]['Port']
            
            tkSerial_port = tk.StringVar()
            tkSerail_status = tk.StringVar()
            tkSerial_portname = tk.StringVar()
            tkSerial_enable = tk.IntVar()

            tkSerial_port.set(port)
            tkSerail_status.set('Discon')
            tkSerial_portname.set(portname)

            # tkSerial_enable.set('enabled' if enable else 'disabled')
            tkSerial_enable.set(enable)
            tkSerial = (tkSerial_port,tkSerail_status,tkSerial_portname,tkSerial_enable)
            self.tkSerials.append(tkSerial)
            tk.Label(self.ltFrame, bg="lightGray",fg="black",text='##',textvariable=tkSerial[0]).place(relx=left_col1_relx, rely=(15+idx)*label_rely, relheight=lable_relheight, relwidth=lable_relwidth, anchor='sw')
            tk.Label(self.ltFrame, bg="black",fg="white",text='##',textvariable=tkSerial[1]).place(relx=left_col2_relx, rely=(15+idx)*label_rely, relheight=lable_relheight, relwidth=lable_relwidth, anchor='sw')
            tk.Label(self.ltFrame, bg="black",fg="white",text='##',textvariable=tkSerial[2]).place(relx=left_col3_relx, rely=(15+idx)*label_rely, relheight=lable_relheight, relwidth=lable_relwidth, anchor='sw')
            tk.Checkbutton(self.tkFrame,variable=tkSerial[3]).place(relx=left_col4_relx,rely=(15+idx)*label_rely,relheight=lable_relheight, anchor='sw')

        # 打开串口按钮 
        tk.Button(self.ltFrame, text="速度显示", command=self.matplot_test).place(relx=.002, rely=0.96,  relwidth=0.055, anchor='sw')  
        tk.Button(self.ltFrame, width=10, text="参数设置", command=self.setup_serial_config).place(relx=.002, rely=1,  relwidth=0.15, anchor='sw')  

        center_relx = self.lt_f_relw + 0.01
        center_rely = 0.045
        self.status = [0 for i in range(10)]

        tk.Label(self.tkFrame,bg="black",fg="white", text="条目").place(relx=center_relx, rely=(1)*center_rely, relheight=lable_relheight, relwidth=0.06)
        tk.Label(self.tkFrame,bg="black",fg="white", text="发送帧").place(relx=center_relx+0.06, rely=(1)*center_rely, relheight=lable_relheight, relwidth=0.26)
        tk.Label(self.tkFrame,bg="black",fg="white", text="循环间隔").place(relx=center_relx+0.27, rely=(1)*center_rely, relheight=lable_relheight, relwidth=0.06)
        tk.Label(self.tkFrame,bg="black",fg="white", text="C").place(relx=center_relx+0.33, rely=(1)*center_rely, relheight=lable_relheight, relwidth=0.06)
        tk.Label(self.tkFrame,bg="black",fg="white", text="命令").place(relx=center_relx+0.36, rely=(1)*center_rely, relheight=lable_relheight, relwidth=0.06)

        base_y = 2

        for i in range(len(self.fconfig['test_frame'])):
            tk.Label(self.tkFrame,bg="lightGray",fg="black", text=self.fconfig['test_frame'][i]["name"]).place(relx=center_relx, rely=(base_y+i)*center_rely, relheight=lable_relheight, relwidth=0.06) 
            default = tk.StringVar()
            default.set(self.fconfig['test_frame'][i]["frame"].replace("\r\n",""))
            tk.Label(self.tkFrame, bg="black",fg="white",text='##',textvariable=default,anchor="nw").place(relx=center_relx+0.06, rely=(base_y+i)*center_rely, relheight=lable_relheight, relwidth=0.26)

            tk.Label(self.tkFrame,bg="lightGray",fg="black", text=self.fconfig['test_frame'][i]["cycle_int"]).place(relx=center_relx+0.27, rely=(base_y+i)*center_rely, relheight=lable_relheight, relwidth=0.06) 

            tk.Button(self.tkFrame, text='Send', command=lambda ii=i:self.sendBtn(ii)).place(relx=center_relx+0.36, rely=(base_y+i)*center_rely, relheight=lable_relheight)
            self.status[i]=tk.BooleanVar()
            self.status[i].set(self.fconfig['test_frame'][i]["cycle"])
            tk.Checkbutton(self.tkFrame,variable=self.status[i],command=lambda ii=i:self.checkBtn(ii)).place(relx=center_relx+0.33,rely=(base_y+i)*center_rely, relheight=lable_relheight)
        
        tk.Button(self.tkFrame, width=10, text='开始循环',textvariable=self.btn2Text, command=self.test).place(relx=center_relx+0.36, rely=(base_y+7)*center_rely, relheight=lable_relheight, relwidth=lable_relwidth)

        
        self.SLAVE_ADDR= self.fconfig["my_entry_list"][0]['value']
        self.FUN_CODE = self.fconfig["my_entry_list"][1]['value']
        self.RD_REG_START=self.fconfig["my_entry_list"][2]['value']
        self.RD_REG_CNT=self.fconfig["my_entry_list"][3]['value']
        self.WR_REG_START=self.fconfig["my_entry_list"][4]['value']
        self.WR_REG_CNT=self.fconfig["my_entry_list"][5]['value']
        self.WR_BYTES_CNT=self.fconfig["my_entry_list"][6]['value']
        self.WR_DATA=self.fconfig["my_entry_list"][7]['value']
        
        frame=":" + \
                "{:02X}".format(self.fconfig["my_entry_list"][0]['value'])+ \
                "{:02X}".format(self.fconfig["my_entry_list"][1]['value'])  + \
                "{:04X}".format(self.fconfig["my_entry_list"][2]['value'])+ \
                "{:04X}".format(self.fconfig["my_entry_list"][3]['value'])+ \
                "{:04X}".format(self.fconfig["my_entry_list"][4]['value'])+ \
                "{:04X}".format(self.fconfig["my_entry_list"][5]['value'])+ \
                "{:02X}".format(self.fconfig["my_entry_list"][6]['value'])

        if(self.fconfig["my_entry_list"][6]['value'] != 0):
            for i in self.fconfig["my_entry_list"][7]['value'].split(' '):
                frame += "{:04X}".format(int(i,16))
        
        lrc = self.calc_lrc(frame)
        frame = frame + "{:02X}\r\n".format(lrc)

        self.test_frame_user = {
            "name":"自定义",
            "frame":frame
        }

        WR_DATA = []
        if(self.fconfig["my_entry_list"][6]["value"] != 0): 
            for i in self.fconfig["my_entry_list"][7]["value"].split(" "):
                WR_DATA.append(int(i,16))
            self.fconfig["my_entry_list"][7]["value"]=' '.join(str(i) for i in WR_DATA)

        self.entry =  [i for i in range(len(self.fconfig["my_entry_list"]))]
        self.eVar =  [tk.StringVar() for i in range(len(self.fconfig["my_entry_list"]))]

        for i in range(len(self.fconfig["my_entry_list"])):
            self.eVar[i].set(self.fconfig["my_entry_list"][i]["value"])

        lineIdx = 8
        for i in range(len(self.fconfig["my_entry_list"])):
           tk.Label(self.ctFrame,bg="lightGray",fg="black", text=self.fconfig["my_entry_list"][i]["label"]).place(relx=center_relx, rely=(base_y+i+lineIdx)*center_rely, relheight=lable_relheight, relwidth=0.06)  
           self.entry[i] = tk.Entry(self.tkFrame,textvariable=self.eVar[i],validate='focusout',validatecommand=self.validate_test(i),invalidcommand=self.invalidate_test(i))
           self.entry[i].place(relx=center_relx+0.06, rely=(base_y+lineIdx+i)*center_rely, relheight=lable_relheight, relwidth=0.29)

        tk.Button(self.tkFrame, text='SET', command=lambda :self.setUser()).place(relx=center_relx+0.36, rely=(base_y+15)*center_rely, relheight=lable_relheight)
        # 自定义发送
        # okayCommand = self.register(self.validate_test)
        lineIdx = 16
        tk.Label(self.tkFrame,bg="lightGray",fg="black", text=self.test_frame_user["name"]).place(relx=center_relx, rely=(base_y+lineIdx)*center_rely, relheight=lable_relheight, relwidth=0.06) 
        self.send_user_var = tk.StringVar()
        self.send_user_var.set(self.test_frame_user["frame"].replace("\r\n",""))
        tk.Entry(self.tkFrame,textvariable=self.send_user_var,validate='focusout',validatecommand=self.validate_test(0),invalidcommand=self.invalidate_test(0)).place(relx=center_relx+0.06, rely=(base_y+lineIdx)*center_rely, relheight=lable_relheight, relwidth=0.29)

        tk.Button(self.tkFrame, text='Send', command=lambda :self.sendUser()).place(relx=center_relx+0.36, rely=(base_y+lineIdx)*center_rely, relheight=lable_relheight)

        # 右边
        text = tk.Text(self.rtFrame, width=300, height=500,fg="yellow", bg="black",state=tk.DISABLED)
        text.place(relx=0.6, rely=0, relheight=1, relwidth=0.4)


        ft = tf.Font(family='微软雅黑',size=10) ###有很多参数
        text.tag_add('tag_red','end') #申明一个tag,在a位置使用
        text.tag_config('tag_red',foreground='red',font = ft ) #设置tag即插入文字的大小,颜色等

        text.tag_add('tag_pink','end')
        text.tag_config('tag_pink',foreground = 'pink',background='black',font = ft)

        text.tag_add('tag_yellow','end')
        text.tag_config('tag_yellow',foreground = 'yellow',background='black',font = ft)

        text.tag_add('tag_blue','end')
        text.tag_config('tag_blue',foreground = 'blue',background='black',font = ft)

        text.tag_add('tag_white','end')
        text.tag_config('tag_white',foreground = 'white',background='black',font = ft)

        text.tag_add('tag_green','end')
        text.tag_config('tag_green',foreground = 'LimeGreen',background='black',font = ft)

        self.text_log = text

        # mask = tk.Frame(self.tkFrame,bg="red")
        # mask.place(relx=1.0, rely=.95, relheight=0.9, relwidth=0.78,anchor='se')
        
        self.text_log.config(state='normal')
        # self.text_log.insert("insert",datetime.datetime.now().strftime('%Y-%m-%d %H:%M:%S')+ "\r\n")
        self.text_log.config(state='disabled')
        # self.text_log.pack(expand=1, fill=tk.BOTH)

        scrollbar = tk.Scrollbar(text, command=self.text_log.yview)
        scrollbar.pack(side="right", fill=tk.Y)
        self.text_log['yscrollcommand'] = scrollbar.set

        # 右上角
        tk.Button(self.rtFrame,  text='清空', bg="blue",fg="red", command=self.text_clear).place(relx=0.95, rely=0, relheight=lable_relheight, relwidth=0.05)

        # 右下角 
        tk.Label(self.rtFrame, width=10, bg="gray",fg="white", text="tx_q:").place(relx=center_relx-0.01, rely=1, relheight=.04, relwidth=0.05, anchor='sw')  
        tk.Label(self.rtFrame, width=10, bg="gray",fg="white", textvariable=self.send_queue_size).place(relx=center_relx+.04, rely=1, relheight=.04, relwidth=0.05, anchor='sw')  
        tk.Label(self.ctFrame, width=10, bg="gray",fg="white", text="间隔:").place(relx=center_relx+.11, rely=1, relheight=.04, relwidth=0.05, anchor='sw')
        tk.Label(self.ctFrame, width=10, bg="gray",fg="white", textvariable=self.tkIntval).place(relx=center_relx+.16, rely=1, relheight=.04, relwidth=0.1, anchor='sw')
        
        tk.Button(self.tkFrame, text='重启', command=lambda :self.cmd_reset()).place(relx=center_relx+0.36, rely=(base_y+17)*center_rely, relheight=lable_relheight)
        tk.Button(self.tkFrame, text='打开 Web', command=lambda :self.cmd_openweb()).place(relx=center_relx+0.36, rely=(base_y+18)*center_rely, relheight=lable_relheight)
        tk.Button(self.tkFrame, text='更新程序', command=lambda :self.cmd_down()).place(relx=center_relx+0.36, rely=(base_y+19)*center_rely, relheight=lable_relheight)


        self.bar = ttk.Progressbar(self.tkFrame,length='100', cursor='spider',
                    mode="determinate",
                    orient=tk.HORIZONTAL)


        self.bar.place(relx=center_relx, rely=(base_y+19)*center_rely,relwidth=0.36, relheight=lable_relheight)

    def cmd_openweb(self):
        webbrowser.open('http://192.168.1.100', new=0, autoraise=True)


    def cmd_down(self):
        self.insert_text('准备下载程序\r','tag_green')
        imagesDir='./images'
        nuWriterDir = './NuWriter'
        # 检测下载工具是否存在

        if not os.path.isdir(nuWriterDir):
            # mc policy set download aliyun/tools
            url = 'http://47.94.84.166:9000/tools/NuWriter.zip'
            myfile = requests.get(url)
            open('./NuWriter.zip', 'wb').write(myfile.content)
            unzip('./NuWriter.zip')

        #  检测本地是否存在程序
        if os.path.isdir(imagesDir):
            # os.rmdir(imagesDir)
            shutil.rmtree(imagesDir)

        # 开始从服务器下载镜像

        self.insert_text('从服务器下载最新的程序\r','tag_green')

        url = 'http://47.94.84.166:9000/images/ProtoTrans.zip'
        myfile = requests.get(url)
        open('./ProtoTrans.zip', 'wb').write(myfile.content)

        self.insert_text('解压程序\r','tag_green') 

        unzip('./ProtoTrans.zip')

        # 启动程序下载
        # os.system("./NuWriter/bin/nuwriter ./NuWriter/bin/ram.ini")
        self.cmd_reset()
        if platform.system() == 'Darwin':
            cmdStr= "./NuWriter/bin/nuwriter ./NuWriter/bin/ram.ini"
        else:
            cmdStr= cur_path+"NuWriter\\bin\\nuwriter.exe " + cur_path+"NuWriter\\bin\\ram.ini"
        print('cmdStr',cmdStr)
        result = os.popen(cmdStr)  
        res = result.read()  
        for line in res.splitlines():  
            # print(line)
            # self.insert_text(line+'\r','tag_green')
            pctStr=line.split('%')[0]
            if is_number(pctStr):
                self.bar["value"] = int(pctStr)
            self.update()
            time.sleep(0.01)
        
        self.insert_text('下载成功\r','tag_green')
 
        # for i in range(100):
        # 1
        #  self.update()
        #  time.sleep(0.1)


    def cmd_reset(self):
        try:
            r=requests.get('http://192.168.1.100/cgi-bin/reset',timeout=1)
            pprint(r.json())
        except requests.exceptions.RequestException as e:
            print(e)

    def setUser(self):
        self.SLAVE_ADDR= int(self.eVar[0].get())
        self.FUN_CODE = int(self.eVar[1].get())
        self.RD_REG_START= int(self.eVar[2].get())
        self.RD_REG_CNT=  int(self.eVar[3].get())
        self.WR_REG_START=  int(self.eVar[4].get())
        self.WR_REG_CNT=  int(self.eVar[5].get())
        self.WR_BYTES_CNT=  int(self.eVar[6].get())
        temp = self.eVar[7].get()

        log_d("type temp:%s",type(temp))
        templist=list(temp.strip().split(' '))
        for i in range(len(templist)):
            log_d("templist:%s",templist[i])

        # print("self.SLAVE_ADDR:%d"%self.SLAVE_ADDR)

        # :01 17 0000 000D 0000 0000 00 0000DB
        # :01 17 0000 000D 0000 0000 00 DB
        # :01 17 0000 000D 0000 0000 00 DB

        frame=":" + \
        "{:02X}".format(self.SLAVE_ADDR)+ \
        "{:02X}".format(self.FUN_CODE)  + \
        "{:04X}".format(self.RD_REG_START)+ \
        "{:04X}".format(self.RD_REG_CNT)+ \
        "{:04X}".format(self.WR_REG_START)+ \
        "{:04X}".format(self.WR_REG_CNT)+ \
        "{:02X}".format(self.WR_BYTES_CNT)

        if(int(self.WR_BYTES_CNT != 0 )):
            for i in templist:
                frame += "{:04X}".format(int(i))
        
        lrc = self.calc_lrc(frame)
        frame = frame + "{:02X}\r\n".format(lrc)

        self.test_frame_user = {
            "name":"自定义",
            "frame":frame
        }

        self.send_user_var.set(self.test_frame_user["frame"].replace("\r\n",""))

    def validate_test(self,i):
        # print('validate_test:%d'%i)
        # val = self.eVar[i].get()
        # # self.entry[i].delete(0,END)
        # print('val:%s'%val)
        return True

    def invalidate_test(self,i):
        pass
        # print('validate_test:%d'%i)    

    def centerWindow(self):
        """
        centers a tkinter window
        :param win: the main window or Toplevel window to center
        """
        self.update_idletasks()
        width = self.winfo_width()
        frm_width = self.winfo_rootx() - self.winfo_x()
        win_width = width + 2 * frm_width
        height = self.winfo_height()
        titlebar_height = self.winfo_rooty() - self.winfo_y()
        win_height = height + titlebar_height + frm_width
        x = self.winfo_screenwidth() // 2 - win_width // 2
        y = self.winfo_screenheight() // 2 - win_height // 2
        self.geometry('{}x{}+{}+{}'.format(width, height, x, y))
        # self.deiconify()

    def matplot_test(self):
        self.plot_win = Win2(self)
        
    def setup_serial_config(self):
        global flagWin1,flagWin2,comvar
        winSerialConfig = WinSerialConfig(self,self.fconfig['serial'])
        self.wait_window(winSerialConfig)
        if winSerialConfig.modified:
            print("参数已修改，需要重载")
            self.attributes('-topmost', False)
            resutl = showwarning("即将关闭","退出后，请重新打开,如果无法启动，请删除 config.json 再试")
            self.attributes('-topmost', True)
            self.close_win()
        else:
            print("参数没有修改，不需要重载")

        # 读取串口配置，如果配置
        # if not comvar:
        #     if isDev:
        #         comvar = devCom
        #         serial_config = (devCom,9600,8,"N",1)
        #         self.opencom() 
        #     else:
        #         # if flagWin1:
        #         #     self.Input1.focus()
        #         #     return
        #         # if flagWin2:
        #         #     self.Print1.focus()
        #         #     return
        #         res = self.get_voucherNum()
        #         if res is None: return
        #         self.code1, self.code2, comvar = res
        #         log_d('code1:%d,%d,%s',self.code1,self.code2, comvar)
        #         if comvar:
        #             self.opencom()

    # def get_voucherNum(self):
    #     '''对应Win1Input窗口类'''
    #     self.Input1 = WinSerialConfig(self)
    #     self.wait_window(self.Input1)
    #     try:
    #         return self.Input1.Retcode
    #     except:
    #         pass

    def close_win(self,event=''):
        log_d("准备关闭窗口，确认所有资源已释放")
        
        for item in g_myserials:
            print(item)
            MySerial.disconnect(item['hw'])
        
        # delay_win(self,"hello",1000)

        self.is_sending = False
        self.is_running = False

        # if isDev:
        #     self.closecom()
        #     self.after(1000,self.destroy)
        # else:
        #     ans = askyesno(parent=self,title="Warning",icon="question", message="Close the windows?")
        #     if ans:
        #         self.closecom()
        #         self.after(1000,self.destroy)
        #     else:
        #         return
        self.after(1000,self.destroy)

    def create_threads(self):
        self.gothread=threading.Thread(target=self.go,args=(cond,))
        self.recv_thread = threading.Thread(target=self.recv_thread,args=(cond,))
        self.recv_thread.start()

        self.send_thread = threading.Thread(target=self.send_thread,args=(cond,))
        self.send_thread.start()
        self.gothread.start()

    def checkBtn(self,i):
        log_d("checkBtn:%d:%d",i,self.status[i].get())
        self.fconfig['test_frame'][i]["cycle"] = bool(self.status[i].get())
    
    def test(self):
        if self.is_sending:
            self.is_sending = False
            self.btn2Text.set('开始循环')
        else:
            sum = 0
            for i in range(len(self.fconfig['test_frame'])):
                sum = sum + self.fconfig['test_frame'][i]["cycle"]
            
            if sum == 0:
                showwarning("Warnning","至少选择一个",parent=self)
                return

            self.is_sending = True
            self.btn2Text.set('停止循环')

    # pymodbus.utilities.computeLRC
    # https://pythonhosted.org/pymodbus/library/utilities.html?highlight=computelrc
    # http://www.tlu.ee/~boynamed/Tarkvaraarenduse%20Projekt%20I/FINAL/DEMO/Python26/pymodbus/utilities.py
    # https://stackoverflow.com/questions/12799122/how-can-i-calculate-longitudinal-redundancy-check-lrc
    # https://stackoverflow.com/questions/29569791/incorrect-lrc-value-calculated-from-checksum

    def calc_lrc(self,data):

        # 注意 LRC 的计算并不是直接将 ASCII 字符串进行累加，而是要两两先组合成一个 Hex ，然后对 Hex 进行累加
        # 这里很容易简单以为是将 ASCII 字符串进行累加
        res = textwrap.fill(data[1:], width=2)
        hexArr = res.split()
        # print(hexArr)
        lrc = sum(int(a,16) for a in hexArr)
        # print("lrc:0x%x"%lrc) 
        lrc = 256 - lrc%256
        # print("lrc:0x%X"%lrc) 

        return lrc

        # lrc = sum(ord(a) for a in data) & 0xff
        # lrc = (lrc ^ 0xff) + 1
        # return lrc & 0xff   

        # buf = data.encode('ascii')
        # length = len(buf)
        # print(length)

        checksum = 0
        # 
        # for i in range(0,len(hexArr)):
        #     hexByte=int.from_bytes(hexArr[i:i+1], 'little', signed=False)
        #     print("i:%d"%i)
        #     print("0x%x"%hexByte)

        #     checksum += int.from_bytes(buf[i:i+1], 'little', signed=False)
        #     # checksum &= 0xFF # 强制截断
        # print("checksum:%d"%checksum)
        # mod = checksum%256
        # print("mod:%d"%mod)
        # checksum = 256 - checksum%256
        # print("checksum:0x%x"%checksum)

        # lrc = sum(ord(a) for a in data) & 0xff
        # lrc = (lrc ^ 0xff) + 1
        # return lrc & 0xff   

        # return lrc

    def unpack_bitstring(self,string):
        byte_count = len(string)
        bits = []
        for byte in range(byte_count):
            value = ord(string[byte])
            for _ in range(8):
                bits.append((value & 1) == 1)
                value >>= 1
        return bits


    def sendUser(self,frame=''):
        log_d("sendUser frame:%s",frame)

        frame = self.send_user_var.get()
        log_d("frame11:%s",frame)
        frame = frame+ "\r\n"

        # lrc = self.calc_lrc(frame)
        # frame = frame + "{:02X}\r\n".format(lrc)
        # print("frame22:%s"%frame)
        self.send_queue.put(frame) 

        # print("lrc:0x%x"%lrc)


    def sendBtn(self,ii):
        ascframe=self.fconfig['test_frame'][ii]['frame']
        self.send_queue.put(ascframe) 

        # 启停机组
    def start(self):
        log_d("self.starts:%d",self.starts)
        if self.starts:
            log_d("发送启动指令")
            self.startxet.set('停止')
            self.starts = 1
            ascframe = ':011700000000000C0001020001D8\r\n'
        else:
            log_d("发送停机指令")
            ascframe = ':011700000000000C0001020000D9\r\n'
            self.startxet.set('启动')
            self.starts = 0
        try:
            with self.lock:
                self.send_queue.put(ascframe)
        # except:
        #     print("except")
        # else:
        #     print("send ok")
        finally:
            pass

    def fullscreen_toggle(self,event="none"):
        self.focus_set()
        self.overrideredirect(True)
        self.overrideredirect(False) #added for a toggle effect, not fully sure why it's like this on Mac OS
        self.attributes("-fullscreen", True)
        self.wm_attributes("-topmost", 1)

    def fullscreen_cancel(self,event="none"):
        self.overrideredirect(False)
        self.attributes("-fullscreen", False)
        self.wm_attributes("-topmost", 0)
        self.centerWindow()
    

    def text_clear(self):
        log_d("clear text_log")
        self.text_log.config(state="normal")
        self.text_log.delete('1.0','end')
        self.text_log.config(state="disable")
    
    def testWR(self):
        pass

    # 打开/关闭com
    def opencom(self):
        global comvar,serial_config

        log_d("serial_config %s",serial_config)

        if self.ser:
            if self.ser.is_open:
                self.closecom()
        try:
            # self.ser = serial.Serial(comvar, 9600, 7, 'E', 1, timeout=2)
            self.ser = serial.Serial(comvar, 9600, 8, 'N', 1, timeout=0.3)
            self.openornot.set(comvar)
        except(IOError):
            log_d("self.ser:%d",self.ser)
            showwarning('Warnning','串口打开失败',parent=self) 
            self.setup_serial_config()
        self.insert_text("串口%s打开成功\r\n"%comvar,'green') 

    
    def insert_text(self,strMsg,tag = 'tag_green'):
        curtime= datetime.datetime.now().strftime('%m-%d %H:%M:%S.%f')[:-3]
        self.text_log.config(state=tk.NORMAL)
        self.text_log.insert("end",curtime +' '+ strMsg,tag) # 如果是 insert 会出现鼠标点击替换的问题
        self.text_log.see(tk.END)
        self.text_log.config(state=tk.DISABLED) 

    def send_thread(self,cond):
        while self.is_running:
            try:
                send_msg = self.send_queue.get(timeout=0.1)
                send_qsize=self.send_queue.qsize()
                self.send_queue_size.set(send_qsize)
                # log_d("send_queue qsize:%d",self.send_queue_size)
                # with self.lock:
                                
                for item in g_myserials:
                    if item['if'] == 'ASC' and item['hw'] and item['hw'].is_open and item['enabled']:
                        self.last_send_time = time.time()
                        item['hw'].write(send_msg.encode('ascii'))
                        log_i("send:%s",send_msg)
                        self.insert_text(item['type']+"-->"+send_msg,'tag_white')
                        if send_qsize >= 20:
                            time.sleep(0.35)
                        elif send_qsize >= 10:
                            time.sleep(0.38)
                        elif send_qsize >=5:
                            time.sleep(0.4)
                        elif send_qsize >=1:
                            time.sleep(0.6)
                        else:
                            time.sleep(1)
            except:
                pass
            finally:
                pass
        log_d("线程send已退出")

    def recv_thread(self,cond):
        while self.is_running:
            try:
                for item in g_myserials: 
                    if item['if'] == 'ASC' and item['hw'] and item['hw'].is_open and item['enabled']:
                        line = item['hw'].readline() 
                        if len(line)>0:
                            log_i("recv:%s"%line.decode('ASCII'))
                            single_shot_time=round(time.time()-self.last_send_time,3)
                            log_d("single_shot_time:%s",single_shot_time)
                            self.tkIntval.set(single_shot_time)
                            self.insert_text(item['type']+"<--"+line.decode('ASCII'),'tag_green')
                            if line[0] != b':'[0]:
                                log_e("接收到错误的数据")
                                continue
                            if line[0:1].decode() != ":":
                                continue
                            slaveAddr = line[1:3]; 
                            # print("slaveAddr:",slaveAddr)
                            funCode = int(line[3:5],16)
                            # print("funCode:",funCode)
                            bytesCnt= int(line[5:7],16)
                            # print("bytesCnt:",bytesCnt)
                            # 00 0C 00 22 00 38 00 4E 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 01
                            if bytesCnt == 26:
                                pv_f=int(line[7:11].decode(),16)
                                self.pv.set(pv_f/10)   # 数据流解码成字符串再转int
                                flow_f=int(line[11:15].decode(),16)
                                self.flow.set(flow_f/10)
                                pre_f=int(line[15:19].decode(),16)
                                self.pre.set(pre_f/10)
                                resis_f=int(line[19:23].decode(),16)
                                self.resis.set(resis_f/10)
                            else:
                                self.pv.set("##")
                                self.flow.set("##")
                                self.pre.set("##")
                                self.resis.set("##")
                    else:
                        pass        

                # while self.ser and self.ser.is_open:
                #     self.line = self.ser.readline()
                #     if len(self.line) > 0:
                #         self.single_shot_time=round(time.time()-self.last_send_time,3)
                #         log_d("self.single_shot_time:%s",self.single_shot_time)
                #         self.tkIntval.set(self.single_shot_time)

                #         self.insert_text("<--"+self.line.decode('ASCII'),'tag_green')
                #         log_i("recv:%s"%self.line.decode('ASCII'))
                #         if self.line[0] != b':'[0]:
                #             log_e("接收到错误的数据")
                #             continue

                #         if self.line[0:1].decode() != ":":
                #             continue

                #         slaveAddr = self.line[1:3];
                #         # print("slaveAddr:",slaveAddr)
                #         funCode = int(self.line[3:5],16)
                #         # print("funCode:",funCode)
                #         bytesCnt= int(self.line[5:7],16)
                #         # print("bytesCnt:",bytesCnt)

                #         # 00 0C 00 22 00 38 00 4E 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 0C 00 01
                #         if bytesCnt == 26:
                #             pv_f=int(self.line[7:11].decode(),16)
                #             self.pv.set(pv_f/10)   # 数据流解码成字符串再转int
                #             flow_f=int(self.line[11:15].decode(),16)
                #             self.flow.set(flow_f/10)
                #             pre_f=int(self.line[15:19].decode(),16)
                #             self.pre.set(pre_f/10)
                #             resis_f=int(self.line[19:23].decode(),16)
                #             self.resis.set(resis_f/10)
                #         else:
                #             self.pv.set("##")
                #             self.flow.set("##")
                #             self.pre.set("##")
                #             self.resis.set("##")
                #     else:
                #         pass
            except:
                pass
            finally:
                pass
        log_d("线程recv已退出")
    # 读写
    def go(self,cond):
        while self.is_running:
            # self.lock.acquire()
            # cond.acquire() # 锁
            while self.is_sending:
                # if not self.is_sending:
                #     break
                # if not self.is_running:
                #     cond.release() # 解锁

                for i in range(len(self.fconfig['test_frame'])):
                    if self.fconfig['test_frame'][i]["cycle"]:
                        ascframe = self.fconfig['test_frame'][i]["frame"]
                        log_d("ascframe:%s",ascframe)
                        # :01170000000D0000000000DB
                        print("funCode:",int(ascframe[3:5],16))
                        print("re_len:",int(ascframe[9:13],16))


                        self.send_queue.put(ascframe)
                        time.sleep(self.fconfig['test_frame'][i]["cycle_int"])

                # sv输入
                # if len(self.en.get()) > 0:
                #     self.svset = str(hex(int(float(self.en.get())*10)))[2:]
                #     self.svset = self.svset.upper()
                #     self.svset = (4-len(self.svset))*'0' + self.svset        
                #     self.svset = ':011700000000000B000102'+self.svset+'E5'+'\r\n'          # 校验后续增加,多个输入格式转换考虑写模块
                #     self.ser.write(self.svset.encode('ascii'))                             # 向端口些数据 字符串必须译码
                #     self.ser.readline()
                #     print(3,time.time(),self.line)            
                # # 启停
                # if self.starts:
                #     with self.lock:
                #         ascframe = ':011700000000000C0001020001D8\r\n'.encode('ascii') 
                #         self.ser.write(ascframe)  # 向端口些数据 字符串必须译码
                #         log_i("send:%s"%ascframe)
                #         # self.ser.readline()            
                # else:
                #     pass
                    # self.ser.write(':011700000000000C0001020000D9\r\n'.encode(
                    #         'ascii'))  # 向端口些数据 字符串必须译码
                    # self.ser.readline()
                # self.lock.release()
            else:
                pass
                # log_d("已暂停")
                time.sleep(1)
        # cond.release() # 解锁
        log_d("线程go已退出")
        self.is_stopped = True

    # 关闭串口    
    def closecom(self):
        if self.ser and self.ser.is_open:
            self.ser.flush()
            self.ser.close()  # 关闭端串口
        self.openornot.set('串口关闭')

main=EHS015() 


# https://pymodbus.readthedocs.io/en/v1.3.2/examples/tk-frontend.html
# https://github.com/riptideio/pymodbus/issues/324

from multiprocessing import Process

rtu_server = pymodbus_async_server()

def rtus_thread():
    rtu_server.run_async_server(main.fconfig['serial']['RTU_485'])

if main.fconfig['serial']['RTU_485']['Enabled'] and main.fconfig["enableRTU"]:
    #my_process = Process(target = rtu_server.run_async_server(main.fconfig['serial']['RTU_485']), args = ())
    #my_process.start()
    rtus_thread = threading.Thread(target=rtus_thread)
    rtus_thread.start()
    pass


main.mainloop()

try:
    StopServer()
except:
    pass

# while not main.is_stopped:
#     log_d("等待所有线程都退出")
#     time.sleep(1)

# cond.acquire()
# cond.notify() # 唤醒休眠的线程，立即结束。
# cond.release()

if platform.system() == "Darwin":
    os.system("ps -ef | grep rtu_server | grep -v grep | awk '{print $2}' | xargs kill")

log_d("所有线程都已经退出!")


