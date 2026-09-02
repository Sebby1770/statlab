#include "stats_core.h"

#include <math.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

static int compare_double(const void *left, const void *right) {
  const double a = *(const double *)left;
  const double b = *(const double *)right;

  if (a < b) {
    return -1;
  }

  if (a > b) {
    return 1;
  }

  return 0;
}

/* Linear interpolation percentile on a sorted ascending array. p in [0, 1]. */
static double quantile_sorted(const double *sorted, size_t count,
                              double percentile) {
  if (count == 1) {
    return sorted[0];
  }

  const double position = percentile * (double)(count - 1);
  const size_t lower = (size_t)floor(position);
  const size_t upper = lower + 1;

  if (upper >= count) {
    return sorted[count - 1];
  }

  const double weight = position - (double)lower;
  return sorted[lower] + ((sorted[upper] - sorted[lower]) * weight);
}

/* Most frequent value; first mode after sort if multimodal; NaN if all unique. */
static double compute_mode(const double *sorted, size_t count) {
  if (count == 0) {
    return NAN;
  }

  size_t best_count = 1;
  size_t run_count = 1;
  double best_value = sorted[0];

  for (size_t i = 1; i < count; ++i) {
    if (sorted[i] == sorted[i - 1]) {
      ++run_count;
    } else {
      /* Strict > keeps the earliest mode when frequencies tie. */
      if (run_count > best_count) {
        best_count = run_count;
        best_value = sorted[i - 1];
      }
      run_count = 1;
    }
  }

  if (run_count > best_count) {
    best_count = run_count;
    best_value = sorted[count - 1];
  }

  if (best_count == 1) {
    return NAN; /* all values unique */
  }

  return best_value;
}

/* Paired means and second moments for correlation / regression. */
static StatsStatus paired_moments(const double *x, const double *y, size_t n,
                                  double *mean_x, double *mean_y, double *ss_xx,
                                  double *ss_yy, double *ss_xy) {
  if ((x == NULL || y == NULL) && n > 0) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  double mx = 0.0;
  double my = 0.0;
  for (size_t i = 0; i < n; ++i) {
    mx += x[i];
    my += y[i];
  }
  mx /= (double)n;
  my /= (double)n;

  double sxx = 0.0;
  double syy = 0.0;
  double sxy = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double dx = x[i] - mx;
    const double dy = y[i] - my;
    sxx += dx * dx;
    syy += dy * dy;
    sxy += dx * dy;
  }

  if (mean_x != NULL) {
    *mean_x = mx;
  }
  if (mean_y != NULL) {
    *mean_y = my;
  }
  if (ss_xx != NULL) {
    *ss_xx = sxx;
  }
  if (ss_yy != NULL) {
    *ss_yy = syy;
  }
  if (ss_xy != NULL) {
    *ss_xy = sxy;
  }

  return STATS_OK;
}

