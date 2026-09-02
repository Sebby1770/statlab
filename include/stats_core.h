#ifndef STATS_CORE_H
#define STATS_CORE_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum StatsStatus {
  STATS_OK = 0,
  STATS_ERR_NULL = 1,
  STATS_ERR_EMPTY = 2,
  STATS_ERR_ALLOCATION = 3,
  STATS_ERR_INVALID = 4
} StatsStatus;

/**
 * Summary statistics for a numeric sample.
 *
 * Fields that are undefined for the given sample size are set to NaN
 * (or 0 where noted). For n == 1:
 *   - sample_variance / sample_stddev are 0
 *   - iqr / mad are 0
 *   - mode is NaN when every value appears once (including n == 1)
 *   - skewness / kurtosis are NaN (need n >= 3 / n >= 4)
 *   - cv is 0 when mean != 0 and stddev is 0; NaN when mean == 0
 *
 * Quartile aliases (five-number summary):
 *   - q1 is the 25th percentile (p25)
 *   - median is the 50th percentile (p50)
 *   - q3 is the 75th percentile (p75)
 * Five-number summary: min, q1 (p25), median, q3 (p75), max.
 *
 * Tukey boxplot fences (appended in v1.3, non-breaking):
 *   - fence_low  = q1 - 1.5 * iqr
 *   - fence_high = q3 + 1.5 * iqr
 *   - outlier_count = number of values strictly outside [fence_low, fence_high]
 */
typedef struct StatsSummary {
  size_t count;
  double sum;
  double min;
  double max;
  double range;
  double mean;
  double median;
  double q1;               /* 25th percentile (p25) */
  double q3;               /* 75th percentile (p75) */
  double iqr;              /* q3 - q1 */
  double mad;              /* median absolute deviation from the median */
  double mode;             /* first modal value after sort; NaN if all unique */
  double p10;              /* 10th percentile (linear interpolation) */
  double p90;              /* 90th percentile (linear interpolation) */
  double variance;         /* population variance (divide by n) */
  double stddev;           /* population standard deviation */
  double sample_variance;  /* sample variance (divide by n-1); 0 if n == 1 */
  double sample_stddev;    /* sample standard deviation; 0 if n == 1 */
  double skewness;         /* bias-corrected sample skewness (Fisher); NaN if n < 3 */
  double kurtosis;         /* bias-corrected excess kurtosis; NaN if n < 4 */
  double cv;               /* coefficient of variation: stddev / mean; NaN if mean == 0 */
  /* --- v1.3 append-only fields (do not reorder above) --- */
  double fence_low;        /* Tukey lower fence: q1 - 1.5 * iqr */
  double fence_high;       /* Tukey upper fence: q3 + 1.5 * iqr */
  size_t outlier_count;    /* count of values < fence_low or > fence_high */
} StatsSummary;

/**
 * Compute a full StatsSummary for the given sample.
 * values may be unsorted. Does not modify the input array.
 */
StatsStatus stats_summary(const double *values, size_t count, StatsSummary *out);

/**
 * Linear-interpolation percentile of an already-sorted ascending array.
 * p is in [0, 1]. Writes the result to *out.
 *
 * Returns STATS_ERR_NULL if sorted or out is NULL (when n > 0),
 * STATS_ERR_EMPTY if n == 0, STATS_ERR_INVALID if p is outside [0, 1].
 */
StatsStatus stats_percentile(const double *sorted, size_t n, double p,
                             double *out);

/**
 * Build a fixed-width histogram over [min, max] of the sample.
 *
 * Fills counts_out[bins] with bin frequencies. The last bin is closed on the
 * right so max maps into the final bin. Writes the observed min/max to
 * min_out / max_out when those pointers are non-NULL.
 *
 * If min == max (all values equal), every observation falls into bin 0.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID (bins == 0).
 */
StatsStatus stats_histogram(const double *values, size_t n, size_t bins,
                            size_t *counts_out, double *min_out,
                            double *max_out);

/**
 * Pearson product-moment correlation coefficient between paired series x and y.
 * Requires n >= 2 and non-zero variance in both series.
 * On success writes r in [-1, 1] to *out.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY (n == 0), or STATS_ERR_INVALID
 * (n == 1, or zero variance in either series).
 */
StatsStatus stats_correlation(const double *x, const double *y, size_t n,
                              double *out);

/**
 * Simple ordinary least squares linear regression: y ≈ slope * x + intercept.
 * Also writes the coefficient of determination R² to *r2 when r2 is non-NULL.
 * Requires n >= 2 and non-zero variance in x.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID.
 */
StatsStatus stats_linreg(const double *x, const double *y, size_t n,
                         double *slope, double *intercept, double *r2);

