#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <thread>

#include "DurationStatistics.hpp"
#include "libxr.hpp"

namespace
{
using namespace std::chrono_literals;

/// 条件不成立就打印并以 1 退出 / Prints and exits with 1 when the condition fails.
void Expect(bool condition, const char* message)
{
  if (!condition)
  {
    std::fprintf(stderr, "FAIL: %s\n", message);
    std::exit(1);
  }
}

/// 记一个至少持续 duration 的样本 / Records one sample lasting at least `duration`.
void Measure(XRobot::DurationStatistics& statistics, std::chrono::milliseconds duration)
{
  auto measurement = statistics.Measure();
  std::this_thread::sleep_for(duration);
}

void TestSummary()
{
  XRobot::DurationStatistics statistics;
  XRobot::DurationStatistics::Summary empty = statistics.GetSummary();
  Expect(empty.sample_count == 0 && empty.average_us == 0 && empty.minimum_us == 0 &&
             empty.maximum_us == 0,
         "no sample: all fields are zero");
  Measure(statistics, 2ms);
  Measure(statistics, 4ms);
  const XRobot::DurationStatistics::Summary s = statistics.GetSummary();
  Expect(s.sample_count == 2, "two samples");
  Expect(s.minimum_us >= 2000 && s.maximum_us >= 4000 && s.minimum_us <= s.maximum_us,
         "minimum and maximum follow the samples");
  Expect(s.average_us >= s.minimum_us && s.average_us <= s.maximum_us,
         "the average lies between them");
  Expect(statistics.GetSummary().sample_count == 2, "GetSummary does not clear");
}

/// GetSummaryAndReset 返回累计并清零：第二个周期里看不到第一个周期的尖峰。
/// GetSummaryAndReset returns the accumulation and clears it: the second period shows
/// nothing of the first period's spike.
void TestSummaryAndReset()
{
  XRobot::DurationStatistics statistics;
  Measure(statistics, 30ms);  // 第一个周期的尖峰 / the first period's spike
  Measure(statistics, 2ms);
  const XRobot::DurationStatistics::Summary first = statistics.GetSummaryAndReset();
  Expect(first.sample_count == 2, "the first period has both samples");
  Expect(first.maximum_us >= 30000, "its maximum holds the spike");

  const XRobot::DurationStatistics::Summary cleared = statistics.GetSummary();
  Expect(cleared.sample_count == 0 && cleared.maximum_us == 0, "cleared by the read");

  Measure(statistics, 2ms);
  Measure(statistics, 3ms);
  const XRobot::DurationStatistics::Summary second = statistics.GetSummaryAndReset();
  Expect(second.sample_count == 2, "the second period has its own two samples");
  Expect(second.maximum_us < 25000, "the spike of the first period is gone");
  Expect(second.minimum_us >= 2000, "the minimum follows the new samples");

  const XRobot::DurationStatistics::Summary none = statistics.GetSummaryAndReset();
  Expect(none.sample_count == 0 && none.average_us == 0 && none.minimum_us == 0 &&
             none.maximum_us == 0,
         "a period without samples reads as zero");
}
}  // namespace

int main()
{
  LibXR::PlatformInit();
  TestSummary();
  TestSummaryAndReset();
  std::puts("duration_statistics_test passed");
  return 0;
}
