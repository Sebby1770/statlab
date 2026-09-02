#include "stats_core.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void check(bool condition, const char *expr, const char *file, int line) {
  if (!condition) {
    std::cerr << "CHECK failed: " << expr << " at " << file << ":" << line
              << '\n';
    ++failures;
  }
}

/* NDEBUG-safe: always evaluates expr (unlike assert). */
#define CHECK(expr) check(static_cast<bool>(expr), #expr, __FILE__, __LINE__)

bool close_to(double actual, double expected, double tol = 0.000001) {
  if (std::isnan(expected)) {
    return std::isnan(actual);
  }
  return std::fabs(actual - expected) < tol;
}

} // namespace

int main() {
  /* Classic dataset used across README / docs. */
  const double values[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
  StatsSummary summary{};

  CHECK(stats_summary(values, 8, &summary) == STATS_OK);
  CHECK(summary.count == 8);
  CHECK(close_to(summary.sum, 40.0));
  CHECK(close_to(summary.min, 2.0));
  CHECK(close_to(summary.max, 9.0));
  CHECK(close_to(summary.range, 7.0));
  CHECK(close_to(summary.mean, 5.0));
  CHECK(close_to(summary.median, 4.5));
  CHECK(close_to(summary.q1, 4.0));  /* p25 */
  CHECK(close_to(summary.q3, 5.5));  /* p75 */
  CHECK(close_to(summary.iqr, 1.5));
  CHECK(close_to(summary.mad, 0.5));
  CHECK(close_to(summary.mode, 4.0));
  CHECK(close_to(summary.p10, 3.4));
  CHECK(close_to(summary.p90, 7.6));
  CHECK(close_to(summary.variance, 4.0));
  CHECK(close_to(summary.stddev, 2.0));
  CHECK(close_to(summary.sample_variance, 4.5714285714));
  CHECK(close_to(summary.sample_stddev, 2.1380899353));
  CHECK(close_to(summary.cv, 0.4));
  CHECK(close_to(stats_sum(values, 8), 40.0));

  /* Tukey fences: q1 - 1.5*iqr = 4 - 2.25 = 1.75; q3 + 1.5*iqr = 7.75 */
  CHECK(close_to(summary.fence_low, 1.75));
  CHECK(close_to(summary.fence_high, 7.75));
  /* 9.0 is above fence_high → one outlier */
  CHECK(summary.outlier_count == 1);

  /* Bias-corrected sample skewness G1 ≈ 0.8184875534 */
  CHECK(close_to(summary.skewness, 0.8184875534, 0.00001));

  /* Bias-corrected excess kurtosis ≈ 0.940625 */
  CHECK(close_to(summary.kurtosis, 0.940625, 0.00001));

  /* Single-value edge cases. */
  const double single[] = {42.0};
  CHECK(stats_summary(single, 1, &summary) == STATS_OK);
  CHECK(close_to(summary.median, 42.0));
  CHECK(close_to(summary.q1, 42.0));
  CHECK(close_to(summary.q3, 42.0));
  CHECK(close_to(summary.iqr, 0.0));
  CHECK(close_to(summary.mad, 0.0));
  CHECK(std::isnan(summary.mode)); /* all unique */
  CHECK(close_to(summary.p10, 42.0));
  CHECK(close_to(summary.p90, 42.0));
  CHECK(close_to(summary.sample_variance, 0.0));
  CHECK(close_to(summary.sample_stddev, 0.0));
  CHECK(std::isnan(summary.skewness));
  CHECK(std::isnan(summary.kurtosis));
  CHECK(close_to(summary.cv, 0.0)); /* stddev 0 / mean 42 */
  CHECK(close_to(summary.fence_low, 42.0));
  CHECK(close_to(summary.fence_high, 42.0));
  CHECK(summary.outlier_count == 0);

  /* Mean zero → cv is NaN. */
  const double centered[] = {-1.0, 0.0, 1.0};
  CHECK(stats_summary(centered, 3, &summary) == STATS_OK);
  CHECK(close_to(summary.mean, 0.0));
  CHECK(std::isnan(summary.cv));
  CHECK(!std::isnan(summary.skewness)); /* n >= 3 */
  CHECK(std::isnan(summary.kurtosis));  /* n < 4 */

  /* All unique → mode is NaN. */
  const double unique[] = {1.0, 2.0, 3.0, 4.0};
  CHECK(stats_summary(unique, 4, &summary) == STATS_OK);
  CHECK(std::isnan(summary.mode));

  /* Multimodal: first mode after sort wins. */
  const double multi[] = {1.0, 1.0, 2.0, 2.0, 3.0};
  CHECK(stats_summary(multi, 5, &summary) == STATS_OK);
  CHECK(close_to(summary.mode, 1.0));

  /* Null / empty errors. */
  CHECK(stats_summary(nullptr, 1, &summary) == STATS_ERR_NULL);
  CHECK(stats_summary(values, 8, nullptr) == STATS_ERR_NULL);
  CHECK(stats_summary(values, 0, &summary) == STATS_ERR_EMPTY);
  CHECK(stats_status_message(STATS_OK) == std::string("ok"));
  CHECK(stats_status_message(STATS_ERR_EMPTY) == std::string("empty input"));
  CHECK(stats_status_message(STATS_ERR_INVALID) ==
        std::string("invalid argument"));

  /* stats_percentile */
  double p = 0.0;
  const double sorted[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
  CHECK(stats_percentile(sorted, 8, 0.5, &p) == STATS_OK);
  CHECK(close_to(p, 4.5));
  CHECK(stats_percentile(sorted, 8, 0.0, &p) == STATS_OK);
  CHECK(close_to(p, 2.0));
  CHECK(stats_percentile(sorted, 8, 1.0, &p) == STATS_OK);
  CHECK(close_to(p, 9.0));
  CHECK(stats_percentile(sorted, 8, 0.10, &p) == STATS_OK);
  CHECK(close_to(p, 3.4));
  CHECK(stats_percentile(sorted, 8, 0.25, &p) == STATS_OK);
  CHECK(close_to(p, 4.0)); /* p25 == q1 */
  CHECK(stats_percentile(sorted, 8, 0.75, &p) == STATS_OK);
  CHECK(close_to(p, 5.5)); /* p75 == q3 */
  CHECK(stats_percentile(nullptr, 8, 0.5, &p) == STATS_ERR_NULL);
  CHECK(stats_percentile(sorted, 0, 0.5, &p) == STATS_ERR_EMPTY);
  CHECK(stats_percentile(sorted, 8, -0.1, &p) == STATS_ERR_INVALID);
  CHECK(stats_percentile(sorted, 8, 1.1, &p) == STATS_ERR_INVALID);

  /* stats_histogram */
  size_t counts[4] = {0, 0, 0, 0};
  double hmin = 0.0;
  double hmax = 0.0;
  CHECK(stats_histogram(values, 8, 4, counts, &hmin, &hmax) == STATS_OK);
  CHECK(close_to(hmin, 2.0));
  CHECK(close_to(hmax, 9.0));
  size_t total = 0;
  for (size_t i = 0; i < 4; ++i) {
    total += counts[i];
  }
  CHECK(total == 8);

  /* All equal → every observation in bin 0. */
  const double same[] = {3.0, 3.0, 3.0};
  size_t counts2[3] = {9, 9, 9};
  CHECK(stats_histogram(same, 3, 3, counts2, &hmin, &hmax) == STATS_OK);
  CHECK(counts2[0] == 3);
  CHECK(counts2[1] == 0);
  CHECK(counts2[2] == 0);
  CHECK(close_to(hmin, 3.0));
  CHECK(close_to(hmax, 3.0));

  /* Histogram error paths. */
  CHECK(stats_histogram(nullptr, 1, 2, counts, nullptr, nullptr) ==
        STATS_ERR_NULL);
  CHECK(stats_histogram(values, 0, 2, counts, nullptr, nullptr) ==
        STATS_ERR_EMPTY);
  CHECK(stats_histogram(values, 8, 0, counts, nullptr, nullptr) ==
        STATS_ERR_INVALID);
  CHECK(stats_histogram(values, 8, 4, nullptr, nullptr, nullptr) ==
        STATS_ERR_NULL);

  /* ---------- v1.3: Pearson correlation ---------- */
  {
    const double x[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    const double y_pos[] = {2.0, 4.0, 6.0, 8.0, 10.0};
    const double y_neg[] = {5.0, 4.0, 3.0, 2.0, 1.0};
    double r = 0.0;

    CHECK(stats_correlation(x, y_pos, 5, &r) == STATS_OK);
    CHECK(close_to(r, 1.0));

    CHECK(stats_correlation(x, y_neg, 5, &r) == STATS_OK);
    CHECK(close_to(r, -1.0));

    /* Known pair: r = 10 / sqrt(10 * 14.8) ≈ 0.8219949365 */
    const double xa[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    const double ya[] = {2.0, 1.0, 4.0, 3.0, 6.0};
    CHECK(stats_correlation(xa, ya, 5, &r) == STATS_OK);
    CHECK(close_to(r, 0.8219949365, 0.00001));

    CHECK(stats_correlation(nullptr, y_pos, 5, &r) == STATS_ERR_NULL);
    CHECK(stats_correlation(x, nullptr, 5, &r) == STATS_ERR_NULL);
    CHECK(stats_correlation(x, y_pos, 5, nullptr) == STATS_ERR_NULL);
    CHECK(stats_correlation(x, y_pos, 0, &r) == STATS_ERR_EMPTY);
    CHECK(stats_correlation(x, y_pos, 1, &r) == STATS_ERR_INVALID);

    const double constant[] = {3.0, 3.0, 3.0};
    CHECK(stats_correlation(x, constant, 3, &r) == STATS_ERR_INVALID);
  }

  /* ---------- v1.3: linear regression (perfect line R² ≈ 1) ---------- */
  {
    /* y = 2x + 1 */
    const double x[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    const double y[] = {3.0, 5.0, 7.0, 9.0, 11.0};
    double slope = 0.0;
    double intercept = 0.0;
    double r2 = 0.0;

    CHECK(stats_linreg(x, y, 5, &slope, &intercept, &r2) == STATS_OK);
    CHECK(close_to(slope, 2.0));
    CHECK(close_to(intercept, 1.0));
    CHECK(close_to(r2, 1.0));

    /* y = -0.5x + 10 */
    const double y2[] = {9.5, 9.0, 8.5, 8.0, 7.5};
    CHECK(stats_linreg(x, y2, 5, &slope, &intercept, &r2) == STATS_OK);
    CHECK(close_to(slope, -0.5));
    CHECK(close_to(intercept, 10.0));
    CHECK(close_to(r2, 1.0));

    CHECK(stats_linreg(nullptr, y, 5, &slope, &intercept, &r2) ==
          STATS_ERR_NULL);
    CHECK(stats_linreg(x, y, 5, nullptr, &intercept, &r2) == STATS_ERR_NULL);
    CHECK(stats_linreg(x, y, 5, &slope, nullptr, &r2) == STATS_ERR_NULL);
    CHECK(stats_linreg(x, y, 0, &slope, &intercept, &r2) == STATS_ERR_EMPTY);
    CHECK(stats_linreg(x, y, 1, &slope, &intercept, &r2) == STATS_ERR_INVALID);

    const double x_const[] = {2.0, 2.0, 2.0};
    const double y_any[] = {1.0, 2.0, 3.0};
    CHECK(stats_linreg(x_const, y_any, 3, &slope, &intercept, &r2) ==
          STATS_ERR_INVALID);

    /* r2 may be NULL */
    CHECK(stats_linreg(x, y, 5, &slope, &intercept, nullptr) == STATS_OK);
  }

  /* ---------- v1.3: trimmed mean ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 100.0};
    double tm = 0.0;

    /* trim 0.1 → k = floor(10*0.1)=1 → drop 1 and 100 → mean of 2..9 = 5.5 */
    CHECK(stats_trimmed_mean(data, 10, 0.1, &tm) == STATS_OK);
    CHECK(close_to(tm, 5.5));

    /* trim 0 → ordinary mean */
    CHECK(stats_trimmed_mean(data, 10, 0.0, &tm) == STATS_OK);
    CHECK(close_to(tm, 14.5));

    /* classic set, trim 0 → mean 5 */
    CHECK(stats_trimmed_mean(values, 8, 0.0, &tm) == STATS_OK);
    CHECK(close_to(tm, 5.0));

    CHECK(stats_trimmed_mean(nullptr, 5, 0.1, &tm) == STATS_ERR_NULL);
    CHECK(stats_trimmed_mean(data, 10, 0.1, nullptr) == STATS_ERR_NULL);
    CHECK(stats_trimmed_mean(data, 0, 0.1, &tm) == STATS_ERR_EMPTY);
    CHECK(stats_trimmed_mean(data, 10, 0.5, &tm) == STATS_ERR_INVALID);
    CHECK(stats_trimmed_mean(data, 10, -0.1, &tm) == STATS_ERR_INVALID);
    CHECK(stats_trimmed_mean(data, 10, 1.0, &tm) == STATS_ERR_INVALID);
  }

  /* ---------- v1.3: z-score ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0};
    double z[3] = {0.0, 0.0, 0.0};
    /* mean=2, pop var=2/3, stddev=sqrt(2/3) */
    const double s = std::sqrt(2.0 / 3.0);
    CHECK(stats_zscore(data, 3, z) == STATS_OK);
    CHECK(close_to(z[0], (1.0 - 2.0) / s));
    CHECK(close_to(z[1], 0.0));
    CHECK(close_to(z[2], (3.0 - 2.0) / s));

    const double same_z[] = {5.0, 5.0, 5.0};
    CHECK(stats_zscore(same_z, 3, z) == STATS_OK);
    CHECK(close_to(z[0], 0.0));
    CHECK(close_to(z[1], 0.0));
    CHECK(close_to(z[2], 0.0));

    CHECK(stats_zscore(nullptr, 3, z) == STATS_ERR_NULL);
    CHECK(stats_zscore(data, 3, nullptr) == STATS_ERR_NULL);
    CHECK(stats_zscore(data, 0, z) == STATS_ERR_EMPTY);
  }

  /* ---------- v1.3: Tukey fences / outliers ---------- */
  {
    const double with_out[] = {1.0, 2.0, 3.0, 4.0, 5.0, 100.0};
    double flo = 0.0;
    double fhi = 0.0;
    size_t n_out = 0;
    CHECK(stats_tukey_fences(with_out, 6, &flo, &fhi, &n_out) == STATS_OK);
    CHECK(n_out >= 1);

    CHECK(stats_summary(with_out, 6, &summary) == STATS_OK);
    CHECK(summary.outlier_count == n_out);
    CHECK(close_to(summary.fence_low, flo));
    CHECK(close_to(summary.fence_high, fhi));
    /* 100 should be outside upper fence */
    CHECK(100.0 > summary.fence_high);

    CHECK(stats_tukey_fences(nullptr, 3, &flo, &fhi, &n_out) ==
          STATS_ERR_NULL);
    CHECK(stats_tukey_fences(with_out, 0, &flo, &fhi, &n_out) ==
          STATS_ERR_EMPTY);

    /* All pointers optional except needing valid values */
    CHECK(stats_tukey_fences(with_out, 6, nullptr, nullptr, nullptr) ==
          STATS_OK);
  }

  /* ---------- v1.4: Spearman rank correlation ---------- */
  {
    const double x[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    const double y_mono[] = {10.0, 20.0, 30.0, 40.0, 50.0};
    const double y_inv[] = {50.0, 40.0, 30.0, 20.0, 10.0};
    double rho = 0.0;

    CHECK(stats_spearman(x, y_mono, 5, &rho) == STATS_OK);
    CHECK(close_to(rho, 1.0));

    CHECK(stats_spearman(x, y_inv, 5, &rho) == STATS_OK);
    CHECK(close_to(rho, -1.0));

    /* Nonlinear monotone: ranks still perfect */
    const double y_sq[] = {1.0, 4.0, 9.0, 16.0, 25.0};
    CHECK(stats_spearman(x, y_sq, 5, &rho) == STATS_OK);
    CHECK(close_to(rho, 1.0));

    /* Ties: average ranks */
    const double xt[] = {1.0, 1.0, 2.0, 3.0};
    const double yt[] = {1.0, 2.0, 2.0, 3.0};
    CHECK(stats_spearman(xt, yt, 4, &rho) == STATS_OK);
    CHECK(std::isfinite(rho));
    CHECK(rho > 0.0);

    CHECK(stats_spearman(nullptr, y_mono, 5, &rho) == STATS_ERR_NULL);
    CHECK(stats_spearman(x, nullptr, 5, &rho) == STATS_ERR_NULL);
    CHECK(stats_spearman(x, y_mono, 5, nullptr) == STATS_ERR_NULL);
    CHECK(stats_spearman(x, y_mono, 0, &rho) == STATS_ERR_EMPTY);
    CHECK(stats_spearman(x, y_mono, 1, &rho) == STATS_ERR_INVALID);
  }

  /* ---------- v1.4: covariance ---------- */
  {
    const double x[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    const double y[] = {2.0, 4.0, 6.0, 8.0, 10.0};
    double c = 0.0;

    /* Perfect linear: sample cov of x with 2x.
       mean_x=3, mean_y=6, ss_xy = sum((x-3)(y-6)) = 2*ss_xx = 2*10 = 20
       sample cov = 20/4 = 5; pop cov = 20/5 = 4 */
    CHECK(stats_covariance(x, y, 5, 1, &c) == STATS_OK);
    CHECK(close_to(c, 5.0));
    CHECK(stats_covariance(x, y, 5, 0, &c) == STATS_OK);
    CHECK(close_to(c, 4.0));

    CHECK(stats_covariance(nullptr, y, 5, 1, &c) == STATS_ERR_NULL);
    CHECK(stats_covariance(x, y, 5, 1, nullptr) == STATS_ERR_NULL);
    CHECK(stats_covariance(x, y, 0, 1, &c) == STATS_ERR_EMPTY);
    CHECK(stats_covariance(x, y, 1, 1, &c) == STATS_ERR_INVALID);
    /* population allows n=1 */
    CHECK(stats_covariance(x, y, 1, 0, &c) == STATS_OK);
    CHECK(close_to(c, 0.0));
  }

  /* ---------- v1.4: moving average ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double ma[5] = {0};
    /* window 3 → length 3: (1+2+3)/3, (2+3+4)/3, (3+4+5)/3 */
    CHECK(stats_moving_average(data, 5, 3, ma) == STATS_OK);
    CHECK(close_to(ma[0], 2.0));
    CHECK(close_to(ma[1], 3.0));
    CHECK(close_to(ma[2], 4.0));

    /* window 1 → identity */
    CHECK(stats_moving_average(data, 5, 1, ma) == STATS_OK);
    CHECK(close_to(ma[0], 1.0));
    CHECK(close_to(ma[4], 5.0));

    /* window == n → single overall mean */
    CHECK(stats_moving_average(data, 5, 5, ma) == STATS_OK);
    CHECK(close_to(ma[0], 3.0));

    CHECK(stats_moving_average(nullptr, 5, 2, ma) == STATS_ERR_NULL);
    CHECK(stats_moving_average(data, 5, 2, nullptr) == STATS_ERR_NULL);
    CHECK(stats_moving_average(data, 0, 1, ma) == STATS_ERR_EMPTY);
    CHECK(stats_moving_average(data, 5, 0, ma) == STATS_ERR_INVALID);
    CHECK(stats_moving_average(data, 5, 6, ma) == STATS_ERR_INVALID);
  }

  /* ---------- v1.4: EMA ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0};
    double e[3] = {0};
    /* alpha=1 → out == values */
    CHECK(stats_ema(data, 3, 1.0, e) == STATS_OK);
    CHECK(close_to(e[0], 1.0));
    CHECK(close_to(e[1], 2.0));
    CHECK(close_to(e[2], 3.0));

    /* alpha=0.5: e0=1; e1=0.5*2+0.5*1=1.5; e2=0.5*3+0.5*1.5=2.25 */
    CHECK(stats_ema(data, 3, 0.5, e) == STATS_OK);
    CHECK(close_to(e[0], 1.0));
    CHECK(close_to(e[1], 1.5));
    CHECK(close_to(e[2], 2.25));

    CHECK(stats_ema(nullptr, 3, 0.5, e) == STATS_ERR_NULL);
    CHECK(stats_ema(data, 3, 0.5, nullptr) == STATS_ERR_NULL);
    CHECK(stats_ema(data, 0, 0.5, e) == STATS_ERR_EMPTY);
    CHECK(stats_ema(data, 3, 0.0, e) == STATS_ERR_INVALID);
    CHECK(stats_ema(data, 3, 1.5, e) == STATS_ERR_INVALID);
    CHECK(stats_ema(data, 3, -0.1, e) == STATS_ERR_INVALID);
  }

  /* ---------- v1.4: geometric / harmonic mean ---------- */
  {
    const double data[] = {1.0, 3.0, 9.0};
    double g = 0.0;
    double h = 0.0;
    /* geo: exp((ln1+ln3+ln9)/3) = exp(ln3) = 3 */
    CHECK(stats_geometric_mean(data, 3, &g) == STATS_OK);
    CHECK(close_to(g, 3.0));
    /* harmonic: 3 / (1 + 1/3 + 1/9) = 3 / (13/9) = 27/13 ≈ 2.076923 */
    CHECK(stats_harmonic_mean(data, 3, &h) == STATS_OK);
    CHECK(close_to(h, 27.0 / 13.0));

    const double bad[] = {1.0, 0.0, 2.0};
    CHECK(stats_geometric_mean(bad, 3, &g) == STATS_ERR_INVALID);
    CHECK(stats_harmonic_mean(bad, 3, &h) == STATS_ERR_INVALID);

    const double neg[] = {1.0, -2.0, 3.0};
    CHECK(stats_geometric_mean(neg, 3, &g) == STATS_ERR_INVALID);

    CHECK(stats_geometric_mean(nullptr, 3, &g) == STATS_ERR_NULL);
    CHECK(stats_geometric_mean(data, 0, &g) == STATS_ERR_EMPTY);
    CHECK(stats_harmonic_mean(data, 3, nullptr) == STATS_ERR_NULL);
  }

  /* ---------- v1.4: mean CI (Student-t) ---------- */
  {
    const double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    double mean = 0.0;
    double lo = 0.0;
    double hi = 0.0;
    CHECK(stats_mean_ci(data, 8, &mean, &lo, &hi) == STATS_OK);
    CHECK(close_to(mean, 5.0));
    /* sigma is estimated from the data, so the interval is Student-t with
       n-1 = 7 df. t(0.975, 7) = 2.364624252 (published table: 2.365). */
    const double s = std::sqrt(32.0 / 7.0); /* sample sd for mean 5 */
    const double half = 2.364624252 * s / std::sqrt(8.0);
    CHECK(close_to(lo, 5.0 - half, 0.0001));
    CHECK(close_to(hi, 5.0 + half, 0.0001));
    CHECK(hi > lo);
    /* The t interval must be strictly wider than the old normal one. */
    CHECK(hi - lo > 2.0 * 1.96 * s / std::sqrt(8.0));

    /* Critical values against a published two-sided 95% t table. */
    CHECK(close_to(stats_t_critical(0.95, 1.0), 12.706, 0.001));
    CHECK(close_to(stats_t_critical(0.95, 2.0), 4.303, 0.001));
    CHECK(close_to(stats_t_critical(0.95, 5.0), 2.571, 0.001));
    CHECK(close_to(stats_t_critical(0.95, 10.0), 2.228, 0.001));
    CHECK(close_to(stats_t_critical(0.95, 30.0), 2.042, 0.001));
    CHECK(close_to(stats_t_critical(0.95, 100.0), 1.984, 0.001));
    /* Converges to the normal quantile as df grows, and is monotone in df. */
    CHECK(close_to(stats_t_critical(0.95, 1.0e7), 1.95996, 0.001));
    CHECK(stats_t_critical(0.95, 5.0) > stats_t_critical(0.95, 50.0));
    CHECK(stats_t_critical(0.99, 10.0) > stats_t_critical(0.95, 10.0));
    /* Invalid arguments are rejected rather than returning a bogus number. */
    CHECK(std::isnan(stats_t_critical(0.0, 10.0)));
    CHECK(std::isnan(stats_t_critical(1.0, 10.0)));
    CHECK(std::isnan(stats_t_critical(0.95, 0.0)));

    CHECK(stats_mean_ci(nullptr, 3, &mean, &lo, &hi) == STATS_ERR_NULL);
    CHECK(stats_mean_ci(data, 0, &mean, &lo, &hi) == STATS_ERR_EMPTY);
    CHECK(stats_mean_ci(data, 1, &mean, &lo, &hi) == STATS_ERR_INVALID);
    CHECK(stats_mean_ci(data, 8, nullptr, nullptr, nullptr) == STATS_ERR_NULL);
  }

  /* ---------- v1.4: two-sample t-test ---------- */
  {
    const double a[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    const double b[] = {2.0, 3.0, 4.0, 5.0, 6.0}; /* mean shifted +1 */
    double t = 0.0;
    double df = 0.0;

    /* Same variance samples: pooled and Welch should be similar */
    CHECK(stats_ttest_ind(a, 5, b, 5, 0, &t, &df) == STATS_OK);
    CHECK(close_to(df, 8.0));
    CHECK(t < 0.0); /* mean_a < mean_b */

    /* Equal samples → t ≈ 0 */
    CHECK(stats_ttest_ind(a, 5, a, 5, 0, &t, &df) == STATS_OK);
    CHECK(close_to(t, 0.0));
    CHECK(close_to(df, 8.0));

    CHECK(stats_ttest_ind(a, 5, b, 5, 1, &t, &df) == STATS_OK);
    CHECK(std::isfinite(t));
    CHECK(std::isfinite(df));
    CHECK(df > 0.0);

    /* Known: two groups with known means/vars
       a mean=3 var=2.5; b mean=6 var=2.5 (b = a+3)
       pooled se = sqrt(2.5*(1/5+1/5)) = sqrt(1) = 1
       t = (3-6)/1 = -3, df=8 */
    const double c[] = {4.0, 5.0, 6.0, 7.0, 8.0};
    CHECK(stats_ttest_ind(a, 5, c, 5, 0, &t, &df) == STATS_OK);
    CHECK(close_to(t, -3.0));
    CHECK(close_to(df, 8.0));

    CHECK(stats_ttest_ind(nullptr, 5, b, 5, 0, &t, &df) == STATS_ERR_NULL);
    CHECK(stats_ttest_ind(a, 5, b, 5, 0, nullptr, nullptr) == STATS_ERR_NULL);
    CHECK(stats_ttest_ind(a, 0, b, 5, 0, &t, &df) == STATS_ERR_EMPTY);
    CHECK(stats_ttest_ind(a, 1, b, 5, 0, &t, &df) == STATS_ERR_INVALID);
  }

  /* ---------- v1.5: robust z-scores (MAD) ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double z[5] = {0};
    /* median=3, MAD of |x-3| = median(2,1,0,1,2)=1
       scale = 1.482602218505602 * 1 */
    const double scale = 1.482602218505602;
    CHECK(stats_robust_zscore(data, 5, z) == STATS_OK);
    CHECK(close_to(z[2], 0.0));
    CHECK(close_to(z[0], (1.0 - 3.0) / scale));
    CHECK(close_to(z[4], (5.0 - 3.0) / scale));

    const double same_z[] = {7.0, 7.0, 7.0};
    CHECK(stats_robust_zscore(same_z, 3, z) == STATS_OK);
    CHECK(close_to(z[0], 0.0));
    CHECK(close_to(z[1], 0.0));
    CHECK(close_to(z[2], 0.0));

    CHECK(stats_robust_zscore(nullptr, 3, z) == STATS_ERR_NULL);
    CHECK(stats_robust_zscore(data, 3, nullptr) == STATS_ERR_NULL);
    CHECK(stats_robust_zscore(data, 0, z) == STATS_ERR_EMPTY);
  }

  /* ---------- v1.5: winsorize ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0, 9.0, 100.0};
    double w[10] = {0};
    /* limits 0.1 → p10 and p90 on sorted [1..9,100]
       n=10, p10 = pos 0.9*(9)=0.9 → 1+0.9*(2-1)=1.9
       p90 = pos 0.9*9=8.1 → 9+0.1*(100-9)=18.1 */
    CHECK(stats_winsorize(data, 10, 0.1, w) == STATS_OK);
    CHECK(close_to(w[0], 1.9));   /* 1 clamped up */
    CHECK(close_to(w[9], 18.1));  /* 100 clamped down */
    CHECK(close_to(w[4], 5.0));   /* middle unchanged */

    /* limits 0 → identity */
    CHECK(stats_winsorize(data, 10, 0.0, w) == STATS_OK);
    CHECK(close_to(w[0], 1.0));
    CHECK(close_to(w[9], 100.0));

    CHECK(stats_winsorize(nullptr, 5, 0.1, w) == STATS_ERR_NULL);
    CHECK(stats_winsorize(data, 10, 0.1, nullptr) == STATS_ERR_NULL);
    CHECK(stats_winsorize(data, 0, 0.1, w) == STATS_ERR_EMPTY);
    CHECK(stats_winsorize(data, 10, 0.5, w) == STATS_ERR_INVALID);
    CHECK(stats_winsorize(data, 10, -0.1, w) == STATS_ERR_INVALID);
  }

  /* ---------- v1.5: entropy ---------- */
  {
    const double same[] = {3.0, 3.0, 3.0, 3.0};
    double h = -1.0;
    /* all equal → one occupied bin → H = 0 */
    CHECK(stats_entropy(same, 4, 4, &h) == STATS_OK);
    CHECK(close_to(h, 0.0));

    /* two equal bins of 2: H = -2*(0.5*ln(0.5)) = ln(2) */
    const double two[] = {0.0, 0.0, 1.0, 1.0};
    CHECK(stats_entropy(two, 4, 2, &h) == STATS_OK);
    CHECK(close_to(h, std::log(2.0), 0.00001));

    CHECK(stats_entropy(nullptr, 4, 2, &h) == STATS_ERR_NULL);
    CHECK(stats_entropy(two, 4, 2, nullptr) == STATS_ERR_NULL);
    CHECK(stats_entropy(two, 0, 2, &h) == STATS_ERR_EMPTY);
    CHECK(stats_entropy(two, 4, 0, &h) == STATS_ERR_INVALID);
  }

  /* ---------- v1.5: nunique ---------- */
  {
    size_t nu = 0;
    const double data[] = {1.0, 2.0, 2.0, 3.0, 1.0};
    CHECK(stats_nunique(data, 5, &nu) == STATS_OK);
    CHECK(nu == 3);

    const double all_same[] = {5.0, 5.0, 5.0};
    CHECK(stats_nunique(all_same, 3, &nu) == STATS_OK);
    CHECK(nu == 1);

    const double all_unique[] = {1.0, 2.0, 3.0, 4.0};
    CHECK(stats_nunique(all_unique, 4, &nu) == STATS_OK);
    CHECK(nu == 4);

    CHECK(stats_nunique(nullptr, 3, &nu) == STATS_ERR_NULL);
    CHECK(stats_nunique(data, 5, nullptr) == STATS_ERR_NULL);
    CHECK(stats_nunique(data, 0, &nu) == STATS_ERR_EMPTY);
  }

  /* ---------- v1.5: ACF ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double acf = 0.0;
    CHECK(stats_acf(data, 5, 0, &acf) == STATS_OK);
    CHECK(close_to(acf, 1.0));

    /* lag 1 for 1..5: mean=3, denom=10
       numer = (-2)*(-1)+(-1)*0+0*1+1*2 = 2+0+0+2 = 4 → 0.4 */
    CHECK(stats_acf(data, 5, 1, &acf) == STATS_OK);
    CHECK(close_to(acf, 0.4));

    /* Perfect alternating-ish short series still finite */
    CHECK(stats_acf(data, 5, 4, &acf) == STATS_OK);
    CHECK(std::isfinite(acf));

    CHECK(stats_acf(nullptr, 5, 1, &acf) == STATS_ERR_NULL);
    CHECK(stats_acf(data, 5, 1, nullptr) == STATS_ERR_NULL);
    CHECK(stats_acf(data, 0, 0, &acf) == STATS_ERR_EMPTY);
    CHECK(stats_acf(data, 5, 5, &acf) == STATS_ERR_INVALID);

    const double constant[] = {2.0, 2.0, 2.0};
    CHECK(stats_acf(constant, 3, 0, &acf) == STATS_OK);
    CHECK(close_to(acf, 1.0));
    CHECK(stats_acf(constant, 3, 1, &acf) == STATS_ERR_INVALID);
  }

  /* ---------- v2.1: ACF series ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double series[5];
    CHECK(stats_acf_series(data, 5, 2, series) == STATS_OK);
    CHECK(close_to(series[0], 1.0));
    CHECK(close_to(series[1], 0.4));
    /* lag 2: numer = (-2)*0 + (-1)*1 + 0*2 = -1, denom = 10 → -0.1 */
    CHECK(close_to(series[2], -0.1));

    double single = 0.0;
    for (size_t lag = 0; lag <= 4; ++lag) {
      CHECK(stats_acf(data, 5, lag, &single) == STATS_OK);
      CHECK(stats_acf_series(data, 5, lag, series) == STATS_OK);
      CHECK(close_to(series[lag], single));
    }

    CHECK(stats_acf_series(nullptr, 5, 1, series) == STATS_ERR_NULL);
    CHECK(stats_acf_series(data, 5, 1, nullptr) == STATS_ERR_NULL);
    CHECK(stats_acf_series(data, 0, 0, series) == STATS_ERR_EMPTY);
    CHECK(stats_acf_series(data, 5, 5, series) == STATS_ERR_INVALID);

    const double constant[] = {2.0, 2.0, 2.0};
    CHECK(stats_acf_series(constant, 3, 0, series) == STATS_OK);
    CHECK(close_to(series[0], 1.0));
    CHECK(stats_acf_series(constant, 3, 1, series) == STATS_ERR_INVALID);
  }

  /* ---------- v2.1: ECDF ---------- */
  {
    const double data[] = {5.0, 1.0, 3.0, 2.0, 4.0};
    const double orig[] = {5.0, 1.0, 3.0, 2.0, 4.0};
    double f = 0.0;
    /* Unsorted input: F(3) = 3/5, and the array is left untouched. */
    CHECK(stats_ecdf(data, 5, 3.0, &f) == STATS_OK);
    CHECK(close_to(f, 0.6));
    CHECK(data[0] == orig[0] && data[1] == orig[1] && data[2] == orig[2] &&
          data[3] == orig[3] && data[4] == orig[4]);

    CHECK(stats_ecdf(data, 5, 0.0, &f) == STATS_OK);
    CHECK(close_to(f, 0.0));
    CHECK(stats_ecdf(data, 5, 5.0, &f) == STATS_OK);
    CHECK(close_to(f, 1.0));
    CHECK(stats_ecdf(data, 5, 2.5, &f) == STATS_OK);
    CHECK(close_to(f, 0.4));

    const double tied[] = {1.0, 2.0, 2.0, 2.0, 5.0};
    CHECK(stats_ecdf(tied, 5, 2.0, &f) == STATS_OK);
    CHECK(close_to(f, 0.8));

    CHECK(stats_ecdf(nullptr, 5, 1.0, &f) == STATS_ERR_NULL);
    CHECK(stats_ecdf(data, 5, 1.0, nullptr) == STATS_ERR_NULL);
    CHECK(stats_ecdf(data, 0, 1.0, &f) == STATS_ERR_EMPTY);
  }

  /* ---------- v1.5: percentile rank ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0, 4.0, 5.0};
    double pr = 0.0;
    /* value 3: below=2, equal=1 → (2+0.5)/5 = 0.5 */
    CHECK(stats_percentile_rank(data, 5, 3.0, &pr) == STATS_OK);
    CHECK(close_to(pr, 0.5));
    /* value 1: below=0, equal=1 → 0.5/5 = 0.1 */
    CHECK(stats_percentile_rank(data, 5, 1.0, &pr) == STATS_OK);
    CHECK(close_to(pr, 0.1));
    /* value 5: below=4, equal=1 → 4.5/5 = 0.9 */
    CHECK(stats_percentile_rank(data, 5, 5.0, &pr) == STATS_OK);
    CHECK(close_to(pr, 0.9));
    /* value not in set: 0 → 0; 6 → 1 */
    CHECK(stats_percentile_rank(data, 5, 0.0, &pr) == STATS_OK);
    CHECK(close_to(pr, 0.0));
    CHECK(stats_percentile_rank(data, 5, 6.0, &pr) == STATS_OK);
    CHECK(close_to(pr, 1.0));

    CHECK(stats_percentile_rank(nullptr, 5, 1.0, &pr) == STATS_ERR_NULL);
    CHECK(stats_percentile_rank(data, 5, 1.0, nullptr) == STATS_ERR_NULL);
    CHECK(stats_percentile_rank(data, 0, 1.0, &pr) == STATS_ERR_EMPTY);
  }

  /* ---------- v1.5: Jarque–Bera ---------- */
  {
    const double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    double jb = 0.0;
    double p = 0.0;
    CHECK(stats_jarque_bera(data, 8, &jb, &p) == STATS_OK);
    /* Jarque-Bera is defined on the POPULATION (biased) moments, matching
       scipy.stats.jarque_bera and R's tseries::jarque.bera.test. For this
       sample: m2 = 4.5, m3 = 6.28125, m4 = 63.28125, so
         S = m3 / m2^1.5      = 0.65625
         K = m4 / m2^2 - 3    = -0.21875
         JB = 8/6 * (S^2 + K^2/4) = 0.5901692708...
       NOTE: an earlier version of this test used the bias-corrected sample
       moments the summary reports (0.8184875534 / 0.940625), which is not how
       the statistic is defined — it asserted the implementation rather than
       the definition, and so locked in a value ~3x too large. */
    const double s = 0.65625;
    const double k = -0.21875;
    const double expected = (8.0 / 6.0) * (s * s + (k * k) / 4.0);
    CHECK(close_to(expected, 0.5901692708, 1e-9));
    CHECK(close_to(jb, expected, 1e-9));
    CHECK(close_to(p, std::exp(-0.5 * jb), 0.0001));
    CHECK(p > 0.0 && p <= 1.0);

    /* Normal-ish symmetric small sample still finite */
    const double sym[] = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0, 7.0, 8.0};
    CHECK(stats_jarque_bera(sym, 8, &jb, &p) == STATS_OK);
    CHECK(std::isfinite(jb));
    CHECK(jb >= 0.0);

    CHECK(stats_jarque_bera(nullptr, 8, &jb, &p) == STATS_ERR_NULL);
    CHECK(stats_jarque_bera(data, 8, nullptr, nullptr) == STATS_ERR_NULL);
    CHECK(stats_jarque_bera(data, 0, &jb, &p) == STATS_ERR_EMPTY);
    CHECK(stats_jarque_bera(data, 3, &jb, &p) == STATS_ERR_INVALID);

    const double constant[] = {1.0, 1.0, 1.0, 1.0};
    CHECK(stats_jarque_bera(constant, 4, &jb, &p) == STATS_ERR_INVALID);
  }


  /* ---------- v1.6: rms / diff / cumsum / argmin/max / rank ---------- */
  {
    const double data[] = {3.0, 1.0, 4.0, 1.0, 5.0};
    double rms = 0.0;
    CHECK(stats_rms(data, 5, &rms) == STATS_OK);
    /* sqrt((9+1+16+1+25)/5) = sqrt(10.4) */
    CHECK(close_to(rms, std::sqrt(10.4)));

    double d[4];
    CHECK(stats_diff(data, 5, d) == STATS_OK);
    CHECK(close_to(d[0], -2.0));
    CHECK(close_to(d[1], 3.0));
    CHECK(close_to(d[2], -3.0));
    CHECK(close_to(d[3], 4.0));
    CHECK(stats_diff(data, 1, d) == STATS_ERR_INVALID);

    double cs[5];
    CHECK(stats_cumsum(data, 5, cs) == STATS_OK);
    CHECK(close_to(cs[0], 3.0));
    CHECK(close_to(cs[4], 14.0));

    size_t imin = 99, imax = 99;
    CHECK(stats_argmin(data, 5, &imin) == STATS_OK);
    CHECK(imin == 1); /* first min at index 1 */
    CHECK(stats_argmax(data, 5, &imax) == STATS_OK);
    CHECK(imax == 4);

    double ranks[5];
    CHECK(stats_rank(data, 5, ranks) == STATS_OK);
    /* values: 3,1,4,1,5 → sorted 1,1,3,4,5 ranks mid 1.5,1.5,3,4,5 */
    CHECK(close_to(ranks[1], 1.5));
    CHECK(close_to(ranks[3], 1.5));
    CHECK(close_to(ranks[0], 3.0));
    CHECK(close_to(ranks[2], 4.0));
    CHECK(close_to(ranks[4], 5.0));

    CHECK(stats_rms(nullptr, 5, &rms) == STATS_ERR_NULL);
    CHECK(stats_rms(data, 0, &rms) == STATS_ERR_EMPTY);
  }

  /* ---------- v2.0: Student-t CDF ---------- */
  {
    /* Cauchy (df=1): P(T <= 1) = 1/2 + arctan(1)/π = 3/4. */
    CHECK(close_to(stats_t_cdf(1.0, 1.0), 0.75, 1e-9));
    CHECK(close_to(stats_t_cdf(0.0, 1.0), 0.5, 1e-12));
    /* df=2 closed form: 1/2 + x / (2 sqrt(2+x^2)). */
    CHECK(close_to(stats_t_cdf(1.0, 2.0),
                   0.5 + 1.0 / (2.0 * std::sqrt(3.0)), 1e-9));
    /* Critical value is the 1 - α/2 quantile. */
    CHECK(close_to(stats_t_cdf(stats_t_critical(0.95, 9.0), 9.0), 0.975,
                   1e-6));
    CHECK(std::isnan(stats_t_cdf(0.0, 0.0)));
    CHECK(std::isnan(stats_t_cdf(1.0, -1.0)));
  }

  /* ---------- v2.0: bootstrap mean CI (xorshift64, percentile) ---------- */
  {
    const double data[] = {1.0, 2.0, 3.0};
    double lo = 0.0, hi = 0.0, m = 0.0;
    CHECK(stats_bootstrap_mean_ci(data, 3, 20, 1ull, 0.95, &lo, &hi, &m) ==
          STATS_OK);
    CHECK(close_to(m, 2.0));
    /* Independent xorshift64 + linear-interpolation percentile, nboot=20. */
    CHECK(close_to(lo, 1.1583333333333332, 1e-12));
    CHECK(close_to(hi, 2.6666666666666665, 1e-12));
    CHECK(lo <= m && m <= hi);

    double lo2 = 0.0, hi2 = 0.0, m2 = 0.0;
    CHECK(stats_bootstrap_mean_ci(data, 3, 20, 1ull, 0.95, &lo2, &hi2, &m2) ==
          STATS_OK);
    CHECK(close_to(lo, lo2) && close_to(hi, hi2)); /* deterministic */

    CHECK(stats_bootstrap_mean_ci(nullptr, 3, 20, 1, 0.95, &lo, &hi, &m) ==
          STATS_ERR_NULL);
    CHECK(stats_bootstrap_mean_ci(data, 0, 20, 1, 0.95, &lo, &hi, &m) ==
          STATS_ERR_EMPTY);
    CHECK(stats_bootstrap_mean_ci(data, 3, 0, 1, 0.95, &lo, &hi, &m) ==
          STATS_ERR_INVALID);
    CHECK(stats_bootstrap_mean_ci(data, 3, 20, 1, 0.0, &lo, &hi, &m) ==
          STATS_ERR_INVALID);
    CHECK(stats_bootstrap_mean_ci(data, 3, 20, 1, 0.95, nullptr, nullptr,
                                  nullptr) == STATS_ERR_NULL);
  }

  /* ---------- v2.0: Mann–Whitney U ---------- */
  {
    const double a[] = {1.0, 2.0, 3.0};
    const double b[] = {4.0, 5.0, 6.0};
    double u = 0.0, p = 0.0;
    /* R1 = 1+2+3 = 6; U = 3*3 + 3*4/2 - 6 = 9. n < 8 → p is NaN. */
    CHECK(stats_mannwhitney(a, 3, b, 3, &u, &p) == STATS_OK);
    CHECK(close_to(u, 9.0));
    CHECK(std::isnan(p));

    const double same[] = {1.0, 2.0, 3.0};
    CHECK(stats_mannwhitney(same, 3, same, 3, &u, &p) == STATS_OK);
    CHECK(close_to(u, 4.5)); /* n1 n2 / 2 when identical */

    const double kX[] = {2, 4, 4, 5, 7, 9, 12, 15, 22, 40};
    const double kY[] = {1, 3, 5, 4, 9, 8, 15, 13, 25, 35};
    CHECK(stats_mannwhitney(kX, 10, kY, 10, &u, &p) == STATS_OK);
    /* Midranks: R1 = 105.5, U = 100 + 55 - 105.5 = 49.5 */
    CHECK(close_to(u, 49.5));
    CHECK(close_to(p, 0.9697703583, 1e-9));

    CHECK(stats_mannwhitney(nullptr, 3, b, 3, &u, &p) == STATS_ERR_NULL);
    CHECK(stats_mannwhitney(a, 3, b, 3, nullptr, nullptr) == STATS_ERR_NULL);
    CHECK(stats_mannwhitney(a, 0, b, 3, &u, &p) == STATS_ERR_EMPTY);
  }

  /* ---------- v2.0: Gaussian KDE (Scott bandwidth) ---------- */
  {
    const double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    double d = 0.0;
    /* sample s = sqrt(32/7), h = 8^{-1/5} s; φ-kernel at x=5. */
    CHECK(stats_kde(data, 8, 5.0, &d) == STATS_OK);
    CHECK(close_to(d, 0.1704511195, 1e-9));
    CHECK(stats_kde(data, 8, 2.0, &d) == STATS_OK);
    CHECK(close_to(d, 0.0816017625, 1e-9));

    const double same[] = {3.0, 3.0, 3.0};
    CHECK(stats_kde(same, 3, 3.0, &d) == STATS_OK);
    CHECK(close_to(d, 1.0));
    CHECK(stats_kde(same, 3, 4.0, &d) == STATS_OK);
    CHECK(close_to(d, 0.0));

    double xs[3] = {2.0, 5.0, 9.0};
    double out[3] = {0, 0, 0};
    CHECK(stats_kde_grid(data, 8, xs, 3, out) == STATS_OK);
    CHECK(close_to(out[1], 0.1704511195, 1e-9));

    CHECK(stats_kde(nullptr, 3, 0.0, &d) == STATS_ERR_NULL);
    CHECK(stats_kde(data, 0, 0.0, &d) == STATS_ERR_EMPTY);
    CHECK(stats_kde_grid(data, 8, xs, 0, out) == STATS_ERR_INVALID);
  }

  /* ---------- v2.0: normal Q–Q (Blom + inverse-erf) ---------- */
  {
    const double data[] = {2.0, 4.0, 4.0, 4.0, 5.0, 5.0, 7.0, 9.0};
    double sample_q[8];
    double theo_q[8];
    CHECK(stats_normal_qq(data, 8, sample_q, theo_q) == STATS_OK);
    CHECK(close_to(sample_q[0], 2.0));
    CHECK(close_to(sample_q[7], 9.0));
    /* p_1 = (1-0.375)/(8.25) = 0.075757...; Φ^{-1} via √2 erfinv. */
    CHECK(close_to(theo_q[0], -1.4342001597, 1e-9));
    CHECK(close_to(theo_q[7], 1.4342001597, 1e-9));
    CHECK(close_to(theo_q[0] + theo_q[7], 0.0, 1e-12));
    CHECK(close_to(theo_q[3] + theo_q[4], 0.0, 1e-12));

    const double one[] = {42.0};
    double sq1, tq1;
    CHECK(stats_normal_qq(one, 1, &sq1, &tq1) == STATS_OK);
    CHECK(close_to(sq1, 42.0));
    CHECK(close_to(tq1, 0.0)); /* Blom p = 0.5 */

    CHECK(stats_normal_qq(nullptr, 3, sample_q, theo_q) == STATS_ERR_NULL);
    CHECK(stats_normal_qq(data, 0, sample_q, theo_q) == STATS_ERR_EMPTY);
  }

  if (failures != 0) {
    std::cerr << failures << " check(s) failed\n";
    return 1;
  }

  return 0;
}
