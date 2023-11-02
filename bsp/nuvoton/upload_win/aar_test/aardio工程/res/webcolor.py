from tkinter import Tk,Label
from os.path import abspath
from inspect import getsourcefile
import sys

path = abspath(getsourcefile(lambda:0))

# path1 = os.path.dirname(os.path.abspath(sys.argv[0]))
end = path.rindex('\\')
print("end:",end)
path = path[:end]+'\\'
print("path:",path)
cur_path = path +"webcolors.txt"
file=open(cur_path,"r")#改成你的位置，参考第一部分
root=Tk()
content=file.read().splitlines()[1:]
for i in range(len(content)):
    content[i]=content[i].split("#")[0].rstrip()
for x in range(20):
    for y in range(7):#应该是140个，如果不是可以修改代码
        Label(bg=content[7*x+y],width=20,font=("宋体","10")).grid(row=x*2,column=y)
        Label(text=content[7*x+y],font=("宋体","10")).grid(row=x*2+1,column=y)#如字体不好可以自行调整
file.close()
root.mainloop()#避免闪退