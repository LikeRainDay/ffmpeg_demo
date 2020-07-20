//
// Created by 候帅 on 2020/6/5.
//
#include "../test/rtmp/test_rtmp_pusher.h"

int main() {
    // 测试 本地文件推流
    test_rtmp_pusher test_pusher;
    test_pusher.doPush();
    return 0;
}