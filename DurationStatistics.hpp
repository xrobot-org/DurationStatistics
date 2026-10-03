#pragma once

// clang-format off
/* === MODULE MANIFEST V2 ===
module_description: LibXR 作用域耗时统计库 / Scoped-duration statistics library for LibXR
standalone: false
depends: []
=== END MANIFEST === */
// clang-format on

#include <cstdint>

#include "mutex.hpp"
#include "timebase.hpp"

namespace XRobot
{

/**
 * @brief 作用域耗时的累计统计。
 *        Accumulated statistics for scoped durations.
 *
 * 该类采集统计数据，输出由使用方负责。把 `Measure()` 返回的对象放在待测作用域内，
 * 其析构函数提交一次耗时。业务线程可以并发记录，监控线程通过 `GetSummary()` 读取
 * 一致快照。记录和读取使用 LibXR 互斥锁，仅在任务上下文调用。
 *
 * This class collects statistics; the output is the responsibility of the user. Place
 * the object returned by `Measure()` in the measured scope; its destructor submits one
 * duration. Worker threads can record concurrently, and a monitor thread reads a
 * consistent snapshot through `GetSummary()`. Recording and reading use a LibXR mutex
 * and are called from task context only.
 */
class DurationStatistics
{
 public:
  /**
   * @brief 统计快照。
   *        Statistics snapshot.
   */
  struct Summary
  {
    uint64_t sample_count = 0;  ///< 已记录的样本数
                                ///< Number of recorded samples
    uint64_t average_us = 0;    ///< 平均耗时，单位 µs
                                ///< Average duration in µs
    uint64_t minimum_us = 0;    ///< 最小耗时，单位 µs
                                ///< Minimum duration in µs
    uint64_t maximum_us = 0;    ///< 最大耗时，单位 µs
                                ///< Maximum duration in µs
  };

  /**
   * @brief 一个作用域测量。
   *        One scoped measurement.
   *
   * 复制与移动均被删除，一个待测作用域只提交一次样本。所属的统计器的生命周期长于
   * 该对象。
   *
   * Copy and move are deleted, so one measured scope submits one sample. The owning
   * collector outlives this object.
   */
  class ScopedMeasurement
  {
   public:
    ScopedMeasurement(const ScopedMeasurement&) = delete;
    ScopedMeasurement& operator=(const ScopedMeasurement&) = delete;
    ScopedMeasurement(ScopedMeasurement&&) = delete;
    ScopedMeasurement& operator=(ScopedMeasurement&&) = delete;

    /**
     * @brief 结束测量并提交耗时。
     *        Finish the measurement and submit the duration.
     */
    ~ScopedMeasurement() noexcept;

   private:
    friend class DurationStatistics;

    /**
     * @brief 创建作用域测量对象。
     *        Construct a scoped measurement.
     *
     * @param owner 接收测量结果的统计器。
     *              Collector that receives the result.
     * @param start 待测作用域的开始时间。
     *              Start time of the measured scope.
     */
    ScopedMeasurement(DurationStatistics& owner,
                      LibXR::MicrosecondTimestamp start) noexcept
        : owner_(&owner), start_(start)
    {
    }

    DurationStatistics* owner_;          ///< 结果接收者 / Result collector
    LibXR::MicrosecondTimestamp start_;  ///< 开始时间 / Start timestamp
  };

  /**
   * @brief 构造空的耗时统计器。
   *        Construct an empty duration statistics collector.
   */
  DurationStatistics() = default;

  /// 统计器持有同步状态和累计值，复制与移动均被删除。
  /// The collector holds synchronization state and accumulated values; copy and move
  /// are deleted.
  DurationStatistics(const DurationStatistics&) = delete;
  DurationStatistics& operator=(const DurationStatistics&) = delete;
  DurationStatistics(DurationStatistics&&) = delete;
  DurationStatistics& operator=(DurationStatistics&&) = delete;

  /**
   * @brief 开始一次作用域测量。
   *        Start one scoped measurement.
   *
   * @return 测量对象，其析构记录经过的耗时。
   *         Measurement whose destructor records the elapsed duration.
   */
  [[nodiscard]] ScopedMeasurement Measure() noexcept
  {
    return ScopedMeasurement(*this, LibXR::Timebase::GetMicroseconds());
  }

  /**
   * @brief 读取统计快照。
   *        Read a statistics snapshot.
   *
   * 平均值采用整数除法；没有样本时四个字段均为 `0`。
   * The average uses integer division; all four fields are `0` when no sample has been
   * recorded.
   *
   * @return 样本数、平均值、最小值和最大值，耗时单位为 µs。
   *         Sample count, average, minimum and maximum, durations in µs.
   */
  [[nodiscard]] Summary GetSummary() const noexcept;

 private:
  friend class ScopedMeasurement;

  /**
   * @brief 提交一次已结束的作用域测量。
   *        Submit one finished scoped measurement.
   *
   * 结束时间在获取互斥锁之前读取，锁等待不计入被测作用域的耗时。
   * The end timestamp is read before the mutex is acquired, so lock contention is
   * excluded from the measured duration.
   *
   * @param start 待测作用域的开始时间。
   *              Start time of the measured scope.
   */
  void Record(LibXR::MicrosecondTimestamp start) noexcept;

  mutable LibXR::Mutex mutex_;  ///< 保护累计值 / Protects accumulated values
  uint64_t sample_count_ = 0;   ///< 已记录样本数 / Recorded sample count
  uint64_t total_us_ = 0;       ///< 累计耗时 / Accumulated duration
  uint64_t minimum_us_ = 0;     ///< 最小耗时 / Minimum duration
  uint64_t maximum_us_ = 0;     ///< 最大耗时 / Maximum duration
};

inline DurationStatistics::ScopedMeasurement::~ScopedMeasurement() noexcept
{
  owner_->Record(start_);
}

inline void DurationStatistics::Record(LibXR::MicrosecondTimestamp start) noexcept
{
  const LibXR::MicrosecondTimestamp end = LibXR::Timebase::GetMicroseconds();
  const uint64_t duration_us = static_cast<uint64_t>(end - start);
  LibXR::Mutex::LockGuard lock(mutex_);

  if (sample_count_ == 0)
  {
    minimum_us_ = duration_us;
    maximum_us_ = duration_us;
  }
  else
  {
    if (duration_us < minimum_us_)
    {
      minimum_us_ = duration_us;
    }
    if (duration_us > maximum_us_)
    {
      maximum_us_ = duration_us;
    }
  }

  ++sample_count_;
  total_us_ += duration_us;
}

inline DurationStatistics::Summary DurationStatistics::GetSummary() const noexcept
{
  LibXR::Mutex::LockGuard lock(mutex_);
  if (sample_count_ == 0)
  {
    return {};
  }

  return Summary{sample_count_, total_us_ / sample_count_, minimum_us_, maximum_us_};
}

}  // namespace XRobot