StatsStatus stats_percentile(const double *sorted, size_t n, double p,
                             double *out) {
  if (out == NULL || (sorted == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (p < 0.0 || p > 1.0 || !isfinite(p)) {
    return STATS_ERR_INVALID;
  }

  *out = quantile_sorted(sorted, n, p);
  return STATS_OK;
}

StatsStatus stats_histogram(const double *values, size_t n, size_t bins,
                            size_t *counts_out, double *min_out,
                            double *max_out) {
  if (counts_out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (bins == 0) {
    return STATS_ERR_INVALID;
  }

  double min = values[0];
  double max = values[0];

  for (size_t i = 1; i < n; ++i) {
    if (values[i] < min) {
      min = values[i];
    }
    if (values[i] > max) {
      max = values[i];
    }
  }

  memset(counts_out, 0, sizeof(size_t) * bins);

  if (min == max) {
    counts_out[0] = n;
  } else {
    const double width = (max - min) / (double)bins;

    for (size_t i = 0; i < n; ++i) {
      size_t bin = (size_t)((values[i] - min) / width);
      if (bin >= bins) {
        bin = bins - 1; /* include max in the last bin */
      }
      counts_out[bin] += 1;
    }
  }

  if (min_out != NULL) {
    *min_out = min;
  }
  if (max_out != NULL) {
    *max_out = max;
  }

  return STATS_OK;
}

StatsStatus stats_summary(const double *values, size_t count, StatsSummary *out) {
  if (out == NULL || (values == NULL && count > 0)) {
    return STATS_ERR_NULL;
  }

  if (count == 0) {
    return STATS_ERR_EMPTY;
  }

  if (count > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *sorted = (double *)malloc(sizeof(double) * count);
  if (sorted == NULL) {
    return STATS_ERR_ALLOCATION;
  }

  double sum = 0.0;
  double mean = 0.0;
  double m2 = 0.0;
  double min = values[0];
  double max = values[0];

  for (size_t i = 0; i < count; ++i) {
    const double x = values[i];
    sorted[i] = x;
    sum += x;

    if (x < min) {
      min = x;
    }

    if (x > max) {
      max = x;
    }

    /* Welford online mean / M2 */
    const double delta = x - mean;
    mean += delta / (double)(i + 1);
    const double delta2 = x - mean;
    m2 += delta * delta2;
  }

  qsort(sorted, count, sizeof(double), compare_double);

  out->count = count;
  out->sum = sum;
  out->min = min;
  out->max = max;
  out->range = max - min;
  out->mean = mean;
  out->median = quantile_sorted(sorted, count, 0.50);
  out->q1 = quantile_sorted(sorted, count, 0.25); /* p25 */
  out->q3 = quantile_sorted(sorted, count, 0.75); /* p75 */
  out->iqr = out->q3 - out->q1;
  out->p10 = quantile_sorted(sorted, count, 0.10);
  out->p90 = quantile_sorted(sorted, count, 0.90);
  out->mode = compute_mode(sorted, count);
  out->variance = m2 / (double)count;
  out->stddev = sqrt(out->variance);
  out->sample_variance = count > 1 ? m2 / (double)(count - 1) : 0.0;
  out->sample_stddev = sqrt(out->sample_variance);

  /* Tukey fences and outlier count (v1.3). */
  out->fence_low = out->q1 - 1.5 * out->iqr;
  out->fence_high = out->q3 + 1.5 * out->iqr;
  out->outlier_count = 0;
  for (size_t i = 0; i < count; ++i) {
    if (values[i] < out->fence_low || values[i] > out->fence_high) {
      out->outlier_count += 1;
    }
  }

  /* MAD: median of |x_i - median| */
  double *abs_dev = (double *)malloc(sizeof(double) * count);
  if (abs_dev == NULL) {
    free(sorted);
    return STATS_ERR_ALLOCATION;
  }

  for (size_t i = 0; i < count; ++i) {
    abs_dev[i] = fabs(sorted[i] - out->median);
  }
  qsort(abs_dev, count, sizeof(double), compare_double);
  out->mad = quantile_sorted(abs_dev, count, 0.50);
  free(abs_dev);

  /* Higher moments for skewness / kurtosis (about the mean). */
  double m3 = 0.0;
  double m4 = 0.0;
  for (size_t i = 0; i < count; ++i) {
    const double d = values[i] - mean;
    const double d2 = d * d;
    m3 += d2 * d;
    m4 += d2 * d2;
  }

  /* Bias-corrected sample skewness (Fisher / adjusted Fisher-Pearson). */
  if (count >= 3 && out->sample_stddev > 0.0) {
    const double s3 = out->sample_stddev * out->sample_stddev * out->sample_stddev;
    const double n = (double)count;
    out->skewness = (n / ((n - 1.0) * (n - 2.0))) * (m3 / s3);
  } else {
    out->skewness = NAN;
  }

  /* Bias-corrected excess kurtosis. */
  if (count >= 4 && out->sample_stddev > 0.0) {
    const double n = (double)count;
    const double s2 = out->sample_variance;
    const double s4 = s2 * s2;
    const double term1 =
        (n * (n + 1.0) / ((n - 1.0) * (n - 2.0) * (n - 3.0))) * (m4 / s4);
    const double term2 =
        (3.0 * (n - 1.0) * (n - 1.0)) / ((n - 2.0) * (n - 3.0));
    out->kurtosis = term1 - term2;
  } else {
    out->kurtosis = NAN;
  }

  /* Coefficient of variation uses population stddev. */
  if (mean != 0.0 && isfinite(mean)) {
    out->cv = out->stddev / mean;
  } else {
    out->cv = NAN;
  }

  free(sorted);

  return STATS_OK;
}

StatsStatus stats_correlation(const double *x, const double *y, size_t n,
                              double *out) {
  if (out == NULL) {
    return STATS_ERR_NULL;
  }

  double ss_xx = 0.0;
  double ss_yy = 0.0;
  double ss_xy = 0.0;
  const StatsStatus st =
      paired_moments(x, y, n, NULL, NULL, &ss_xx, &ss_yy, &ss_xy);
  if (st != STATS_OK) {
    return st;
  }

  if (n < 2 || ss_xx <= 0.0 || ss_yy <= 0.0) {
    return STATS_ERR_INVALID;
  }

  *out = ss_xy / sqrt(ss_xx * ss_yy);
  return STATS_OK;
}

StatsStatus stats_linreg(const double *x, const double *y, size_t n,
                         double *slope, double *intercept, double *r2) {
  if (slope == NULL || intercept == NULL) {
    return STATS_ERR_NULL;
  }

  double mean_x = 0.0;
  double mean_y = 0.0;
  double ss_xx = 0.0;
  double ss_yy = 0.0;
  double ss_xy = 0.0;
  const StatsStatus st =
      paired_moments(x, y, n, &mean_x, &mean_y, &ss_xx, &ss_yy, &ss_xy);
  if (st != STATS_OK) {
    return st;
  }

  if (n < 2 || ss_xx <= 0.0) {
    return STATS_ERR_INVALID;
  }

  *slope = ss_xy / ss_xx;
  *intercept = mean_y - (*slope) * mean_x;

  if (r2 != NULL) {
    if (ss_yy <= 0.0) {
      /* y is constant: perfect fit only if residuals are zero (ss_xy==0 too). */
      *r2 = 1.0;
    } else {
      const double r = ss_xy / sqrt(ss_xx * ss_yy);
      *r2 = r * r;
    }
  }

  return STATS_OK;
}

StatsStatus stats_trimmed_mean(const double *values, size_t n,
                               double trim_fraction, double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (!isfinite(trim_fraction) || trim_fraction < 0.0 || trim_fraction >= 0.5) {
    return STATS_ERR_INVALID;
  }

  if (n > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *sorted = (double *)malloc(sizeof(double) * n);
  if (sorted == NULL) {
    return STATS_ERR_ALLOCATION;
  }

  memcpy(sorted, values, sizeof(double) * n);
  qsort(sorted, n, sizeof(double), compare_double);

  const size_t k = (size_t)floor((double)n * trim_fraction);
  if (2 * k >= n) {
    free(sorted);
    return STATS_ERR_INVALID;
  }

  double sum = 0.0;
  const size_t remain = n - 2 * k;
  for (size_t i = k; i < n - k; ++i) {
    sum += sorted[i];
  }
  free(sorted);

  *out = sum / (double)remain;
  return STATS_OK;
}

StatsStatus stats_zscore(const double *values, size_t n, double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  double mean = 0.0;
  for (size_t i = 0; i < n; ++i) {
    mean += values[i];
  }
  mean /= (double)n;

  double m2 = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double d = values[i] - mean;
    m2 += d * d;
  }
  const double stddev = sqrt(m2 / (double)n);

  if (stddev == 0.0) {
    for (size_t i = 0; i < n; ++i) {
      out[i] = 0.0;
    }
    return STATS_OK;
  }

  for (size_t i = 0; i < n; ++i) {
    out[i] = (values[i] - mean) / stddev;
  }

  return STATS_OK;
}

StatsStatus stats_tukey_fences(const double *values, size_t n,
                               double *fence_low, double *fence_high,
                               size_t *count_out) {
  StatsSummary summary;
  memset(&summary, 0, sizeof(summary));

  const StatsStatus st = stats_summary(values, n, &summary);
  if (st != STATS_OK) {
    return st;
  }

  if (fence_low != NULL) {
    *fence_low = summary.fence_low;
  }
  if (fence_high != NULL) {
    *fence_high = summary.fence_high;
  }
  if (count_out != NULL) {
    *count_out = summary.outlier_count;
  }

  return STATS_OK;
}

/* Assign average ranks for ties into ranks[0..n-1] for values[0..n-1]. */
static StatsStatus assign_average_ranks(const double *values, size_t n,
                                        double *ranks) {
  if (n > SIZE_MAX / sizeof(size_t)) {
    return STATS_ERR_ALLOCATION;
  }

  size_t *order = (size_t *)malloc(sizeof(size_t) * n);
  if (order == NULL) {
    return STATS_ERR_ALLOCATION;
  }

  for (size_t i = 0; i < n; ++i) {
    order[i] = i;
  }

  /* Insertion sort indices by value (stable, fine for modest n). */
  for (size_t i = 1; i < n; ++i) {
    const size_t key = order[i];
    const double key_v = values[key];
    size_t j = i;
    while (j > 0 && values[order[j - 1]] > key_v) {
      order[j] = order[j - 1];
      --j;
    }
    order[j] = key;
  }

  size_t i = 0;
  while (i < n) {
    size_t j = i + 1;
    while (j < n && values[order[j]] == values[order[i]]) {
      ++j;
    }
    /* Ranks are 1-based: positions i..j-1 → average of (i+1)..j */
    const double avg_rank = 0.5 * ((double)(i + 1) + (double)j);
    for (size_t k = i; k < j; ++k) {
      ranks[order[k]] = avg_rank;
    }
    i = j;
  }

  free(order);
  return STATS_OK;
}

StatsStatus stats_spearman(const double *x, const double *y, size_t n,
                           double *out) {
  if (out == NULL || ((x == NULL || y == NULL) && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (n < 2) {
    return STATS_ERR_INVALID;
  }

  if (n > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *rx = (double *)malloc(sizeof(double) * n);
  double *ry = (double *)malloc(sizeof(double) * n);
  if (rx == NULL || ry == NULL) {
    free(rx);
    free(ry);
    return STATS_ERR_ALLOCATION;
  }

  StatsStatus st = assign_average_ranks(x, n, rx);
  if (st != STATS_OK) {
    free(rx);
    free(ry);
    return st;
  }
  st = assign_average_ranks(y, n, ry);
  if (st != STATS_OK) {
    free(rx);
    free(ry);
    return st;
  }

  st = stats_correlation(rx, ry, n, out);
  free(rx);
  free(ry);
  return st;
}

StatsStatus stats_covariance(const double *x, const double *y, size_t n,
                             int sample, double *out) {
  if (out == NULL) {
    return STATS_ERR_NULL;
  }

  double ss_xy = 0.0;
  const StatsStatus st =
      paired_moments(x, y, n, NULL, NULL, NULL, NULL, &ss_xy);
  if (st != STATS_OK) {
    return st;
  }

  if (sample) {
    if (n < 2) {
      return STATS_ERR_INVALID;
    }
    *out = ss_xy / (double)(n - 1);
  } else {
    *out = ss_xy / (double)n;
  }

  return STATS_OK;
}

StatsStatus stats_moving_average(const double *values, size_t n, size_t window,
                                 double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (window == 0 || window > n) {
    return STATS_ERR_INVALID;
  }

  /* Sliding window sum for O(n) total work. */
  double sum = 0.0;
  for (size_t i = 0; i < window; ++i) {
    sum += values[i];
  }
  out[0] = sum / (double)window;

  const size_t out_len = n - window + 1;
  for (size_t i = 1; i < out_len; ++i) {
    sum += values[i + window - 1] - values[i - 1];
    out[i] = sum / (double)window;
  }

  return STATS_OK;
}

StatsStatus stats_ema(const double *values, size_t n, double alpha,
                      double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (!isfinite(alpha) || alpha <= 0.0 || alpha > 1.0) {
    return STATS_ERR_INVALID;
  }

  out[0] = values[0];
  const double one_minus = 1.0 - alpha;
  for (size_t i = 1; i < n; ++i) {
    out[i] = alpha * values[i] + one_minus * out[i - 1];
  }

  return STATS_OK;
}

StatsStatus stats_geometric_mean(const double *values, size_t n, double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  double log_sum = 0.0;
  for (size_t i = 0; i < n; ++i) {
    if (!(values[i] > 0.0) || !isfinite(values[i])) {
      return STATS_ERR_INVALID;
    }
    log_sum += log(values[i]);
  }

  *out = exp(log_sum / (double)n);
  return STATS_OK;
}

StatsStatus stats_harmonic_mean(const double *values, size_t n, double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  double inv_sum = 0.0;
  for (size_t i = 0; i < n; ++i) {
    if (!(values[i] > 0.0) || !isfinite(values[i])) {
      return STATS_ERR_INVALID;
    }
    inv_sum += 1.0 / values[i];
  }

  if (inv_sum == 0.0 || !isfinite(inv_sum)) {
    return STATS_ERR_INVALID;
  }

  *out = (double)n / inv_sum;
  return STATS_OK;
}

/* Continued-fraction evaluation of the incomplete beta function (Lentz's
   method). Used only to build the Student-t CDF below. */
static double stats_betacf(double a, double b, double x) {
  const int kMaxIter = 300;
  const double kEps = 3.0e-14;
  const double kFpMin = 1.0e-300;

  const double qab = a + b;
  const double qap = a + 1.0;
  const double qam = a - 1.0;
  double c = 1.0;
  double d = 1.0 - qab * x / qap;
  if (fabs(d) < kFpMin) {
    d = kFpMin;
  }
  d = 1.0 / d;
  double h = d;

  for (int m = 1; m <= kMaxIter; ++m) {
    const int m2 = 2 * m;
    double aa = (double)m * (b - (double)m) * x /
                ((qam + (double)m2) * (a + (double)m2));
    d = 1.0 + aa * d;
    if (fabs(d) < kFpMin) {
      d = kFpMin;
    }
    c = 1.0 + aa / c;
    if (fabs(c) < kFpMin) {
      c = kFpMin;
    }
    d = 1.0 / d;
    h *= d * c;

    aa = -(a + (double)m) * (qab + (double)m) * x /
         ((a + (double)m2) * (qap + (double)m2));
    d = 1.0 + aa * d;
    if (fabs(d) < kFpMin) {
      d = kFpMin;
    }
    c = 1.0 + aa / c;
    if (fabs(c) < kFpMin) {
      c = kFpMin;
    }
    d = 1.0 / d;
    const double del = d * c;
    h *= del;
    if (fabs(del - 1.0) < kEps) {
      break;
    }
  }
  return h;
}

/* Regularized incomplete beta I_x(a, b). */
static double stats_betai(double a, double b, double x) {
  if (!(x > 0.0)) {
    return 0.0;
  }
  if (x >= 1.0) {
    return 1.0;
  }
  const double bt = exp(lgamma(a + b) - lgamma(a) - lgamma(b) + a * log(x) +
                        b * log1p(-x));
  if (x < (a + 1.0) / (a + b + 2.0)) {
    return bt * stats_betacf(a, b, x) / a;
  }
  return 1.0 - bt * stats_betacf(b, a, 1.0 - x) / b;
}

double stats_t_cdf(double t, double df) {
  if (!(df > 0.0) || !isfinite(df) || !isfinite(t)) {
    return NAN;
  }
  const double x = df / (df + t * t);
  const double tail = 0.5 * stats_betai(0.5 * df, 0.5, x);
  return (t >= 0.0) ? 1.0 - tail : tail;
}

/* Two-sided Student-t critical value: the t with P(-t < T < t) = conf.
   Found by bisection on the monotone CDF, which cannot diverge. Falls back to
   the normal limit for very large df, where the two agree to well past the
   precision anyone prints. */
double stats_t_critical(double conf, double df) {
  if (!(conf > 0.0) || !(conf < 1.0) || !(df > 0.0) || !isfinite(df)) {
    return NAN;
  }
  if (df > 1.0e6) {
    return (fabs(conf - 0.95) < 1e-12) ? 1.959963984540054 : NAN;
  }

  const double target = 1.0 - 0.5 * (1.0 - conf); /* upper-tail probability */
  double low = 0.0;
  double high = 1.0e4;
  for (int i = 0; i < 200; ++i) {
    const double mid = 0.5 * (low + high);
    if (stats_t_cdf(mid, df) < target) {
      low = mid;
    } else {
      high = mid;
    }
  }
  return 0.5 * (low + high);
}

StatsStatus stats_mean_ci(const double *values, size_t n, double *mean_out,
                          double *lo, double *hi) {
  if ((values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (n < 2) {
    return STATS_ERR_INVALID;
  }

  if (mean_out == NULL && lo == NULL && hi == NULL) {
    return STATS_ERR_NULL;
  }

  double mean = 0.0;
  for (size_t i = 0; i < n; ++i) {
    mean += values[i];
  }
  mean /= (double)n;

  double m2 = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double d = values[i] - mean;
    m2 += d * d;
  }
  const double sample_sd = sqrt(m2 / (double)(n - 1));
  /* sigma is estimated from the data, so the interval is Student-t, not
     normal. Using z = 1.96 here (as this did before) makes the interval far
     too narrow on the small samples this tool is built for: at n = 5 the true
     critical value is 2.776, so a "95%" interval actually covered ~88%. */
  const double tcrit = stats_t_critical(0.95, (double)(n - 1));
  if (!isfinite(tcrit)) {
    return STATS_ERR_INVALID;
  }
  const double half = tcrit * sample_sd / sqrt((double)n);

  if (mean_out != NULL) {
    *mean_out = mean;
  }
  if (lo != NULL) {
    *lo = mean - half;
  }
  if (hi != NULL) {
    *hi = mean + half;
  }

  return STATS_OK;
}

/* Sample mean and sample variance (divide by n-1). Requires n >= 2. */
static StatsStatus sample_mean_var(const double *values, size_t n, double *mean,
                                   double *var) {
  if (values == NULL && n > 0) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  if (n < 2) {
    return STATS_ERR_INVALID;
  }

  double m = 0.0;
  for (size_t i = 0; i < n; ++i) {
    m += values[i];
  }
  m /= (double)n;

  double m2 = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double d = values[i] - m;
    m2 += d * d;
  }

  *mean = m;
  *var = m2 / (double)(n - 1);
  return STATS_OK;
}

StatsStatus stats_ttest_ind(const double *x, size_t nx, const double *y,
                            size_t ny, int welch, double *t_out,
                            double *df_out) {
  if ((x == NULL && nx > 0) || (y == NULL && ny > 0)) {
    return STATS_ERR_NULL;
  }

  if (nx == 0 || ny == 0) {
    return STATS_ERR_EMPTY;
  }

  if (nx < 2 || ny < 2) {
    return STATS_ERR_INVALID;
  }

  if (t_out == NULL && df_out == NULL) {
    return STATS_ERR_NULL;
  }

  double mx = 0.0;
  double my = 0.0;
  double vx = 0.0;
  double vy = 0.0;
  StatsStatus st = sample_mean_var(x, nx, &mx, &vx);
  if (st != STATS_OK) {
    return st;
  }
  st = sample_mean_var(y, ny, &my, &vy);
  if (st != STATS_OK) {
    return st;
  }

  const double nxd = (double)nx;
  const double nyd = (double)ny;
  double t = 0.0;
  double df = 0.0;

  if (welch) {
    const double se2 = vx / nxd + vy / nyd;
    if (se2 <= 0.0) {
      /* Both samples constant and equal means → t = 0; if means differ with
         zero se, undefined. */
      if (mx == my) {
        t = 0.0;
        df = nxd + nyd - 2.0;
      } else {
        return STATS_ERR_INVALID;
      }
    } else {
      t = (mx - my) / sqrt(se2);
      const double a = vx / nxd;
      const double b = vy / nyd;
      const double num = (a + b) * (a + b);
      const double den =
          (a * a) / (nxd - 1.0) + (b * b) / (nyd - 1.0);
      if (den <= 0.0) {
        return STATS_ERR_INVALID;
      }
      df = num / den;
    }
  } else {
    const double df_pool = nxd + nyd - 2.0;
    const double sp2 =
        ((nxd - 1.0) * vx + (nyd - 1.0) * vy) / df_pool;
    const double se2 = sp2 * (1.0 / nxd + 1.0 / nyd);
    df = df_pool;
    if (se2 <= 0.0) {
      if (mx == my) {
        t = 0.0;
      } else {
        return STATS_ERR_INVALID;
      }
    } else {
      t = (mx - my) / sqrt(se2);
    }
  }

  if (t_out != NULL) {
    *t_out = t;
  }
  if (df_out != NULL) {
    *df_out = df;
  }

  return STATS_OK;
}

/* Consistency constant: MAD of N(0,1) ≈ 0.67448975; 1/0.67448975 ≈ 1.4826 */
static const double kMadScale = 1.482602218505602;

StatsStatus stats_robust_zscore(const double *values, size_t n, double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (n > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *sorted = (double *)malloc(sizeof(double) * n);
  if (sorted == NULL) {
    return STATS_ERR_ALLOCATION;
  }

  memcpy(sorted, values, sizeof(double) * n);
  qsort(sorted, n, sizeof(double), compare_double);
  const double median = quantile_sorted(sorted, n, 0.50);

  double *abs_dev = (double *)malloc(sizeof(double) * n);
  if (abs_dev == NULL) {
    free(sorted);
    return STATS_ERR_ALLOCATION;
  }

  for (size_t i = 0; i < n; ++i) {
    abs_dev[i] = fabs(sorted[i] - median);
  }
  qsort(abs_dev, n, sizeof(double), compare_double);
  const double mad = quantile_sorted(abs_dev, n, 0.50);
  free(abs_dev);
  free(sorted);

  const double scale = kMadScale * mad;
  if (scale == 0.0) {
    for (size_t i = 0; i < n; ++i) {
      out[i] = 0.0;
    }
    return STATS_OK;
  }

  for (size_t i = 0; i < n; ++i) {
    out[i] = (values[i] - median) / scale;
  }

  return STATS_OK;
}

StatsStatus stats_winsorize(const double *values, size_t n, double limits,
                            double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (!isfinite(limits) || limits < 0.0 || limits >= 0.5) {
    return STATS_ERR_INVALID;
  }

  if (n > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *sorted = (double *)malloc(sizeof(double) * n);
  if (sorted == NULL) {
    return STATS_ERR_ALLOCATION;
  }

  memcpy(sorted, values, sizeof(double) * n);
  qsort(sorted, n, sizeof(double), compare_double);

  const double lo = quantile_sorted(sorted, n, limits);
  const double hi = quantile_sorted(sorted, n, 1.0 - limits);
  free(sorted);

  for (size_t i = 0; i < n; ++i) {
    double v = values[i];
    if (v < lo) {
      v = lo;
    } else if (v > hi) {
      v = hi;
    }
    out[i] = v;
  }

  return STATS_OK;
}

StatsStatus stats_entropy(const double *values, size_t n, size_t bins,
                          double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (bins == 0) {
    return STATS_ERR_INVALID;
  }

  if (bins > SIZE_MAX / sizeof(size_t)) {
    return STATS_ERR_ALLOCATION;
  }

  size_t *counts = (size_t *)malloc(sizeof(size_t) * bins);
  if (counts == NULL) {
    return STATS_ERR_ALLOCATION;
  }

  const StatsStatus st =
      stats_histogram(values, n, bins, counts, NULL, NULL);
  if (st != STATS_OK) {
    free(counts);
    return st;
  }

  double h = 0.0;
  const double nd = (double)n;
  for (size_t i = 0; i < bins; ++i) {
    if (counts[i] == 0) {
      continue;
    }
    const double p = (double)counts[i] / nd;
    h -= p * log(p);
  }

  free(counts);
  *out = h;
  return STATS_OK;
}

StatsStatus stats_nunique(const double *values, size_t n, size_t *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (n > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *sorted = (double *)malloc(sizeof(double) * n);
  if (sorted == NULL) {
    return STATS_ERR_ALLOCATION;
  }

  memcpy(sorted, values, sizeof(double) * n);
  qsort(sorted, n, sizeof(double), compare_double);

  size_t uniq = 1;
  for (size_t i = 1; i < n; ++i) {
    if (sorted[i] != sorted[i - 1]) {
      ++uniq;
    }
  }

  free(sorted);
  *out = uniq;
  return STATS_OK;
}

StatsStatus stats_acf(const double *values, size_t n, size_t lag, double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (lag >= n) {
    return STATS_ERR_INVALID;
  }

  double mean = 0.0;
  for (size_t i = 0; i < n; ++i) {
    mean += values[i];
  }
  mean /= (double)n;

  double denom = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double d = values[i] - mean;
    denom += d * d;
  }

  if (denom <= 0.0) {
    if (lag == 0) {
      *out = 1.0;
      return STATS_OK;
    }
    return STATS_ERR_INVALID;
  }

  if (lag == 0) {
    *out = 1.0;
    return STATS_OK;
  }

  double numer = 0.0;
  for (size_t i = 0; i + lag < n; ++i) {
    numer += (values[i] - mean) * (values[i + lag] - mean);
  }

  *out = numer / denom;
  return STATS_OK;
}

StatsStatus stats_ecdf(const double *values, size_t n, double x, double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  size_t count = 0;
  for (size_t i = 0; i < n; ++i) {
    if (values[i] <= x) {
      ++count;
    }
  }

  *out = (double)count / (double)n;
  return STATS_OK;
}

StatsStatus stats_acf_series(const double *values, size_t n, size_t max_lag,
                             double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (max_lag >= n) {
    return STATS_ERR_INVALID;
  }

  for (size_t lag = 0; lag <= max_lag; ++lag) {
    const StatsStatus st = stats_acf(values, n, lag, &out[lag]);
    if (st != STATS_OK) {
      return st;
    }
  }

  return STATS_OK;
}

StatsStatus stats_percentile_rank(const double *values, size_t n, double value,
                                  double *out) {
  if (out == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  size_t below = 0;
  size_t equal = 0;
  for (size_t i = 0; i < n; ++i) {
    if (values[i] < value) {
      ++below;
    } else if (values[i] == value) {
      ++equal;
    }
  }

  *out = ((double)below + 0.5 * (double)equal) / (double)n;
  return STATS_OK;
}

StatsStatus stats_jarque_bera(const double *values, size_t n, double *jb,
                              double *papprox) {
  if ((values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }

  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  if (jb == NULL && papprox == NULL) {
    return STATS_ERR_NULL;
  }

  if (n < 4) {
    return STATS_ERR_INVALID;
  }

  /* The Jarque-Bera statistic is defined on the *population* (biased) moments
     -- b1 = m3 / m2^(3/2) and b2 = m4 / m2^2 -- as in Jarque & Bera (1980) and
     as implemented by scipy.stats.jarque_bera and R's tseries::jarque.bera.test.
     stats_summary deliberately reports the bias-corrected sample skewness and
     kurtosis instead (matching Excel SKEW/KURT), so those values must NOT be
     reused here: doing so inflates the statistic badly on small samples and can
     flip the test's conclusion. The moments are therefore computed locally. */
  double mean = 0.0;
  for (size_t i = 0; i < n; ++i) {
    if (!isfinite(values[i])) {
      return STATS_ERR_INVALID;
    }
    mean += values[i];
  }
  mean /= (double)n;

  double m2 = 0.0, m3 = 0.0, m4 = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double d = values[i] - mean;
    const double d2 = d * d;
    m2 += d2;
    m3 += d2 * d;
    m4 += d2 * d2;
  }
  m2 /= (double)n;
  m3 /= (double)n;
  m4 /= (double)n;

  if (!(m2 > 0.0)) {
    return STATS_ERR_INVALID; /* zero variance: skew/kurtosis undefined */
  }

  const double s = m3 / pow(m2, 1.5); /* population skewness */
  const double k = m4 / (m2 * m2) - 3.0; /* population excess kurtosis */
  if (!isfinite(s) || !isfinite(k)) {
    return STATS_ERR_INVALID;
  }
  const double jb_val =
      ((double)n / 6.0) * (s * s + (k * k) / 4.0);

  if (jb != NULL) {
    *jb = jb_val;
  }
  if (papprox != NULL) {
    /* Survival function of χ²(2): P(X >= x) = exp(-x/2) */
    *papprox = exp(-0.5 * jb_val);
  }

  return STATS_OK;
}


StatsStatus stats_rms(const double *values, size_t n, double *out) {
  if (n > 0 && values == NULL) {
    return STATS_ERR_NULL;
  }
  if (out == NULL) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  double acc = 0.0;
  for (size_t i = 0; i < n; ++i) {
    acc += values[i] * values[i];
  }
  *out = sqrt(acc / (double)n);
  return STATS_OK;
}

StatsStatus stats_diff(const double *values, size_t n, double *out) {
  if (n > 0 && values == NULL) {
    return STATS_ERR_NULL;
  }
  if (out == NULL) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  if (n < 2) {
    return STATS_ERR_INVALID;
  }
  for (size_t i = 0; i + 1 < n; ++i) {
    out[i] = values[i + 1] - values[i];
  }
  return STATS_OK;
}

StatsStatus stats_cumsum(const double *values, size_t n, double *out) {
  if (n > 0 && values == NULL) {
    return STATS_ERR_NULL;
  }
  if (out == NULL) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  double running = 0.0;
  for (size_t i = 0; i < n; ++i) {
    running += values[i];
    out[i] = running;
  }
  return STATS_OK;
}

StatsStatus stats_argmin(const double *values, size_t n, size_t *out_index) {
  if (n > 0 && values == NULL) {
    return STATS_ERR_NULL;
  }
  if (out_index == NULL) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  size_t best = 0;
  for (size_t i = 1; i < n; ++i) {
    if (values[i] < values[best]) {
      best = i;
    }
  }
  *out_index = best;
  return STATS_OK;
}

StatsStatus stats_argmax(const double *values, size_t n, size_t *out_index) {
  if (n > 0 && values == NULL) {
    return STATS_ERR_NULL;
  }
  if (out_index == NULL) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  size_t best = 0;
  for (size_t i = 1; i < n; ++i) {
    if (values[i] > values[best]) {
      best = i;
    }
  }
  *out_index = best;
  return STATS_OK;
}


typedef struct {
  double value;
  size_t index;
} StatsRankPair;

static int compare_rank_pair(const void *a, const void *b) {
  const StatsRankPair *pa = (const StatsRankPair *)a;
  const StatsRankPair *pb = (const StatsRankPair *)b;
  if (pa->value < pb->value) {
    return -1;
  }
  if (pa->value > pb->value) {
    return 1;
  }
  if (pa->index < pb->index) {
    return -1;
  }
  if (pa->index > pb->index) {
    return 1;
  }
  return 0;
}

StatsStatus stats_rank(const double *values, size_t n, double *out) {
  if (n > 0 && values == NULL) {
    return STATS_ERR_NULL;
  }
  if (out == NULL) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  StatsRankPair *pairs = (StatsRankPair *)malloc(n * sizeof(StatsRankPair));
  if (pairs == NULL) {
    return STATS_ERR_ALLOCATION;
  }
  for (size_t i = 0; i < n; ++i) {
    pairs[i].value = values[i];
    pairs[i].index = i;
  }
  qsort(pairs, n, sizeof(StatsRankPair), compare_rank_pair);

  size_t i = 0;
  while (i < n) {
    size_t j = i + 1;
    while (j < n && pairs[j].value == pairs[i].value) {
      ++j;
    }
    /* 1-based midranks: average of ranks (i+1) .. j */
    double mid = 0.5 * ((double)(i + 1) + (double)j);
    for (size_t k = i; k < j; ++k) {
      out[pairs[k].index] = mid;
    }
    i = j;
  }
  free(pairs);
  return STATS_OK;
}

StatsStatus stats_lagged_cov(const double *values, size_t n, size_t lag,
                             double *out) {
  if (n > 0 && values == NULL) {
    return STATS_ERR_NULL;
  }
  if (out == NULL) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  if (lag == 0 || lag >= n) {
    return STATS_ERR_INVALID;
  }
  double mean = 0.0;
  for (size_t i = 0; i < n; ++i) {
    mean += values[i];
  }
  mean /= (double)n;
  double acc = 0.0;
  size_t count = n - lag;
  for (size_t t = 0; t < count; ++t) {
    acc += (values[t] - mean) * (values[t + lag] - mean);
  }
  *out = acc / (double)count;
  return STATS_OK;
}


/* Marsaglia xorshift64. Period 2^64 − 1; state 0 is a fixed point and is
   never used as a live seed. */
static uint64_t xorshift64_next(uint64_t *state) {
  uint64_t x = *state;
  x ^= x << 13;
  x ^= x >> 7;
  x ^= x << 17;
  *state = x;
  return x;
}

StatsStatus stats_bootstrap_mean_ci(const double *values, size_t n,
                                    size_t nboot, uint64_t seed, double conf,
                                    double *lo, double *hi, double *mean) {
  if ((values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  if (lo == NULL && hi == NULL && mean == NULL) {
    return STATS_ERR_NULL;
  }
  if (nboot == 0 || !(conf > 0.0 && conf < 1.0) || !isfinite(conf)) {
    return STATS_ERR_INVALID;
  }

  double sample_mean = 0.0;
  for (size_t i = 0; i < n; ++i) {
    sample_mean += values[i];
  }
  sample_mean /= (double)n;
  if (mean != NULL) {
    *mean = sample_mean;
  }

  if (nboot > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *boot = (double *)malloc(sizeof(double) * nboot);
  if (boot == NULL) {
    return STATS_ERR_ALLOCATION;
  }

  uint64_t state = (seed == 0) ? 1ull : seed;
  const uint64_t n64 = (uint64_t)n;
  for (size_t b = 0; b < nboot; ++b) {
    double sum = 0.0;
    for (size_t i = 0; i < n; ++i) {
      const uint64_t r = xorshift64_next(&state);
      const size_t idx = (size_t)(r % n64);
      sum += values[idx];
    }
    boot[b] = sum / (double)n;
  }

  qsort(boot, nboot, sizeof(double), compare_double);
  const double alpha = 0.5 * (1.0 - conf);
  if (lo != NULL) {
    *lo = quantile_sorted(boot, nboot, alpha);
  }
  if (hi != NULL) {
    *hi = quantile_sorted(boot, nboot, 1.0 - alpha);
  }
  free(boot);
  return STATS_OK;
}

StatsStatus stats_mannwhitney(const double *x, size_t nx, const double *y,
                              size_t ny, double *u, double *papprox) {
  if ((x == NULL && nx > 0) || (y == NULL && ny > 0)) {
    return STATS_ERR_NULL;
  }
  if (u == NULL && papprox == NULL) {
    return STATS_ERR_NULL;
  }
  if (nx == 0 || ny == 0) {
    return STATS_ERR_EMPTY;
  }

  if (nx > SIZE_MAX - ny) {
    return STATS_ERR_ALLOCATION;
  }
  const size_t n = nx + ny;
  if (n > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *pool = (double *)malloc(sizeof(double) * n);
  double *ranks = (double *)malloc(sizeof(double) * n);
  if (pool == NULL || ranks == NULL) {
    free(pool);
    free(ranks);
    return STATS_ERR_ALLOCATION;
  }

  memcpy(pool, x, sizeof(double) * nx);
  memcpy(pool + nx, y, sizeof(double) * ny);

  const StatsStatus st = stats_rank(pool, n, ranks);
  if (st != STATS_OK) {
    free(pool);
    free(ranks);
    return st;
  }

  double r1 = 0.0;
  for (size_t i = 0; i < nx; ++i) {
    r1 += ranks[i];
  }
  const double n1 = (double)nx;
  const double n2 = (double)ny;
  const double u_val = n1 * n2 + n1 * (n1 + 1.0) / 2.0 - r1;
  if (u != NULL) {
    *u = u_val;
  }

  if (papprox != NULL) {
    if (nx < 8 || ny < 8) {
      *papprox = NAN;
    } else {
      /* Tie correction from the pooled values (not the ranks array). */
      memcpy(pool, x, sizeof(double) * nx);
      memcpy(pool + nx, y, sizeof(double) * ny);
      qsort(pool, n, sizeof(double), compare_double);

      double t_corr = 0.0;
      size_t i = 0;
      while (i < n) {
        size_t j = i + 1;
        while (j < n && pool[j] == pool[i]) {
          ++j;
        }
        const double t = (double)(j - i);
        t_corr += t * t * t - t;
        i = j;
      }

      const double nd = (double)n;
      double sigma2 = (n1 * n2 / 12.0) * (nd + 1.0);
      if (nd > 1.0) {
        sigma2 = (n1 * n2 / 12.0) *
                 ((nd + 1.0) - t_corr / (nd * (nd - 1.0)));
      }
      if (!(sigma2 > 0.0)) {
        *papprox = 1.0;
      } else {
        const double mu = n1 * n2 / 2.0;
        const double z = (u_val - mu) / sqrt(sigma2);
        double p = 2.0 * (1.0 - 0.5 * (1.0 + erf(fabs(z) / sqrt(2.0))));
        if (p > 1.0) {
          p = 1.0;
        }
        if (p < 0.0) {
          p = 0.0;
        }
        *papprox = p;
      }
    }
  }

  free(pool);
  free(ranks);
  return STATS_OK;
}

static const double kPi = 3.14159265358979323846;
static const double kInvSqrt2Pi = 0.39894228040143267794; /* 1/sqrt(2π) */

static StatsStatus kde_prepare(const double *values, size_t n, double *h_out,
                               int *degenerate, double *point) {
  if (n == 1) {
    *degenerate = 1;
    *point = values[0];
    *h_out = 0.0;
    return STATS_OK;
  }

  double mean = 0.0;
  double var = 0.0;
  const StatsStatus st = sample_mean_var(values, n, &mean, &var);
  if (st != STATS_OK) {
    return st;
  }
  const double s = sqrt(var);
  if (!(s > 0.0)) {
    *degenerate = 1;
    *point = values[0];
    *h_out = 0.0;
    return STATS_OK;
  }
  *degenerate = 0;
  *point = 0.0;
  *h_out = pow((double)n, -1.0 / 5.0) * s;
  return STATS_OK;
}

static double kde_eval_at(const double *values, size_t n, double x, double h,
                          int degenerate, double point) {
  if (degenerate) {
    return (x == point) ? 1.0 : 0.0;
  }
  double acc = 0.0;
  const double inv_h = 1.0 / h;
  for (size_t i = 0; i < n; ++i) {
    const double u = (x - values[i]) * inv_h;
    acc += kInvSqrt2Pi * exp(-0.5 * u * u);
  }
  return acc / ((double)n * h);
}

StatsStatus stats_kde(const double *values, size_t n, double x,
                      double *density) {
  if (density == NULL || (values == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }

  double h = 0.0;
  int degenerate = 0;
  double point = 0.0;
  const StatsStatus st = kde_prepare(values, n, &h, &degenerate, &point);
  if (st != STATS_OK) {
    return st;
  }
  *density = kde_eval_at(values, n, x, h, degenerate, point);
  return STATS_OK;
}

StatsStatus stats_kde_grid(const double *values, size_t n, const double *xs,
                           size_t nxs, double *out) {
  if ((values == NULL && n > 0) || (xs == NULL && nxs > 0) || out == NULL) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  if (nxs == 0) {
    return STATS_ERR_INVALID;
  }

  double h = 0.0;
  int degenerate = 0;
  double point = 0.0;
  const StatsStatus st = kde_prepare(values, n, &h, &degenerate, &point);
  if (st != STATS_OK) {
    return st;
  }
  for (size_t i = 0; i < nxs; ++i) {
    out[i] = kde_eval_at(values, n, xs[i], h, degenerate, point);
  }
  return STATS_OK;
}

/* Inverse error function: Winitzki seed + Newton. */
static double stats_erfinv(double y) {
  if (!isfinite(y) || y < -1.0 || y > 1.0) {
    return NAN;
  }
  if (y == -1.0) {
    return -INFINITY;
  }
  if (y == 1.0) {
    return INFINITY;
  }
  if (y == 0.0) {
    return 0.0;
  }

  const double a = 0.147;
  const double ln = log(1.0 - y * y);
  const double t = 2.0 / (kPi * a) + 0.5 * ln;
  double x = copysign(sqrt(sqrt(t * t - ln / a) - t), y);

  const double two_over_sqrt_pi = 1.12837916709551257390; /* 2/sqrt(π) */
  for (int i = 0; i < 8; ++i) {
    const double err = erf(x) - y;
    const double der = two_over_sqrt_pi * exp(-x * x);
    if (der == 0.0) {
      break;
    }
    x -= err / der;
  }
  return x;
}

StatsStatus stats_normal_qq(const double *values, size_t n, double *sample_q,
                            double *theo_q) {
  if ((values == NULL && n > 0) || sample_q == NULL || theo_q == NULL) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  if (n > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *sorted = (double *)malloc(sizeof(double) * n);
  if (sorted == NULL) {
    return STATS_ERR_ALLOCATION;
  }
  memcpy(sorted, values, sizeof(double) * n);
  qsort(sorted, n, sizeof(double), compare_double);

  const double nd = (double)n;
  for (size_t i = 0; i < n; ++i) {
    sample_q[i] = sorted[i];
    /* Blom (1958): (i − 0.375) / (n + 0.25), i is 1-based. */
    const double p = ((double)(i + 1) - 0.375) / (nd + 0.25);
    theo_q[i] = sqrt(2.0) * stats_erfinv(2.0 * p - 1.0);
  }
  free(sorted);
  return STATS_OK;
}

StatsStatus stats_ks_2samp(const double *x, size_t nx, const double *y,
                           size_t ny, double *d) {
  if (d == NULL || (x == NULL && nx > 0) || (y == NULL && ny > 0)) {
    return STATS_ERR_NULL;
  }
  if (nx == 0 || ny == 0) {
    return STATS_ERR_EMPTY;
  }
  if (nx > SIZE_MAX / sizeof(double) || ny > SIZE_MAX / sizeof(double)) {
    return STATS_ERR_ALLOCATION;
  }

  double *xs = (double *)malloc(sizeof(double) * nx);
  double *ys = (double *)malloc(sizeof(double) * ny);
  if (xs == NULL || ys == NULL) {
    free(xs);
    free(ys);
    return STATS_ERR_ALLOCATION;
  }
  memcpy(xs, x, sizeof(double) * nx);
  memcpy(ys, y, sizeof(double) * ny);
  qsort(xs, nx, sizeof(double), compare_double);
  qsort(ys, ny, sizeof(double), compare_double);

  /* Walk distinct pooled values t; F uses ≤ so both sides consume every
     observation equal to t before the vertical distance is measured. */
  size_t i = 0;
  size_t j = 0;
  double best = 0.0;
  while (i < nx || j < ny) {
    double t;
    if (i < nx && (j >= ny || xs[i] <= ys[j])) {
      t = xs[i];
    } else {
      t = ys[j];
    }
    while (i < nx && xs[i] <= t) {
      ++i;
    }
    while (j < ny && ys[j] <= t) {
      ++j;
    }
    const double diff =
        fabs((double)i / (double)nx - (double)j / (double)ny);
    if (diff > best) {
      best = diff;
    }
  }

  free(xs);
  free(ys);
  *d = best;
  return STATS_OK;
}

StatsStatus stats_ttest_rel(const double *x, const double *y, size_t n,
                            double *t_out, double *df_out) {
  if ((x == NULL && n > 0) || (y == NULL && n > 0)) {
    return STATS_ERR_NULL;
  }
  if (n == 0) {
    return STATS_ERR_EMPTY;
  }
  if (n < 2) {
    return STATS_ERR_INVALID;
  }
  if (t_out == NULL && df_out == NULL) {
    return STATS_ERR_NULL;
  }

  double mean = 0.0;
  for (size_t i = 0; i < n; ++i) {
    mean += x[i] - y[i];
  }
  mean /= (double)n;

  double m2 = 0.0;
  for (size_t i = 0; i < n; ++i) {
    const double d = (x[i] - y[i]) - mean;
    m2 += d * d;
  }
  const double sd = sqrt(m2 / (double)(n - 1));
  double t = 0.0;
  if (sd <= 0.0) {
    if (mean == 0.0) {
      t = 0.0;
    } else {
      return STATS_ERR_INVALID;
    }
  } else {
    t = mean / (sd / sqrt((double)n));
  }

  if (t_out != NULL) {
    *t_out = t;
  }
  if (df_out != NULL) {
    *df_out = (double)(n - 1);
  }
  return STATS_OK;
}

double stats_normal_pdf(double x, double mean, double sd) {
  if (!(sd > 0.0)) {
    return 0.0;
  }
  const double z = (x - mean) / sd;
  return (kInvSqrt2Pi / sd) * exp(-0.5 * z * z);
}

double stats_sum(const double *values, size_t count) {
  if (values == NULL) {
    return 0.0;
  }

  double total = 0.0;
  for (size_t i = 0; i < count; ++i) {
    total += values[i];
  }

  return total;
}

const char *stats_status_message(StatsStatus status) {
  switch (status) {
  case STATS_OK:
    return "ok";
  case STATS_ERR_NULL:
    return "null pointer";
  case STATS_ERR_EMPTY:
    return "empty input";
  case STATS_ERR_ALLOCATION:
    return "allocation failed";
  case STATS_ERR_INVALID:
    return "invalid argument";
  default:
    return "unknown error";
  }
}
