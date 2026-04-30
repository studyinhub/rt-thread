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

CUR_PATH="$(cd "$(dirname "$0")" && pwd)"

ver=0.2

TARGET_IP=192.168.2.200

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

function tALL() {
  cd ${CUR_PATH}/upload_win/pack_src/webnet/
  getdir .
  echo ${arr[@]}
  for file in ${arr[@]}; do
    echo ${file}
    filename=${file##*/}
    echo ${filename}
    curl -T ./${file} tftp://${TARGET_IP}/webnet/${file}
  done
}

# 上传 test.js 到 webnet/js 目录
# ./tftp.sh js/test.js js
# 上传 config.html 到 webnet/ 目录
# ./tftp.sh config.html
function tConfig() {
  echo "上传${CUR_PATH}/$1文件到 $2"
  # curl -T ${CUR_PATH}/webnet/$1 tftp://${TARGET_IP}/$2/
}

function upload() {
  echo "上传${CUR_PATH}/$1 到 $2"

  curl -T ${CUR_PATH}/upload_win/pack_src/webnet/$1 tftp://${TARGET_IP}/$2

}

while getopts ":r:v" opt; do
  case $opt in
  r)
    src=$OPTARG
    echo "src is $a"
    ;;
  v)
    echo $ver
    ;;
  ?)
    echo "getopts param error"
    exit 1
    ;;
  esac
done

if [ -z $1 ]; then
  # 没有参数就默认上传 webnet
  echo "开始上传 webnet"
  tALL
else
  if [ -z $src ]; then
    echo "上传文件 $1"
    src=$1
    target=$2
  else
    echo "上传文件夹 $src"
    target=$3
  fi

  upload $src $target
  # tConfig $1 $2
fi

exit
