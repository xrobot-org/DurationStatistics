# DurationStatistics

LibXR 作用域耗时统计库 / Scoped-duration statistics library for LibXR

## 1. 模块作用 / Purpose

DurationStatistics 是 LibXR 的作用域耗时统计库，累计测量结果，输出由使用它的模块负责，通常在该模块的 `OnMonitor()` 中读取快照并决定输出格式。

DurationStatistics 在 manifest 中标记为库（`standalone: false`），由其他模块引入使用。其他模块包含 `DurationStatistics.hpp`，并在自己的 manifest `depends` 中列出 `xrobot-org/DurationStatistics`，由此把它拉入工程。

多个业务线程可以向同一个统计器记录数据；监控线程可以并发调用 `GetSummary()`，返回的四个字段来自同一个一致快照。记录和读取通过 LibXR `Mutex` 同步，仅在任务上下文调用。结束时间在加锁之前读取，锁等待不计入耗时。统计从对象构造后持续累计。

DurationStatistics is a scoped-duration statistics library for LibXR. It accumulates measurements; the Module that uses it handles the output, typically by reading a snapshot in its `OnMonitor()` and choosing the output format.

DurationStatistics is marked as a library in its manifest (`standalone: false`) and is used by other Modules. Other Modules include `DurationStatistics.hpp` and list `xrobot-org/DurationStatistics` in their manifest `depends`, which pulls it into the project.

Multiple worker threads can record into one collector; a monitor thread can call `GetSummary()` concurrently, and the four returned fields come from one consistent snapshot. Recording and reading are synchronized by a LibXR `Mutex` and are called from task context only. The end time is read before the lock is taken, so lock contention is excluded from the measured duration. Statistics accumulate from the construction of the object onward.

## 2. 接口 / API

命名空间 `XRobot` 中：

- `DurationStatistics`：统计器，默认构造，复制与移动均被删除。
- `ScopedMeasurement DurationStatistics::Measure()`：开始一次测量。返回的对象在析构时提交一次从构造到析构的微秒耗时，复制与移动均被删除，一次作用域只统计一次。`ScopedMeasurement` 覆盖完整的目标作用域，其生命周期不超过所属的 `DurationStatistics`。
- `Summary DurationStatistics::GetSummary() const`：返回快照 `Summary`，字段 `sample_count`、`average_us`、`minimum_us`、`maximum_us`（`uint64_t`，单位 µs）。平均值为整数除法；零样本时所有字段为 `0`。

In namespace `XRobot`:

- `DurationStatistics`: the collector; default-constructible, with copy and move deleted.
- `ScopedMeasurement DurationStatistics::Measure()`: start one measurement. The returned object submits one duration in microseconds, from its construction to its destruction, when it is destroyed; copy and move are deleted, so one scope is counted once. A `ScopedMeasurement` covers the complete target scope and its lifetime ends before that of the owning `DurationStatistics`.
- `Summary DurationStatistics::GetSummary() const`: return the `Summary` snapshot with the fields `sample_count`, `average_us`, `minimum_us`, `maximum_us` (`uint64_t`, in µs). The average uses integer division; all fields are `0` when no sample has been recorded.

## 3. 构造接口 / Constructor

```cpp
DurationStatistics() = default;
```

依赖：无构造参数。

配置参数：无构造参数。

Dependencies: no constructor parameters.

Configuration parameters: no constructor parameters.

## 4. Topic

无 / None

## 5. 配置示例 / Configuration Example

使用方在自己头文件的 manifest 中声明依赖：

The Module that uses it declares the dependency in the manifest of its header:

```text
depends:
- id: xrobot-org/DurationStatistics
  ref: same-or-dev
```

BSP 添加这样的模块并运行 `xrobot setup` 时，本库随 `depends` 自动解析和拉取；也可以显式添加：

When a BSP adds such a Module and runs `xrobot setup`, this library is resolved and fetched through `depends`; it can also be added explicitly:

```sh
xrobot module add xrobot-org/DurationStatistics
xrobot setup
```

在使用方的代码中：

In the code of the using Module:

```cpp
#include "DurationStatistics.hpp"

class Detector
{
 public:
  void Detect()
  {
    auto measurement = detect_duration_.Measure();
    // Detection work.
  }

  void OnMonitor()
  {
    const auto summary = detect_duration_.GetSummary();
    // Print summary.sample_count, average_us, minimum_us, maximum_us here.
  }

 private:
  XRobot::DurationStatistics detect_duration_;
};
```

## 6. 依赖与硬件 / Dependencies and Hardware

依赖：LibXR（`Mutex`、`Timebase`、`MicrosecondTimestamp`）。

硬件：无。

Dependencies: LibXR (`Mutex`, `Timebase`, `MicrosecondTimestamp`).

Hardware: none.
