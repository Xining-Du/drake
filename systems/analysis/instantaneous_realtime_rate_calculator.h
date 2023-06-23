#pragma once

#include <memory>
#include <optional>

#include "drake/common/timer.h"

namespace drake {
namespace systems {
namespace internal {
/* Utility class that computes the realtime rate achieved between time steps. */
class InstantaneousRealtimeRateCalculator {
 public:
  DRAKE_NO_COPY_NO_MOVE_NO_ASSIGN(InstantaneousRealtimeRateCalculator);
  InstantaneousRealtimeRateCalculator() = default;

  /* Computes the realtime rate which is the ratio of the amount of simulator
   time to real world time that has passed between invocations.
   The very first call to this function seeds the rate calculation and returns
   nullopt because a valid rate cannot be computed yet. It will also return
   nullopt if sim_time goes backwards.
    @param current_sim_time the current simulated time.
    @return realtime rate if one can be calculated, nullopt otherwise.
   */
  std::optional<double> UpdateAndRecalculate(double current_sim_time);

  /* If the simulator was paused, prev_sim_time_ should be set to nullopt
   * so that the timer is not used in the calculation of realtime rate on
   * the first call after the simulator is resumed. Otherwise the realtime
   * rate value would be inaccurate since the timer was not paused.
   */
  void Reset() { prev_sim_time_ = std::nullopt; }

  /* (Internal use for unit testing only) Used to mock the monotonic wall time
     source to control time during unit testing.  */
#ifndef DRAKE_DOXYGEN_CXX
  void InjectMockTimer(std::unique_ptr<Timer>);
#endif

 private:
  std::optional<double> prev_sim_time_;
  std::unique_ptr<Timer> timer_{std::make_unique<SteadyTimer>()};
};
}  // namespace internal
}  // namespace systems
}  // namespace drake
