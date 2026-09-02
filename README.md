# StatLab

StatLab is a compact statistics workshop: a **C17** engine, a **C++17** CLI
named `statlab`, and a zero-backend **web studio**. Version **2.2.0**.

Studio: [https://sebby1770.github.io/statlab/](https://sebby1770.github.io/statlab/)

The reusable library keeps the `stats_*` C API. The command-line tool,
docs, and desktop-in-the-browser are StatLab.

## Features

- C17 statistics core with a small C-compatible API
- C++17 CLI: text, JSON, CSV, and self-contained HTML
- Descriptive statistics (mean through Tukey fences, moments, geometric /
  harmonic means, Student-*t* CI, Jarque–Bera, …)
- Bivariate analysis: Pearson, Spearman, covariance, OLS, two-sample *t*,
  paired *t*, two-sample KS, Mann–Whitney U
- Series helpers: moving average, EMA, ACF / ACF series, ECDF, ranks, diffs,
  robust z-scores
- Bootstrap percentile CI for the mean (deterministic xorshift64)
- Gaussian KDE (Scott bandwidth) and normal Q–Q (Blom positions)
- ASCII histogram / boxplot, plus SVG in the HTML report
- Multi-column CSV (`--column`, `--column2`, `--skip-header`)
- Web studio: drop a CSV, paste numbers, column overview, outlier list,
  histogram (normal overlay + hover readout), ECDF / ACF / residuals /
  bootstrap-mean charts, log10 and drop-outliers working copy, daylight desk
- CMake + CTest, Node tests for the JS port, GitHub Actions on Ubuntu

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
```

## CLI

```sh
./build/statlab 2 4 4 4 5 5 7 9
./build/statlab --file examples/sample.csv
./build/statlab --format json --precision 2 1,2,3,4,5
./build/statlab --histogram 8 --file examples/sample.csv
./build/statlab --column 1 --column2 2 --correlate --skip-header --file examples/paired.csv
./build/statlab --bootstrap 2000 --seed 1 2 4 4 4 5 5 7 9
./build/statlab --qq --format csv 2 4 4 4 5 5 7 9
./build/statlab --kde 40 --file examples/sample.csv
./build/statlab --mwu examples/sample.csv 1 2 3 4 5 6 7 8 9 10
./build/statlab --html report.html --file examples/sample.csv
./build/statlab --format html --file examples/sample.csv
./build/statlab --ecdf 4.5 1 2 3 4 5 6 7 8 9
./build/statlab --acf --format csv 1 2 3 4 5 6
./build/statlab --column 1 --column2 2 --ks --skip-header --file examples/paired.csv
./build/statlab --column 1 --column2 2 --paired --skip-header --file examples/paired.csv
printf "1,2,3,4,5" | ./build/statlab --format csv
```

`--version` prints `statlab 2.2.0`. `-h` / `--help` lists every flag.
Existing flags from Hybrid Stats Lab still work; they now live on the
`statlab` binary.

### HTML report

`--html <path>` writes a self-contained page (inline CSS, no CDN) with the
summary table, an ASCII histogram, an SVG histogram, a Tukey box plot, and
a scatter + OLS chart when two columns are available. `--format html` dumps
the same document to stdout.

### Bootstrap, Q–Q, KDE, Mann–Whitney

| Flag | What it does |
|------|----------------|
| `--bootstrap [nboot]` | Percentile 95% CI of the mean; default 2000 resamples, seed 1 |
| `--seed N` | xorshift64 seed for `--bootstrap` |
| `--qq` | Sorted sample vs Φ⁻¹ of Blom positions `(i−0.375)/(n+0.25)` |
| `--kde [bins]` | Gaussian KDE on a linspace of `[min, max]` (default 50 points) |
| `--mwu [file]` | Mann–Whitney U vs a second file, or two columns via `--column2` |
| `--ecdf <x>` | Empirical CDF F(x) = (count of values ≤ x) / n |
| `--acf [lag]` | Autocorrelation at lag; omit lag to print lags 0..min(20, n−1) |
| `--ks [file]` | Two-sample KS D vs a second file, or two columns via `--column2` |
| `--paired` | Paired *t*-test on two columns (`--column` / `--column2`) |

## Web studio

From `web/`:

```sh
cd web
python3 -m http.server
```

Open [http://localhost:8000](http://localhost:8000). Drop a CSV on the
blotter or paste numbers; the desk detects numeric columns, lists Tukey
outliers, and draws histogram (two-channel overlay plus N(mean, s) curve),
KDE, box, Q–Q, ECDF, ACF, residuals, bootstrap means, and scatter on
`<canvas>` with no chart library. Hover a histogram, KDE, ECDF, or scatter
to read coordinates. Drop outliers rebuilds a working copy of the primary
series (original parse is untouched); Log10 skips non-positive values;
Copy markdown dumps the metric cards. Daylight toggle and the last CSV
stick in `localStorage`. Keys: **S** sample, **E** export, **D** daylight.
Export JSON or a printable HTML snapshot. `web/404.html` is a GitHub Pages
404.

`web/stats.js` is an ES module port of the core (descriptive, bivariate,
bootstrap, Mann–Whitney, KDE, Q–Q, ECDF, ACF, KS, paired *t*, normal pdf)
used by the studio and by `node --test`.

## C API highlights

```c
#include "stats_core.h"

StatsSummary s;
stats_summary(values, n, &s);

double lo, hi, mean;
stats_bootstrap_mean_ci(values, n, 2000, 1, 0.95, &lo, &hi, &mean);

double u, p;
stats_mannwhitney(x, nx, y, ny, &u, &p);   /* p is NaN if nx or ny < 8 */

double density;
stats_kde(values, n, x, &density);         /* Scott h = n^{-1/5} s */

double xs[50], dens[50];
stats_kde_grid(values, n, xs, 50, dens);

double sample_q[n], theo_q[n];
stats_normal_qq(values, n, sample_q, theo_q);

double p_t = stats_t_cdf(t, df);
double tcrit = stats_t_critical(0.95, df);

double F;
stats_ecdf(values, n, 4.5, &F);          /* (# <= 4.5) / n */

double acf[21];
stats_acf_series(values, n, 20, acf);    /* lags 0..20; requires 20 < n */

double D;
stats_ks_2samp(x, nx, y, ny, &D);        /* max |F_x − F_y| on pooled t */

double t_rel, df_rel;
stats_ttest_rel(x, y, n, &t_rel, &df_rel); /* paired; df = n-1 */

double dens = stats_normal_pdf(0.0, 0.0, 1.0); /* 0 if sd <= 0 */
```

NaN / error cases follow the existing `StatsStatus` contract and are
documented next to each prototype in [`include/stats_core.h`](include/stats_core.h).

## Test

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build
ctest --test-dir build --output-on-failure
npm test
```

`tests/reference_values_tests.cpp` asserts published definitions (not the
library’s own previous output). The Node tests check that `web/stats.js`
matches those C goldens for mean, median, stddev, Pearson, OLS,
Mann–Whitney, ECDF, ACF, two-sample KS, paired *t*, and the normal pdf
on the shared fixture.

## Layout

```text
include/stats_core.h           C API
src/stats_core.c               C statistics implementation
src/main.cpp                   C++ CLI (`statlab`)
tests/                         C++ unit + reference-value tests
tests/js/                      Node tests for web/stats.js
web/                           StatLab studio (index, CSS, JS, sample, 404)
examples/                      Sample CSVs
```

## License

MIT — see [LICENSE](LICENSE).
