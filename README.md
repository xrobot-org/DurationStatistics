# DurationStatistics

`DurationStatistics` 是一个面向 LibXR 的通用作用域耗时统计库。它只累计测量结果，
不负责打印、文件输出或消息发布；使用它的模块可以在自己的 `OnMonitor()` 中读取快照并
决定输出格式。

`DurationStatistics` is a reusable scoped-duration statistics library for LibXR.
It only accumulates measurements; the Module using it decides how and where to
print the snapshot, typically from its `OnMonitor()`.

## 依赖 / Dependencies

这是一个库（`standalone: false`）：它从不被实例化，没有构造参数，也不出现在
`User/xrobot.yaml` 的 `modules:` 中。其他模块包含 `DurationStatistics.hpp`，并在自己的
manifest `depends` 中列出 `xrobot-org/DurationStatistics`，由此把它拉入工程。
本库只依赖 LibXR（`Mutex`、`Timebase`、`MicrosecondTimestamp`）。

This is a library (`standalone: false`): it is never instantiated, has no
constructor arguments and never appears in the `modules:` list of
`User/xrobot.yaml`. Other Modules include `DurationStatistics.hpp` and list
`xrobot-org/DurationStatistics` in their manifest `depends`, which pulls it in. The
library itself uses only LibXR (`Mutex`, `Timebase`, `MicrosecondTimestamp`).

## 接口 / API

命名空间 `XRobot` 中 / In namespace `XRobot`:

- `DurationStatistics`：统计器，默认构造，不可复制、不可移动。/ The collector;
  default-constructible, neither copyable nor movable.
- `ScopedMeasurement DurationStatistics::Measure()`：开始一次测量。返回的对象在析构时
  提交一次从构造到析构的微秒耗时，不可复制、不可移动，避免一次作用域被重复统计。/
  Starts one measurement. The returned object records one microsecond duration
  when it is destroyed; it is neither copyable nor movable, preventing duplicate
  samples.
- `Summary DurationStatistics::GetSummary() const`：返回快照 `Summary`，字段
  `sample_count`、`average_us`、`minimum_us`、`maximum_us`（`uint64_t`，微秒）。平均值
  为整数除法；零样本时所有字段均为 `0`。/ Returns a `Summary` snapshot with
  `sample_count`, `average_us`, `minimum_us`, `maximum_us` (`uint64_t`,
  microseconds). The average uses integer division; all fields are `0` when no
  sample has been recorded.

`ScopedMeasurement` 应覆盖完整的目标作用域，且不能比它的 `DurationStatistics` 活得更久。
Keep the `ScopedMeasurement` alive for the complete scope being measured, and do
not let it outlive its `DurationStatistics`.

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

## 并发约束 / Concurrency

- 多个业务线程可以向同一个统计器记录数据；`GetSummary()` 可以由监控线程并发调用，
  返回的四个字段来自同一个一致快照。
- 记录和读取通过 LibXR `Mutex` 同步，只能在任务上下文调用，不能在 ISR 中调用。
  结束时间在加锁之前读取，锁等待不计入耗时。
- 统计从对象构造后持续累计，没有复位接口。

- Multiple worker threads may record into one collector; a monitor may call
  `GetSummary()` concurrently, and the four returned fields come from one
  consistent snapshot.
- Recording and reading are synchronized by a LibXR `Mutex`, so both are
  task-context APIs and must not be called from an ISR. The end time is read before
  the lock is taken, so lock contention is not counted.
- Statistics are cumulative for the lifetime of the object; there is no reset.

## 使用 / Use

在使用它的模块头文件 manifest 中声明依赖：
Declare the dependency in the manifest of the Module that uses it:

```text
depends:
- id: xrobot-org/DurationStatistics
  ref: same-or-dev
```

BSP 中添加这样的模块并运行 `xrobot setup` 时，本库会随其 `depends` 自动解析和拉取；
也可以显式添加：
When a BSP adds such a Module and runs `xrobot setup`, this library is resolved and
fetched through its `depends`; it can also be added explicitly:

```sh
xrobot module add xrobot-org/DurationStatistics
xrobot setup
```

不需要（也不能）`xrobot instance add`。
There is no `xrobot instance add` for a library.

`xrobot module show .`（在本仓库中）或
`xrobot module show Modules/xrobot-org/DurationStatistics`（在 BSP 中）打印 manifest。
`xrobot module show .` in this repository, or
`xrobot module show Modules/xrobot-org/DurationStatistics` in a BSP, prints the
manifest.
