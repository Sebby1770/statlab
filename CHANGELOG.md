# Changelog

All notable changes to StatLab (formerly Hybrid Stats Lab) are documented
in this file.

## [2.2.0] — 2026-09-02

### Added
- `stats_ks_2samp` — two-sample Kolmogorov–Smirnov D = max_t |F_x(t) − F_y(t)|
  over the pooled sample values, with F(t) = #{v ≤ t}/n (same ECDF as
  `stats_ecdf`). Requires nx ≥ 1, ny ≥ 1.
- `stats_ttest_rel` — paired *t*-test: d_i = x_i − y_i,
  t = mean(d) / (s_d / √n), df = n−1. Requires n ≥ 2.
- `stats_normal_pdf(x, mean, sd)` — N(mean, sd²) density; 0 if sd ≤ 0.
- CLI: `--ks [file]` prints KS D vs a second sample (or `--column` /
  `--column2`). `--paired` runs the paired *t*-test on two columns.
- JS port: `ks2samp`, `ttestRel`, `normalPdf` with goldens matching the C
  reference fixture.
- **Web studio:** hover readout on histogram / KDE / ECDF / scatter;
  N(mean, sample sd) overlay on the histogram (scaled to counts); residual
  plot under scatter; bootstrap histogram of 800 resampled means (seed 1);
  Drop outliers (Tukey) on a working copy of the primary series; Copy
  markdown of the metric cards; Log10 toggle (skips non-positive values).
  Night / day is unchanged.

### Changed
- Version **2.2.0**.

### Compatibility
- Existing `stats_*` entry points and `StatsSummary` field order are
  unchanged. New APIs are additive. Previous CLI flags still work.

## [2.1.0] — 2026-09-02

### Added
- `stats_ecdf` — empirical CDF Fₙ(x) = (count of values ≤ x) / n. Does not
  modify the input.
- `stats_acf_series` — autocorrelation for lags 0..max_lag using the same
  definition as `stats_acf` (lag 0 is 1 when variance > 0).
- CLI: `--ecdf <x>` prints F(x). `--acf` without a lag prints lags
  0..min(20, n−1) as CSV rows. `--acf N` still reports a single lag.
- JS port: `ecdf`, `acf`, `acfSeries` with goldens matching the C reference
  fixture.
- **Web studio:** drag-and-drop CSV on the blotter, paste-numbers ingest,
  column overview table, Tukey outlier list, ECDF step and ACF stem charts,
  two-channel histogram overlay, Spearman ρ in the compare panel, daylight
  toggle (`statlab-theme` in localStorage), last-CSV memory (~200k cap),
  keyboard **D** for daylight.

### Changed
- Version **2.1.0**.

### Compatibility
- Existing `stats_*` entry points and `StatsSummary` field order are
  unchanged. New APIs are additive. `--acf N` still works.

## [2.0.0] — 2026-09-02

### Changed
- **Renamed to StatLab.** Project, CMake target, binary, `--version` /
  `--help` text, README, and CI now use `statlab` / **StatLab**. The C
  library prefix remains `stats_*`.
- Version **2.0.0**. Version macro is `STATLAB_VERSION`.

### Added
- `stats_bootstrap_mean_ci` — percentile CI of bootstrap means, Marsaglia
  xorshift64, linear-interpolation percentiles (same rule as
  `stats_percentile`).
- `stats_mannwhitney` — U = n1 n2 + n1(n1+1)/2 − R1 with midranks; optional
  two-sided normal-approx p-value with tie correction (NaN if n < 8).
- `stats_kde` / `stats_kde_grid` — Gaussian KDE, Scott bandwidth
  n^(−1/5)·sample_stddev. Degenerate samples (s = 0) are a unit spike.
- `stats_normal_qq` — sorted sample vs Φ⁻¹ of Blom positions
  (i − 0.375)/(n + 0.25) via inverse erf.
- `stats_t_cdf` — Student-*t* CDF (the incomplete-beta routine already used
  by `stats_t_critical`).
- CLI: `--html <path>`, `--format html`, `--bootstrap [nboot]`, `--seed N`,
  `--mwu [file]`, `--qq`, `--kde [bins]`.
- **Web studio** (`web/`): client-side CSV bench with canvas plots, JSON /
  HTML export, and `404.html` for GitHub Pages.
- `web/stats.js` ES module + Node `node --test` goldens matching the C
  reference fixture.
- CTest smoke tests for the new CLI flags.

### Compatibility
- Existing `stats_*` entry points and `StatsSummary` field order are
  unchanged. New APIs are additive. Previous CLI flags still work on the
  `statlab` binary.

## [1.7.0] — 2026-07-26

### Fixed
- **Jarque-Bera used the wrong moments.** The statistic is defined on the
  *population* (biased) skewness and kurtosis, but the implementation reused
  the bias-corrected sample moments that `stats_summary` reports (matching
  Excel `SKEW`/`KURT`). On a 10-point sample this returned 10.84 instead of
  4.88 — p = 0.004 instead of p = 0.087, flipping the test's conclusion from
  "cannot reject normality" to "reject" at α = 0.05. The moments are now
  computed locally from the definition, matching `scipy.stats.jarque_bera`
  and R's `tseries::jarque.bera.test`. `skewness` and `kurtosis` in the
  summary are unchanged — they remain the bias-corrected values.
  The previous unit test asserted the implementation rather than the
  definition, which is why this survived; it now asserts the definition.

### Changed
- Corrected two stale header comments that still described the old behaviour:
  `stats_mean_ci` documented the `z = 1.96` normal approximation, and
  `stats_robust_zscore` rounded its scale constant to `1.4826` where the code
  correctly uses `1/Phi^-1(3/4) = 1.4826022185056018`.
