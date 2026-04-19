# LwIPC_codex

轻量级 C++ IPC 原型工程，面向机器人/自动驾驶中间件场景，强调：
- 本地低延迟（共享内存）
- 可预测（固定内存与简单 QoS）
- 易测试（ctest + 单元测试）

## 当前实现

### 1. 核心抽象
- `core.hpp`：`Node`、`Channel`、QoS 抽象封装。

### 2. 传输与内存
- `transport.hpp`：统一传输接口 `ITransport`，提供 `ShmTransport` / `UdpTransport`。
- `ShmSegment`：基于 POSIX `shm_open + mmap` 的共享内存段封装（RAII）。
- `MessageHeader` / `MessageView`：固定头 + payload 视图，便于零拷贝传递。
- `MemoryPool`：固定槽位内存池，支持预分配与槽位回收，避免热路径 `new/delete`。

### 3. 队列
- `SpscRingBuffer<T, Capacity>`：无锁 SPSC 环形队列，单生产者/单消费者场景优先。

### 4. 发布订阅核心
- `Broker`：线程安全 topic 路由与订阅管理。
- `Publisher` / `Subscriber`：简化的 pub/sub API，支持 QoS 配置结构。

### 5. 发现与路由
- `StaticTopology`：读取静态拓扑配置（CSV），提供 topic -> channel 映射查询。
- `HeartbeatMonitor`：进程心跳记录与超时节点检测。

### 6. 调度执行
- `Executor`：单工作线程执行器，提供 `post`/`stop`，用于隔离回调执行线程。

### 7. 可观测性
- `Metrics`：内建发布/投递/丢弃计数与最近延迟记录。

### 8. Protobuf 编解码
- 新增 `proto/lwipc_frame.proto`。
- `proto_codec.hpp`：在启用 Protobuf 时，支持 `MessageView` 的 protobuf 编码与头部解码。
- CMake 默认开启 `LWIPC_ENABLE_PROTOBUF=ON`，若环境存在 protobuf 则自动启用。

### 9. 最小 QoS 模型
- `Reliability`: `BestEffort` / `Reliable`
- `Durability`: `Volatile` / `TransientLocal`
- `keep_last`

## 目录结构

```text
.
├── CMakeLists.txt
├── include/lwipc
│   ├── broker.hpp
│   ├── core.hpp
│   ├── executor.hpp
│   ├── heartbeat.hpp
│   ├── memory_pool.hpp
│   ├── message.hpp
│   ├── metrics.hpp
│   ├── proto_codec.hpp
│   ├── ring_buffer_spsc.hpp
│   ├── shm_segment.hpp
│   ├── topology.hpp
│   └── transport.hpp
├── proto
│   └── lwipc_frame.proto
├── src
│   ├── broker.cpp
│   ├── executor.cpp
│   ├── heartbeat.cpp
│   ├── memory_pool.cpp
│   ├── proto_codec.cpp
│   ├── shm_segment.cpp
│   ├── topology.cpp
│   └── transport.cpp
└── tests
    ├── test_broker.cpp
    ├── test_core.cpp
    ├── test_executor.cpp
    ├── test_heartbeat.cpp
    ├── test_memory_pool.cpp
    ├── test_metrics.cpp
    ├── test_proto_codec.cpp
    ├── test_ringbuffer.cpp
    ├── test_shm.cpp
    ├── test_topology.cpp
    └── test_transport.cpp
```

## 构建与测试

```bash
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

## 迭代记录（按你的要求持续记录）

### 2026-04-14 / 第 1 次
- 初始化工程：CMake、`lwipc` 静态库、`Broker`、`ShmSegment`、`SpscRingBuffer`、消息与 QoS 模型。
- 新增 3 个测试：`test_ringbuffer`、`test_broker`、`test_shm`。

### 2026-04-14 / 第 2 次
- 新增内存模型：`MemoryPool`（固定块预分配 + 回收）。
- 新增调度执行：`Executor`（异步任务执行线程）。
- 新增可观测性：`Metrics`（发布/投递/丢弃计数 + 延迟记录）。
- 新增 3 个测试：`test_memory_pool`、`test_metrics`、`test_executor`。

### 2026-04-14 / 第 3 次
- 新增发现与路由基础：`StaticTopology`（静态拓扑加载 + topic 查询）。
- 新增健康检测基础：`HeartbeatMonitor`（心跳记录 + 超时检测）。
- 新增 2 个测试：`test_topology`、`test_heartbeat`。

### 2026-04-14 / 第 4 次
- 新增 MPMC 无锁队列：`MpmcRingBuffer<T>`（多生产者多消费者场景）
  - 基于序列号的无锁算法
  - 支持 SPSC/MPSC/SPMC/MPMC 所有并发模式
  - 性能测试：>1.2M msg/s (单生产者单消费者)
- 新增原子读写锁：`AtomicRWLock`（读多写少场景优化）
  - 支持多读者并发访问
  - 写者独占保证
  - RAII 守卫：`ReadLockGuard` / `WriteLockGuard`
  - 性能测试：读锁 ~27ns/op，写锁 ~26ns/op
- 新增 2 个测试：`test_ringbuffer_mpmc`、`test_atomic_rwlock`
- 总计 10 个单元测试全部通过

### 2026-04-14 / 第 5 次
- 新增 `core.hpp`：`Node`、`Channel` 抽象。
- 新增 `transport.hpp`：统一传输接口 + `ShmTransport` / `UdpTransport`。
- 接入 protobuf：新增 `.proto` 文件与 `proto_codec`（受 `LWIPC_ENABLE_PROTOBUF` 控制）。
- 新增 2 个测试：`test_core`、`test_transport`，并在有 protobuf 时启用 `test_proto_codec`。

## 下一步建议

- 增加 `Unix Domain Socket` 控制面（发现广播/心跳上报/拓扑热更新）
- 增加多进程 loan/reclaim（真正零拷贝生命周期管理）
- 增加可靠传输 ACK 与 `KEEP_LAST_N` 回压策略
- 增加端到端延迟直方图导出
