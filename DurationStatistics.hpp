#pragma once

#include <cstdint>

#include "mutex.hpp"
#include "timebase.hpp"

namespace XRobot
{

/**
 * @brief 记录作用域耗时的累计统计 / Accumulated statistics for scoped durations
 *
 * 该类只负责采集统计数据，不负责输出。将 `Measure()` 返回的对象留在待测
 * 作用域内，其析构函数会自动提交一次耗时。业务线程可以并发记录，监控线程
 * 可以通过 `GetSummary()` 读取一致快照。
 *
 * This class only collects statistics and does not perform any output. Keep the
 * object returned by `Measure()` in the measured scope; its destructor submits
 * one duration. Worker threads may record concurrently, while a monitor thread
 * may read a consistent snapshot through `GetSummary()`.
 *
 * @note 记录和读取使用 LibXR 互斥锁，只能从任务上下文调用，不可从中断服务
 *       程序调用。 / Recording and reading use a LibXR mutex and must be called
 *       from task context, not from an interrupt service routine.
 */
class DurationStatistics
{
 public:
  /**
   * @brief 统计快照 / Statistics snapshot
   */
  struct Summary
  {
    /** @brief 已记录的样本数 / Number of recorded samples */
    uint64_t sample_count = 0;

    /** @brief 平均耗时，单位为微秒 / Average duration in microseconds */
    uint64_t average_us = 0;

    /** @brief 最小耗时，单位为微秒 / Minimum duration in microseconds */
    uint64_t minimum_us = 0;

    /** @brief 最大耗时，单位为微秒 / Maximum duration in microseconds */
    uint64_t maximum_us = 0;
  };

  /**
   * @brief 一个作用域测量 / One scoped measurement
   *
   * 该对象不可复制或移动，确保一个待测作用域只提交一次样本。统计器必须比
   * 其创建的测量对象存活更久。
   *
   * The object is neither copyable nor movable, ensuring that one measured
   * scope submits exactly one sample. Its statistics collector must outlive it.
   */
  class ScopedMeasurement
  {
   public:
    /**
     * @brief 禁止复制和移动 / Disable copy and move operations
     *
     * 一个作用域测量对象只能由 `Measure()` 创建并析构一次。
     * A scoped measurement is created and destroyed exactly once through
     * `Measure()`.
     */
    ScopedMeasurement(const ScopedMeasurement&) = delete;
    ScopedMeasurement& operator=(const ScopedMeasurement&) = delete;
    ScopedMeasurement(ScopedMeasurement&&) = delete;
    ScopedMeasurement& operator=(ScopedMeasurement&&) = delete;

    /**
     * @brief 结束测量并提交耗时 / Finish and submit the measurement
     */
    ~ScopedMeasurement() noexcept;

   private:
    friend class DurationStatistics;

    /**
     * @brief 创建作用域测量对象 / Construct a scoped measurement
     * @param owner 接收测量结果的统计器 / Collector receiving the result
     * @param start 待测作用域的开始时间 / Start time of the measured scope
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
   * @brief 构造空的耗时统计器 / Construct an empty duration statistics
   * collector
   */
  DurationStatistics() = default;

  /**
   * @brief 禁止复制和移动 / Disable copy and move operations
   *
   * 统计器拥有同步原语及累计状态，不能复制或移动。
   * The collector owns synchronization state and accumulated values, so it
   * cannot be copied or moved.
   */
  DurationStatistics(const DurationStatistics&) = delete;
  DurationStatistics& operator=(const DurationStatistics&) = delete;
  DurationStatistics(DurationStatistics&&) = delete;
  DurationStatistics& operator=(DurationStatistics&&) = delete;

  /**
   * @brief 开始一次作用域测量 / Start one scoped measurement
   * @return 测量对象，其析构会自动记录耗时。
   *         A measurement whose destructor records the elapsed duration.
   */
  [[nodiscard]] ScopedMeasurement Measure() noexcept
  {
    return ScopedMeasurement(*this, LibXR::Timebase::GetMicroseconds());
  }

  /**
   * @brief 读取统计快照 / Read a statistics snapshot
   * @return 次数、平均值、最小值和最大值，单位均为微秒。
   *         Sample count, average, minimum, and maximum, all in microseconds.
   *
   * @note 平均值采用整数除法；没有样本时四个字段均为 `0`。
   *       The average uses integer division; all four fields are `0` when no
   *       sample has been recorded.
   */
  [[nodiscard]] Summary GetSummary() const noexcept;

 private:
  friend class ScopedMeasurement;

  /**
   * @brief 提交一次已结束的作用域测量 / Submit one finished scoped measurement
   * @param start 待测作用域的开始时间 / Start time of the measured scope
   *
   * 结束时间在获取互斥锁之前读取，因此锁等待不会计入被测作用域。
   * The end timestamp is read before acquiring the mutex, so lock contention is
   * excluded from the measured duration.
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