/**
 * Trimmed mean: sort a copy, drop the same fraction from each tail, average
 * the remainder. trim_fraction is in [0, 0.5). The number trimmed from each
 * side is floor(n * trim_fraction). At least one value must remain.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, STATS_ERR_ALLOCATION, or
 * STATS_ERR_INVALID (bad fraction or nothing left after trimming).
 */
StatsStatus stats_trimmed_mean(const double *values, size_t n,
                               double trim_fraction, double *out);

/**
 * Z-score normalize: out[i] = (values[i] - mean) / population_stddev.
 * Writes n values to out. If population stddev is 0, all out[i] = 0.
 * out may equal values (in-place) only if the caller accepts that; this
 * function always reads from values first into temporaries for mean/stddev.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY.
 */
StatsStatus stats_zscore(const double *values, size_t n, double *out);

/**
 * Tukey fences and outlier count for a sample.
 * fence_low / fence_high / count_out may be NULL if not needed.
 * Uses q1 - 1.5*iqr and q3 + 1.5*iqr (same as StatsSummary fences).
 *
 * Returns STATS_ERR_NULL (values NULL when n > 0), STATS_ERR_EMPTY,
 * STATS_ERR_ALLOCATION.
 */
StatsStatus stats_tukey_fences(const double *values, size_t n,
                               double *fence_low, double *fence_high,
                               size_t *count_out);

/**
 * Spearman rank correlation between paired series x and y.
 * Ranks use average ranks for ties; then Pearson correlation of ranks.
 * Requires n >= 2 and non-zero rank variance in both series.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, STATS_ERR_ALLOCATION, or
 * STATS_ERR_INVALID.
 */
StatsStatus stats_spearman(const double *x, const double *y, size_t n,
                           double *out);

/**
 * Covariance of paired series x and y.
 * sample != 0 → sample covariance (divide by n-1, requires n >= 2).
 * sample == 0 → population covariance (divide by n, requires n >= 1).
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID.
 */
StatsStatus stats_covariance(const double *x, const double *y, size_t n,
                             int sample, double *out);

/**
 * Simple moving average with window size `window`.
 * Writes (n - window + 1) values to out, where
 *   out[i] = mean(values[i .. i+window-1]).
 * Requires 1 <= window <= n. Caller must provide out with capacity
 * (n - window + 1). Does not modify values.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID.
 */
StatsStatus stats_moving_average(const double *values, size_t n, size_t window,
                                 double *out);

/**
 * Exponential moving average.
 * Writes n values to out:
 *   out[0] = values[0]
 *   out[i] = alpha * values[i] + (1 - alpha) * out[i-1]  for i >= 1
 * alpha must be in (0, 1]. out may alias values (safe left-to-right).
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID.
 */
StatsStatus stats_ema(const double *values, size_t n, double alpha,
                      double *out);

/**
 * Geometric mean: exp(mean(log x_i)). All values must be strictly positive.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID (non-positive).
 */
StatsStatus stats_geometric_mean(const double *values, size_t n, double *out);

/**
 * Harmonic mean: n / sum(1/x_i). All values must be strictly positive.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID (non-positive).
 */
StatsStatus stats_harmonic_mean(const double *values, size_t n, double *out);

/**
 * Exact 95% confidence interval for the mean, using Student's t with n-1
 * degrees of freedom (sigma is estimated from the data, so the normal
 * approximation is only valid for large n):
 *   mean ± t(0.975, n-1) * sample_stddev / sqrt(n)
 * Requires n >= 2. Writes lower/upper bounds to *lo / *hi when non-NULL.
 * Also writes the sample mean to *mean_out when non-NULL.
 *
 * Returns STATS_ERR_NULL (values NULL when n > 0, or all outputs NULL with
 * nothing to write), STATS_ERR_EMPTY, or STATS_ERR_INVALID (n < 2).
 */
StatsStatus stats_mean_ci(const double *values, size_t n, double *mean_out,
                          double *lo, double *hi);

/**
 * Student-t CDF: P(T <= t) for T ~ t(df).
 * Returns NAN if df <= 0 or if t or df is non-finite.
 */
double stats_t_cdf(double t, double df);

/**
 * Two-sided Student-t critical value: the t with P(-t < T < t) == conf.
 * conf must be in (0, 1) and df > 0; returns NAN otherwise.
 * Computed by bisecting the t CDF (regularized incomplete beta), so it is
 * exact to double precision rather than a table lookup or approximation.
 */
double stats_t_critical(double conf, double df);

/**
 * Two-sample t-statistic for independent samples.
 * welch != 0 → Welch's t-test (unequal variances) with Welch–Satterthwaite df.
 * welch == 0 → Student's t with pooled variance; df = nx + ny - 2.
 * Requires nx >= 2, ny >= 2. Writes t to *t_out and degrees of freedom to
 * *df_out when non-NULL.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID.
 */
