#!/bin/zsh
echo "文件系统制作成功"
# echo "复制到本地 OneKeyBurnTool/release/scripts/images/"
# 这里暂时没有找到好的办法，暂时通过局域网来复制，需要 mac 开启 sshd 127.17.0.1 行不通
# scp ./output/images/rootfs.ubi yangxiyuan@192.168.1.135:/Users/yangxiyuan/Projects/airsys-netgates/OneKeyBurnTool/release/scripts/images
echo "开始上传到 minio aliyun/images"

# mc config host list
# mc config host add aliyun http://47.94.84.166:9000 admin snahko12

mc cp ./rtthread.bin aliyun/images

zip -r webnet.zip ./webnet
mc cp ./webnet.zip aliyun/images

echo "通知到企业微信"
curl --location --request POST 'https://qyapi.weixin.qq.com/cgi-bin/webhook/send?key=ba942f5a-4cd1-4a5e-8086-acecfee06849' \
    --header 'Content-Type: application/json' \
    --data-raw '{
    "msgtype": "markdown",
    "markdown": {
    "content": "<font color=\"warning\">网关</font>\n> 版本发布：<font color=\"comment\">转换器--固件和web更新</font>\n[rtthread.bin](http://47.94.84.166:9000/images/rtthread.bin)\n[webnet](http://47.94.84.166:9000/images/webnet.zip)\n>"
    }
}'