- **`stats_mean_ci` is now the exact Student-*t* interval** (n-1 df) instead of
  a fixed *z* = 1.96. Sigma is estimated from the data, so the normal
  approximation is only valid for large samples: at n = 5 the true critical
  value is 2.776, meaning a nominal 95% interval actually covered about 88%.
  **This widens CI output** — existing values change, and are now correct.

### Added
- **`tests/reference_values_tests.cpp`** — 44 assertions covering 25 functions,
  every expectation derived from the published definition (cross-checked
  against NumPy) rather than from this library's own output. This is the guard
  the Jarque-Bera bug slipped past: that test had been written by copying the
  implementation's result, so test and code were wrong together. Validating
  the whole surface this way found no further discrepancies — Spearman handles
  ties by midranks, Welch's df is fractional, ACF and EMA match their
  definitions, and the robust z-score uses the exact 1/Phi^-1(3/4).
- `stats_t_critical(conf, df)` — two-sided Student-*t* critical value, found by
  bisecting the t CDF (regularized incomplete beta, Lentz continued fraction).
  Matches published tables to six decimals from df = 1 to df = 1000, with no
  new dependencies.

## [1.6.0] — 2026-07-21

### Added
- **RMS** (`stats_rms`) and CLI `--rms`
- **First differences** (`stats_diff`) / `--diff`
- **Cumulative sum** (`stats_cumsum`) / `--cumsum`
- **Average ranks** (`stats_rank`) / `--ranks`
- **argmin / argmax** indices / `--argmin` `--argmax`
- **Lagged covariance** (`stats_lagged_cov`)
- Unit + CTest coverage for the new APIs

### Changed
- Version **1.6.0**

## [1.5.0] — 2026-07-20

### Added

- **Robust z-scores** (`stats_robust_zscore`): median / MAD scale with the
  normal consistency factor 1.4826
- **Winsorize** (`stats_winsorize`): clamp values to percentile bounds at
  `limits` and `1 − limits`
- **Shannon entropy** (`stats_entropy`) of a fixed-width histogram (nats)
- **Unique count** (`stats_nunique`)
- **Autocorrelation** (`stats_acf`) at integer lag (full-series mean denominator)
- **Percentile rank** (`stats_percentile_rank`) in `[0, 1]` with mid-rank ties
- **Jarque–Bera** (`stats_jarque_bera`) normality statistic from bias-corrected
  skewness / excess kurtosis, plus χ²(2) survival p-approximation
- CLI: `--winsor <f>`, `--entropy [bins]`, `--acf [lag]`, `--robust-z`,
  `--nunique`, `--jb`
- Expanded unit tests and CTest CLI coverage for new features

### Changed

- Version bumped to **1.5.0**

### Compatibility

- Existing C API entry points and `StatsSummary` field order are unchanged.
  New APIs are additive only.

## [1.4.0] — 2026-07-19

### Added

- **Spearman rank correlation** (`stats_spearman`) with average ranks for ties
- **Covariance** (`stats_covariance`) for sample (`÷ n−1`) and population (`÷ n`)
- **Simple moving average** (`stats_moving_average`); output length
  `n − window + 1`
- **Exponential moving average** (`stats_ema`) with alpha in `(0, 1]`
- **Geometric mean** (`stats_geometric_mean`) and **harmonic mean**
  (`stats_harmonic_mean`) when all values are strictly positive
- **Mean confidence interval** (`stats_mean_ci`): normal approx with *z* = 1.96
- **Two-sample *t*-statistic** (`stats_ttest_ind`) with pooled (Student) or
  Welch–Satterthwaite degrees of freedom
- CLI: `--spearman [file]`, `--cov [file]`, `--cov-pop`, `--rolling <N>`,
  `--ema <alpha>`, `--ci`, `--geomean`
- Expanded unit tests and CTest CLI coverage for new features

### Changed

- Version bumped to **1.4.0**

### Compatibility

- Existing C API entry points and `StatsSummary` field order are unchanged.
  New APIs are additive only.

## [1.3.0] — 2026-07-19

### Added

- **Pearson correlation** (`stats_correlation`) for paired series
- **OLS linear regression** (`stats_linreg`) with slope, intercept, and R²
- **Trimmed mean** (`stats_trimmed_mean`) with equal-fraction tail trimming
- **Z-score normalization** (`stats_zscore`) using population standard deviation
- **Tukey fences** on `StatsSummary` (`fence_low`, `fence_high`) and
  `stats_tukey_fences` helper
- **Outlier count** on `StatsSummary` (`outlier_count`) via Tukey 1.5·IQR rule
- CLI: `--correlate [file]`, `--column2`, `--regression`, `--trim <f>`,
  `--outliers`, `--boxplot`
- Documented p25/p75 aliases for q1/q3 (five-number summary)
- Example paired CSV (`examples/paired.csv`)
- Expanded unit tests and CTest CLI coverage

### Changed

- Version bumped to **1.3.0**
- Text report labels q1/q3 as `q1/p25` and `q3/p75`; JSON/CSV emit both
  `q1`/`q3` and `p25`/`p75`
- Summary output includes fence bounds and outlier count

### Compatibility

- `StatsSummary` fields are **append-only**: new members are at the end of the
  struct. Existing field order and meaning are unchanged.

## [1.2.0] — previous

- Descriptive statistics expansion (quartiles, MAD, mode, percentiles,
  skewness, kurtosis, CV)
- Histogram helper and ASCII histogram CLI
- Multi-column CSV (`--column`, `--skip-header`)
- Dataset comparison (`--compare`)