StatsStatus stats_ttest_ind(const double *x, size_t nx, const double *y,
                            size_t ny, int welch, double *t_out,
                            double *df_out);

/**
 * Robust z-scores using median and MAD (v1.5):
 *   out[i] = (values[i] - median) / (k * MAD)
 * where k = 1/Phi^-1(3/4) = 1.4826022185056018, the constant that makes the
 * scaled MAD a consistent estimator of sigma for normal data.
 * The factor 1.4826 makes the scale consistent with the normal distribution
 * (comparable to ordinary z-scores). If MAD is 0, all out[i] = 0.
 * Writes n values to out. Does not modify values.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_ALLOCATION.
 */
StatsStatus stats_robust_zscore(const double *values, size_t n, double *out);

/**
 * Winsorize: clamp each value to [p_low, p_high] where
 *   p_low  = percentile(limits)
 *   p_high = percentile(1 - limits)
 * limits must be in [0, 0.5). Writes n clamped values to out.
 * out may alias values (safe: bounds computed on a sorted copy first).
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, STATS_ERR_ALLOCATION, or
 * STATS_ERR_INVALID (bad limits).
 */
StatsStatus stats_winsorize(const double *values, size_t n, double limits,
                            double *out);

/**
 * Shannon entropy of a fixed-width histogram over the sample range.
 * H = -sum_i p_i * ln(p_i)  (nats), where p_i = count_i / n for nonempty bins.
 * bins must be >= 1. If all values are equal, H = 0.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, STATS_ERR_ALLOCATION, or
 * STATS_ERR_INVALID (bins == 0).
 */
StatsStatus stats_entropy(const double *values, size_t n, size_t bins,
                          double *out);

/**
 * Number of unique values in the sample (exact equality after sort).
 * Writes the count to *out.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_ALLOCATION.
 */
StatsStatus stats_nunique(const double *values, size_t n, size_t *out);

/**
 * Autocorrelation at integer lag (Pearson of series with itself lagged).
 * lag must satisfy 0 <= lag < n. lag 0 always yields 1 when variance > 0.
 * Formula: sum_{t=0}^{n-lag-1} (x_t-m)(x_{t+lag}-m) / sum (x_t-m)^2
 * with m = mean of the full series. Zero variance → STATS_ERR_INVALID
 * unless lag == 0 (then *out = 1).
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID.
 */
StatsStatus stats_acf(const double *values, size_t n, size_t lag, double *out);

/**
 * Percentile rank of `value` in the sample, in [0, 1]:
 *   (count(x < value) + 0.5 * count(x == value)) / n
 * Uses exact floating equality for ties.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY.
 */
StatsStatus stats_percentile_rank(const double *values, size_t n, double value,
                                  double *out);

/**
 * Jarque–Bera normality statistic from sample skewness S and excess kurtosis K
 * (same bias-corrected estimators as stats_summary):
 *   JB = n/6 * (S^2 + K^2/4)
 * Requires n >= 4 and positive sample variance. Writes JB to *jb when non-NULL.
 * When papprox is non-NULL, writes a rough upper-tail p-value under χ²(2):
 *   P(X >= jb) ≈ exp(-jb/2)  (exact for χ² with 2 df).
 *
 * Returns STATS_ERR_NULL (values NULL when n>0, or both outputs NULL),
 * STATS_ERR_EMPTY, STATS_ERR_ALLOCATION, or STATS_ERR_INVALID (n < 4 or
 * zero variance).
 */
StatsStatus stats_jarque_bera(const double *values, size_t n, double *jb,
                              double *papprox);


/**
 * Root mean square: sqrt(mean(x_i^2)).
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY.
 */
StatsStatus stats_rms(const double *values, size_t n, double *out);

/**
 * First differences: out[i] = values[i+1] - values[i] for i in 0..n-2.
 * Requires n >= 2. Writes n-1 values to out.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID (n < 2).
 */
StatsStatus stats_diff(const double *values, size_t n, double *out);

/**
 * Cumulative sum: out[i] = values[0] + ... + values[i].
 * Writes n values to out. out may alias values (safe left-to-right).
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY.
 */
StatsStatus stats_cumsum(const double *values, size_t n, double *out);

/**
 * Index of the first minimum / maximum. On ties, the smallest index wins.
 * Writes the index to *out_index.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY.
 */
StatsStatus stats_argmin(const double *values, size_t n, size_t *out_index);
StatsStatus stats_argmax(const double *values, size_t n, size_t *out_index);

/**
 * Average ranks (1-based, midrank ties) of the sample into out[0..n-1]
 * corresponding to the original order of values.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_ALLOCATION.
 */
