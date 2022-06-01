#pragma once

#include <functional>
#include <iostream>
#include <tuple>
#include <utility>

#include "drake/common/drake_throw.h"
#include "drake/common/text_logging.h"

namespace drake {
namespace multibody {
namespace contact_solvers {
namespace internal {

/*
  Uses a Newton-Raphson method to compute a root of `function` within the
  bracket [x_lower, x_upper]. This method stops when the difference between the
  previous iterate xᵏ and the next iteration xᵏ⁺¹ is below the absolute
  tolerance `abs_tolerance`, i.e. when |xᵏ⁺¹ - xᵏ| < abs_tolerance.

  This method expect that sign(function(x_lower)) != sign(function_x_upper). For
  continuous functions, this ensures there exists a root in [x_lower, x_upper].

  This method iteratively shrinks the bracket containing the root. Moreover, it
  switches to bisection whenever a Newton iterate falls outside the bracket or
  when Newton's method is slow. Using this procedure, this method is guaranteed
  to find a root (which might be non-unique) within [x_lower, x_upper], with
  accuracy given by `abs_tol`.

  @pre x_lower <= x_upper
  @pre x_guess is in [x_lower, x_upper]
  @pre sign(function(x_lower)) != sign(function_x_upper)
  @pre abs_tolerance > 0
  @pre max_iterations > 0
*/
std::pair<double, int> DoNewtonWithBisectionFallback(
    const std::function<std::pair<double, double>(double)>& function,
    double x_lower, double x_upper, double x_guess, double abs_tolerance,
    int max_iterations) {
  using std::abs;
  using std::swap;
  // Pre-conditions on the bracket.
  DRAKE_THROW_UNLESS(x_lower <= x_guess && x_guess <= x_upper);

  // Pre-conditions on the algorithm's parameters.
  DRAKE_THROW_UNLESS(abs_tolerance > 0);
  DRAKE_THROW_UNLESS(max_iterations > 0);

  // These checks verify there is an appropriate bracket around the root,
  // though at the expense of additional evaluations.
  // TODO(amcastro-tri): Consider removing this extra evaluation whenever the
  // users does know that f_lower * f_upper < 0.
  auto [f_lower, df_lower] = function(x_lower);  // First evaluation.
  if (f_lower == 0) return std::make_pair(x_lower, 1);

  auto [f_upper, df_upper] = function(x_upper);
  if (f_upper == 0) return std::make_pair(x_upper, 2);  // Second evaluation.

  // Verify there is a root inside the bracket. Notice that f_lower * f_upper !=
  // 0 since the case f_lower == 0 || f_upper == 0 has been ruled out above.
  DRAKE_THROW_UNLESS(f_lower * f_upper < 0);

  double root = x_guess;  // Initialize to user supplied guess.
  double minus_dx = (x_lower - x_upper);
  auto [f, df] = function(root);               // Third evaluation.
  if (f == 0) return std::make_pair(root, 3);  // Third evaluation.

  // Helper to perform a bisection update. It returns the pair (root, -dx).
  auto do_bisection = [&x_upper, &x_lower]() {
    const double dx_negative = 0.5 * (x_lower - x_upper);
    // N.B. This way of updating the root will lead to root == x_lower if
    // the value of minus_dx is insignificant compared to x_lower when using
    // floating point precision. This fact is used in the termination check
    // below to exit whenever a user specifies abs_tolerance = 0.
    const double x = x_lower - dx_negative;
    return std::make_pair(x, dx_negative);
  };

  // Helper to perform a Newton update. It returns the pair (root, -dx).
  auto do_newton = [&f, &df, &root]() {
    const double dx_negative = f / df;
    double x = root;
    // N.B. x will not change if dx_negative is negligible within machine
    // precision.
    x -= dx_negative;
    return std::make_pair(x, dx_negative);
  };

  for (int num_evaluations = 3; num_evaluations <= max_iterations;
       ++num_evaluations) {
    // N.B. Notice this check is always true for df = 0 (and f != 0 since we
    // ruled that case out above). Therefore Newton is only called when df != 0,
    // and the search direction is well defined.
    const bool newton_is_slow = 2.0 * abs(f) > abs(minus_dx * df);

    if (newton_is_slow) {
      std::tie(root, minus_dx) = do_bisection();
      DRAKE_LOGGER_DEBUG("Bisect. k = {:d}.", num_evaluations);
    } else {
      std::tie(root, minus_dx) = do_newton();
      const bool outside_bracket = root < x_lower || root > x_upper;
      if (outside_bracket) {
        std::tie(root, minus_dx) = do_bisection();
        DRAKE_LOGGER_DEBUG("Bisect. k = {:d}.", num_evaluations);
      } else {
        DRAKE_LOGGER_DEBUG("Newton. k = {:d}.", num_evaluations);
      }
    }

    DRAKE_LOGGER_DEBUG(
        "x = {:10.4g}. [x_lower, x_upper] = [{:10.4g}, "
        "{:10.4g}]. dx = {:10.4g}. f = {:10.4g}. dfdx = {:10.4g}.",
        root, x_lower, x_upper, -minus_dx, f, df);

    if (abs(minus_dx) < abs_tolerance)
      return std::make_pair(root, num_evaluations);

    // The one evaluation per iteration.
    std::tie(f, df) = function(root);
    if (f == 0) return std::make_pair(root, num_evaluations);

    // Update the bracket around root to guarantee that there exist a root
    // within the interval [x_lower, x_upper].
    if (f * f_upper < 0.0) {
      x_lower = root;
      f_lower = f;
    } else {
      x_upper = root;
      f_upper = f;
    }
  }

  // If here, then DoNewtonWithBisectionFallback did not converge.
  // This will happen for instance when the maximum number of iterations is too
  // small.
  throw std::runtime_error(
      fmt::format("NewtonWithBisectionFallback did not converge.\n"
                  "|x - x_prev| = {}. |x_upper-x_lower| = {}",
                  abs(minus_dx), abs(x_upper - x_lower)));
};

}  // namespace internal
}  // namespace contact_solvers
}  // namespace multibody
}  // namespace drake
