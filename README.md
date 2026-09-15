# DurationStatistics

## Static assembly source line

This source line uses explicit C++ constructor dependencies and ordered instance
arguments. Inspect the current primary header with `xrobot_mod_parser --path .`;
its declarations, not old manifest/config examples, define the interface.
Historical HardwareContainer/ApplicationManager examples below apply only to the
older dynamic source tags. Device/protocol descriptions remain relevant.
See the XRobot [migration guide](https://github.com/xrobot-org/XRobot/blob/dev/MIGRATION.md).
Compilation is not hardware validation; retain version-specific board evidence.


`DurationStatistics` 是一个面向 LibXR 的通用作用域耗时统计模块。
它只累计测量结果，不负责打印、文件输出或消息发布；应用可以在自己的
`OnMonitor()` 中读取快照并决定输出格式。

`DurationStatistics` is a reusable scoped-duration statistics module for LibXR.
It only accumulates measurements. Applications decide how and where to print the
snapshot, typically from `OnMonitor()`.

## 集成方式 / Integration

```cpp
#include "DurationStatistics.hpp"

class Detector
{
  XRobot::DurationStatistics detect_duration_;

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
};
```

`ScopedMeasurement` 在析构时提交一次从构造到析构的微秒耗时，因此应当让它
覆盖完整的目标作用域。它不可复制、不可移动，避免一次作用域被重复统计。

`ScopedMeasurement` records one microsecond duration when it is destroyed. Keep
it alive for the complete scope being measured, and do not let it outlive its
`DurationStatistics` owner. It is neither copyable nor movable, preventing
accidental duplicate samples.

## 并发约束 / Concurrency

- 多个业务线程可以向同一个统计器记录数据。
- `GetSummary()` 可以由监控线程并发调用，并返回一致的快照。
- 记录和读取通过 LibXR `Mutex` 同步，只能在任务上下文调用，不能在 ISR 中调用。
- 统计从对象构造后持续累计；零样本时所有字段均为 `0`。

Multiple worker threads may record into one collector. A monitor may call
`GetSummary()` concurrently, and the returned four fields come from one
consistent snapshot. Recording and reading are synchronized by a LibXR `Mutex`,
so both operations are task-context APIs and must not be called from an ISR.
Statistics are cumulative for the lifetime of the object.

## 依赖 / Depends

- LibXR `Timebase` / `MicrosecondTimestamp`
