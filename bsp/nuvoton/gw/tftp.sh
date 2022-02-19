#!/bin/bash
# tftp <<EOF
# connect 192.168.1.100
# mode binary
# put index.html
# put test.js
# put favicon.ico
# quit
# EOF
# https://blog.csdn.net/sinat_37394380/article/details/54668971


# for file in ./*; do
#     if test -f $file; then
#         arr=(${arr[*]} ${file})
#     fi
# done
# echo ${arr[@]}


# 文件夹不能为空，否则会无限循环
function getdir() {
    echo $1
    for file in $1/*; do
        if test -f $file; then
            echo $file
            arr=(${arr[*]} $file)
        else
            getdir $file
        fi
    done
}

function tALL(){
    cd ./webnet

    getdir .
    echo  ${arr[@]}
    for file in ${arr[@]}
    do
        echo ${file}
        filename=${file##*/}
        echo ${filename}
        curl -T ./${file} tftp://192.168.1.100/${file}
    done
}

# 上传 test.js 到 webnet/js 目录
# ./tftp.sh js/test.js js
# 上传 config.html 到 webnet/ 目录
# ./tftp.sh config.html
function tConfig()
{
    echo "上传配置文件"$1 $2
    curl -T ./webnet/$1 tftp://192.168.1.100/$2/
}

if [ -z $1 ];then
    echo "arg null"
    tALL
else
    echo "arg not null"
    tConfig $1 $2
fi

exit;

