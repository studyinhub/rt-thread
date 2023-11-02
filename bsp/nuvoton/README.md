# Nuvoton BSP Description

Current supported BSP shown in below table:

| **BSP Folder**              | **Board Name**     |
| --------------------------------- | ------------------------ |
| [numaker-iot-m487](numaker-iot-m487) | Nuvoton NuMaker-IoT-M487 |
| [numaker-pfm-m487](numaker-pfm-m487) | Nuvoton NuMaker-PFM-M487 |
| [nk-980iot](nk-980iot)               | Nuvoton NK-980IOT        |
| [numaker-m2354](numaker-m2354)       | Nuvoton NuMaker-M2354    |
| [nk-rtu980](nk-rtu980)               | Nuvoton NK-RTU980        |
| [nk-n9h30](nk-n9h30)                 | Nuvoton NK-N9H30         |

突然发现在 Windows 下的编译有问题，主要是 Python2 的问题，那么自己下载 Python 3.9.9 来试试

pip install scons

在 eveything 里边搜索 gcc-arm，发现在 rt studio env 里都发现了编译器。

修改 rtconfig.py 使之兼容 Windows 平台

ifconfig e0 192.168.2.200 192.168.2.1 255.255.255.0
