#!/bin/python3

from datetime import datetime

fp =open("./test/info.log")
recvTime = 0
for line in fp.readlines():
    timeStr = line[:23]
    if "send" in line:
        sendTime = timeStr
        continue
    elif "recv" in line:
        recvTime = timeStr

    if recvTime:
        delta = [recvTime,sendTime]
        # print(delta)
        t1 =  datetime.strptime(delta[0], "%Y-%m-%d %H:%M:%S,%f")
        t2 =  datetime.strptime(delta[1], "%Y-%m-%d %H:%M:%S,%f")
        print(t1-t2)

fp.close()
