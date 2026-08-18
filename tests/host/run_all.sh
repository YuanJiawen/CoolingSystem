#!/bin/sh
# 全量 host 测试入口:依次运行所有套件
# 用法:./run_all.sh
set -e
cd "$(dirname "$0")"

./run_cooling_control.sh
./run_cooling_actuator.sh
./run_pressure_sampler.sh
./run_event_framework.sh
./run_sdio_nofatal.sh