StatsStatus stats_rank(const double *values, size_t n, double *out);

/**
 * Sample covariance of the series with itself lagged by `lag` (non-normalized
 * lag product mean). Prefer stats_acf for Pearson autocorrelation.
 * lag must satisfy 0 < lag < n.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_INVALID.
 */
StatsStatus stats_lagged_cov(const double *values, size_t n, size_t lag,
                             double *out);

/**
 * Percentile bootstrap CI for the mean.
 *
 * Draws `nboot` resamples with replacement, each of length n, using a
 * deterministic Marsaglia xorshift64 generator:
 *   x ^= x << 13; x ^= x >> 7; x ^= x << 17;
 * seeded by `seed` (seed 0 is replaced by 1 so the generator cannot stall).
 * Each draw uses `next() % n` as the index. The bootstrap means are sorted
 * and the CI is the linear-interpolation percentile interval at `conf`
 * (conf = 0.95 → 2.5th and 97.5th percentiles of the bootstrap means,
 * same interpolation as stats_percentile).
 *
 * Writes the original sample mean to *mean when non-NULL, and the CI bounds
 * to *lo / *hi when non-NULL.
 *
 * Returns STATS_ERR_NULL (values NULL when n > 0, or all outputs NULL),
 * STATS_ERR_EMPTY (n == 0), STATS_ERR_ALLOCATION, or STATS_ERR_INVALID
 * (nboot == 0, or conf not in (0, 1)).
 */
StatsStatus stats_bootstrap_mean_ci(const double *values, size_t n,
                                    size_t nboot, uint64_t seed, double conf,
                                    double *lo, double *hi, double *mean);

/**
 * Mann–Whitney U for sample x versus sample y.
 *
 * Ranks the pooled sample with 1-based midranks. R1 is the rank sum of x.
 *   U = n1*n2 + n1*(n1+1)/2 - R1
 * which is n1*n2 minus the usual Wilcoxon rank-sum form of U_x.
 *
 * When papprox is non-NULL, writes a two-sided normal-approximation p-value
 * with the standard tie correction
 *   Var(U) = n1 n2 / 12 * ((N+1) - Σ(t³−t)/(N(N−1)))
 *   z = (U − n1 n2 / 2) / σ ,   p = 2 (1 − Φ(|z|))
 * Φ is the standard normal CDF (via erf). papprox is NaN when nx < 8 or
 * ny < 8 (the normal approximation is not considered reliable). If the
 * tie-corrected variance is 0, papprox is 1.
 *
 * u and papprox may each be NULL if not needed.
 *
 * Returns STATS_ERR_NULL (x/y NULL when n > 0, or both outputs NULL),
 * STATS_ERR_EMPTY (nx == 0 or ny == 0), or STATS_ERR_ALLOCATION.
 */
StatsStatus stats_mannwhitney(const double *x, size_t nx, const double *y,
                              size_t ny, double *u, double *papprox);

/**
 * Gaussian kernel density at a single evaluation point `x`.
 * Bandwidth is Scott's rule: h = n^(−1/5) * sample_stddev.
 * Kernel is the standard normal density.
 *   f(x) = (1 / (n h)) Σ φ((x − x_i) / h)
 * If sample_stddev == 0 (including n == 1), density is 1 at the unique
 * sample value and 0 elsewhere.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY.
 */
StatsStatus stats_kde(const double *values, size_t n, double x,
                      double *density);

/**
 * Gaussian KDE evaluated on a grid. Writes nxs densities to out.
 * xs and out must each have length nxs. Same bandwidth / degenerate
 * rules as stats_kde.
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY (n == 0), or STATS_ERR_INVALID
 * (nxs == 0).
 */
StatsStatus stats_kde_grid(const double *values, size_t n, const double *xs,
                           size_t nxs, double *out);

/**
 * Normal Q–Q coordinates. Writes n pairs into caller-allocated arrays
 * sample_q and theo_q (each length n):
 *   sample_q[i] = i-th order statistic of the sample (ascending)
 *   theo_q[i]   = Φ^{−1}(p_i) with Blom positions
 *                 p_i = (i − 0.375) / (n + 0.25), i = 1..n
 * Φ^{−1} is computed as √2 erfinv(2p − 1).
 *
 * Returns STATS_ERR_NULL, STATS_ERR_EMPTY, or STATS_ERR_ALLOCATION.
 */
StatsStatus stats_normal_qq(const double *values, size_t n, double *sample_q,
                            double *theo_q);

/** Sum of values; returns 0.0 if values is NULL. */
double stats_sum(const double *values, size_t count);

/** Human-readable status string. */
const char *stats_status_message(StatsStatus status);

#ifdef __cplusplus
}
#endif

#endif
