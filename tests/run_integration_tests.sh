#!/bin/bash
# LwIPC 集成测试脚本
# 测试跨进程通信和网络通信功能

set -e

echo "=========================================="
echo "LwIPC 集成测试"
echo "=========================================="

# 颜色定义
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# 项目根目录
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$PROJECT_ROOT/build"
BIN_DIR="$BUILD_DIR/bin"

# 清理函数
cleanup() {
    echo -e "\n${YELLOW}清理测试进程...${NC}"
    pkill -f "example_ipc_stress" 2>/dev/null || true
    pkill -f "example_network_bridge" 2>/dev/null || true
    rm -f /dev/shm/lwipc_* 2>/dev/null || true
    rm -f /tmp/lwipc_* 2>/dev/null || true
}

trap cleanup EXIT

# 构建项目
echo -e "\n${YELLOW}[1/4] 构建项目...${NC}"
cd "$PROJECT_ROOT"
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# 检查必要的可执行文件
if [ ! -f "$BIN_DIR/example_ipc_stress" ]; then
    echo -e "${RED}错误：未找到 example_ipc_stress 可执行文件${NC}"
    exit 1
fi

if [ ! -f "$BIN_DIR/example_network_bridge" ]; then
    echo -e "${RED}错误：未找到 example_network_bridge 可执行文件${NC}"
    exit 1
fi

echo -e "${GREEN}✓ 构建成功${NC}"

# 测试 1: 跨进程通信 (IPC)
echo -e "\n${YELLOW}[2/4] 测试跨进程通信 (IPC)...${NC}"

# 启动消费者进程
echo "启动消费者进程..."
"$BIN_DIR/example_ipc_stress" consumer > /tmp/ipc_consumer.log 2>&1 &
CONSUMER_PID=$!

# 等待消费者初始化
sleep 2

# 启动生产者进程
echo "启动生产者进程..."
"$BIN_DIR/example_ipc_stress" producer > /tmp/ipc_producer.log 2>&1 &
PRODUCER_PID=$!

# 等待测试完成 (最多 60 秒)
TIMEOUT=60
ELAPSED=0
while kill -0 $PRODUCER_PID 2>/dev/null && [ $ELAPSED -lt $TIMEOUT ]; do
    sleep 1
    ELAPSED=$((ELAPSED + 1))
    if [ $((ELAPSED % 10)) -eq 0 ]; then
        echo "  等待测试完成... ($ELAPSED 秒)"
    fi
done

# 等待消费者结束
sleep 3
kill $CONSUMER_PID 2>/dev/null || true
kill $PRODUCER_PID 2>/dev/null || true

# 显示结果
echo -e "\n${GREEN}=== IPC 测试结果 ===${NC}"
if [ -f /tmp/ipc_producer.log ]; then
    grep -A 10 "Producer Results" /tmp/ipc_producer.log || echo "无生产者结果"
fi
if [ -f /tmp/ipc_consumer.log ]; then
    grep -A 10 "Consumer Results" /tmp/ipc_consumer.log || echo "无消费者结果"
fi

# 测试 2: 网络通信
echo -e "\n${YELLOW}[3/4] 测试网络通信...${NC}"

# 启动接收者
echo "启动接收者 (port 9001)..."
"$BIN_DIR/example_network_bridge" receiver 9000 > /tmp/net_receiver.log 2>&1 &
RECEIVER_PID=$!

sleep 1

# 启动发送者
echo "启动发送者 (port 9000)..."
"$BIN_DIR/example_network_bridge" sender > /tmp/net_sender.log 2>&1 &
SENDER_PID=$!

# 等待测试完成
TIMEOUT=30
ELAPSED=0
while kill -0 $SENDER_PID 2>/dev/null && [ $ELAPSED -lt $TIMEOUT ]; do
    sleep 1
    ELAPSED=$((ELAPSED + 1))
done

# 等待接收者处理完成
sleep 2
kill $RECEIVER_PID 2>/dev/null || true
kill $SENDER_PID 2>/dev/null || true

# 显示结果
echo -e "\n${GREEN}=== 网络通信测试结果 ===${NC}"
if [ -f /tmp/net_sender.log ]; then
    grep -A 8 "Sender Results" /tmp/net_sender.log || echo "无发送者结果"
fi
if [ -f /tmp/net_receiver.log ]; then
    grep -A 10 "Receiver Results" /tmp/net_receiver.log || echo "无接收者结果"
fi

# 测试 3: 运行现有单元测试
echo -e "\n${YELLOW}[4/4] 运行单元测试...${NC}"
cd "$BUILD_DIR"
if [ -f "tests/run_tests" ]; then
    ./tests/run_tests || echo -e "${YELLOW}部分单元测试未通过${NC}"
else
    echo -e "${YELLOW}未找到单元测试可执行文件${NC}"
fi

# 总结
echo -e "\n=========================================="
echo -e "${GREEN}集成测试完成${NC}"
echo "=========================================="
echo ""
echo "日志文件位置:"
echo "  IPC 生产者：/tmp/ipc_producer.log"
echo "  IPC 消费者：/tmp/ipc_consumer.log"
echo "  网络发送者：/tmp/net_sender.log"
echo "  网络接收者：/tmp/net_receiver.log"
echo ""
echo "手动运行示例:"
echo "  IPC 测试:"
echo "    终端 1: $BIN_DIR/example_ipc_stress consumer"
echo "    终端 2: $BIN_DIR/example_ipc_stress producer"
echo ""
echo "  网络测试:"
echo "    终端 1: $BIN_DIR/example_network_bridge receiver 9000"
echo "    终端 2: $BIN_DIR/example_network_bridge sender"
echo ""
