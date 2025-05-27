#!/bin/bash

set -e # 如果任何命令失败（返回非0状态码），立即退出脚本

mkdir -p `pwd`/build

rm -rf `pwd`/build/*
cd `pwd`/build/ && 
    cmake .. &&
    make

cd ..

echo "======================================================================="
echo "The libFiles has been created in $(pwd)/lib"
echo "======================================================================="
#copy headerFiles  to MymuduoHeaderFiles
mkdir -p `pwd`/MymuduoHeaderFiles
rm -rf `pwd`/MymuduoHeaderFiles/*

mkdir -p /usr/include/muduo_rebuild
rm -rf /usr/include/muduo_rebuild/*

for header in `ls *.h`
do
    cp $header `pwd`/MymuduoHeaderFiles
    cp $header /usr/include/muduo_rebuild
done

echo "========================================================================================="
echo "The HeaderFiles has been created in /usr/include/muduo_rebuild"
echo "========================================================================================="



echo "========================================================================================="
echo "The HeaderFiles has been created in $(pwd)/MymuduoHeaderFiles"
echo "========================================================================================="

cp `pwd`/lib/libmuduo_rebuild.so /usr/lib

echo "========================================================================================="
echo "The libFiles has been created in /usr/lib"
echo "========================================================================================="

ldconfig