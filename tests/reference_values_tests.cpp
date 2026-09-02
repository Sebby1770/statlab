/*
 * Reference-value tests.
 *
 * Every expectation here is derived from the *published definition* of the
 * statistic (cross-checked against NumPy where one exists), never from what
 * this library happens to produce. That distinction matters: the Jarque-Bera
 * bug fixed in 1.7.0 survived precisely because its test had been written by
 * copying the implementation's own output, so the test agreed with the code
 * and both were wrong together.
 *
 * When a value here disagrees with the library, the library is the suspect.
 */

#include "stats_core.h"

#include <cmath>
#include <cstdio>
#include <string>

namespace {

int g_failures = 0;

void expect_near(const std::string& name, double got, double want, double tol) {
    if (!(std::fabs(got - want) <= tol)) {
        std::printf("FAIL %-28s got=%.10f want=%.10f (tol %.1e)\n",
                    name.c_str(), got, want, tol);
        ++g_failures;
    } else {
        std::printf("ok   %-28s %.10f\n", name.c_str(), got);
    }
}

/* Shared fixture. Deliberately skewed and containing a tie, so ranking and
   moment code cannot pass by accident on symmetric data. */
const double kX[] = {2, 4, 4, 5, 7, 9, 12, 15, 22, 40};
const double kY[] = {1, 3, 5, 4, 9, 8, 15, 13, 25, 35};
const size_t kN = 10;

} // namespace

int main() {
    double a = 0.0, b = 0.0, c = 0.0;

    /* --- Central tendency ------------------------------------------------ */
    StatsSummary s{};
    stats_summary(kX, kN, &s);
    expect_near("mean", s.mean, 12.0, 1e-12);
    expect_near("median", s.median, 8.0, 1e-12);
    /* Population variance = sum((x-mean)^2)/n; sample divides by n-1. */
    expect_near("variance_population", s.variance, 120.4, 1e-10);
    expect_near("variance_sample", s.sample_variance, 133.7777777778, 1e-9);
    expect_near("stddev_population", s.stddev, 10.9726933795, 1e-9);
    expect_near("stddev_sample", s.sample_stddev, 11.5662343819, 1e-9);

    /* Bias-corrected sample moments (Excel SKEW / KURT, R e1071 type 2).
       These are NOT the moments Jarque-Bera is defined on -- see below. */
    expect_near("skewness_bias_corrected", s.skewness, 1.8365263621, 1e-9);
    expect_near("kurtosis_bias_corrected", s.kurtosis, 3.5393480559, 1e-9);

    /* --- Order statistics ------------------------------------------------ */
    /* Linear interpolation between order statistics (NumPy default).
       Note the contract: p is a fraction in [0, 1], over a sorted array.
       kX is already ascending. */
    stats_percentile(kX, kN, 0.25, &a);
    expect_near("p25", a, 4.25, 1e-10);
    stats_percentile(kX, kN, 0.75, &a);
    expect_near("p75", a, 14.25, 1e-10);
    stats_percentile(kX, kN, 0.10, &a);
    expect_near("p10", a, 3.8, 1e-10);
    /* Out-of-range p must be rejected, not silently treated as a percent. */
    if (stats_percentile(kX, kN, 25.0, &a) != STATS_ERR_INVALID) {
        std::printf("FAIL stats_percentile accepted p=25 (expects a fraction)\n");
        ++g_failures;
    }
    /* Percentile rank: (below + 0.5*equal)/n. */
    stats_percentile_rank(kX, kN, 7.0, &a);
    expect_near("percentile_rank(7)", a, 0.45, 1e-12);

    /* --- Paired statistics ----------------------------------------------- */
    stats_correlation(kX, kY, kN, &a);
    expect_near("pearson_r", a, 0.9776678902, 1e-9);
    stats_covariance(kX, kY, kN, 1, &a);
    expect_near("covariance_sample", a, 122.0, 1e-9);
    stats_covariance(kX, kY, kN, 0, &a);
    expect_near("covariance_population", a, 109.8, 1e-9);
    stats_linreg(kX, kY, kN, &a, &b, &c);
    expect_near("ols_slope", a, 0.9119601329, 1e-9);
    expect_near("ols_intercept", b, 0.8564784053, 1e-9);
    expect_near("ols_r2", c, 0.9558345035, 1e-9);

    /* Spearman must use *average* ranks for ties. On data with ties the naive
       6*sum(d^2) shortcut gives 0.5982 and ordinal ranks give 0.6786; only
       Pearson-on-midranks gives the correct 0.5833. */
    {
        const double tx[] = {1, 2, 2, 3, 5, 5, 7};
        const double ty[] = {10, 20, 20, 15, 15, 30, 25};
        stats_spearman(tx, ty, 7, &a);
        expect_near("spearman_with_ties", a, 0.5833333333, 1e-9);
    }

    /* --- Means ----------------------------------------------------------- */
    stats_geometric_mean(kX, kN, &a);
    expect_near("geometric_mean", a, 8.3237987403, 1e-9);
    stats_harmonic_mean(kX, kN, &a);
    expect_near("harmonic_mean", a, 5.9722072606, 1e-9);
    /* floor(n*fraction) trimmed from each tail. */
    stats_trimmed_mean(kX, kN, 0.1, &a);
    expect_near("trimmed_mean(0.1)", a, 9.75, 1e-10);
    stats_trimmed_mean(kX, kN, 0.2, &a);
    expect_near("trimmed_mean(0.2)", a, 8.6666666667, 1e-9);

    /* --- Series ----------------------------------------------------------- */
    {
        double out[16];
        /* ACF normalises by the full-series mean and total sum of squares. */
        stats_acf(kX, kN, 1, &a);
        expect_near("acf_lag1", a, 0.4651162791, 1e-9);
        stats_acf(kX, kN, 2, &a);
        expect_near("acf_lag2", a, 0.2259136213, 1e-9);

        /* ECDF: F_n(x) = (# of sample points <= x) / n. kX has 5 values <= 7
           and 3 values <= 4. */
        stats_ecdf(kX, kN, 7.0, &a);
        expect_near("ecdf(7)", a, 0.5, 1e-12);
        stats_ecdf(kX, kN, 4.0, &a);
        expect_near("ecdf(4)", a, 0.3, 1e-12);
        stats_ecdf(kX, kN, 1.0, &a);
        expect_near("ecdf(1)", a, 0.0, 1e-12);
        stats_ecdf(kX, kN, 40.0, &a);
        expect_near("ecdf(40)", a, 1.0, 1e-12);

        /* ACF series must match the single-lag definition at each lag. */
        stats_acf_series(kX, kN, 2, out);
        expect_near("acf_series_lag0", out[0], 1.0, 1e-12);
        expect_near("acf_series_lag1", out[1], 0.4651162791, 1e-9);
        expect_near("acf_series_lag2", out[2], 0.2259136213, 1e-9);

        /* EMA seeds with the first observation. */
        stats_ema(kX, kN, 0.5, out);
        expect_near("ema_alpha0.5_last", out[kN - 1], 28.58203125, 1e-10);

        stats_moving_average(kX, kN, 3, out);
        expect_near("moving_average_w3_first", out[0], 3.3333333333, 1e-9);
        expect_near("moving_average_w3_last", out[kN - 3], 25.6666666667, 1e-9);

        /* Winsorising at 10% clamps to the 10th/90th percentiles. */
        stats_winsorize(kX, kN, 0.1, out);
        expect_near("winsorize_low", out[0], 3.8, 1e-10);
        expect_near("winsorize_high", out[kN - 1], 23.8, 1e-10);

        /* Robust z uses k = 1/Phi^-1(3/4) so the scaled MAD estimates sigma. */
        stats_robust_zscore(kX, kN, out);
        expect_near("robust_z_first", out[0], -1.0117346253, 1e-9);
    }

    /* --- Hypothesis tests ------------------------------------------------- */
    {
        double t = 0.0, df = 0.0;
        stats_ttest_ind(kX, kN, kY, kN, 1, &t, &df);
        expect_near("welch_t", t, 0.0399857854, 1e-9);
        /* Welch-Satterthwaite df is fractional and well below nx+ny-2. */
        expect_near("welch_df", df, 17.9135682869, 1e-8);
        stats_ttest_ind(kX, kN, kY, kN, 0, &t, &df);
        expect_near("pooled_t", t, 0.0399857854, 1e-9);
        expect_near("pooled_df", df, 18.0, 1e-12);
    }

    /* --- Jarque-Bera: the regression this file exists for ----------------- */
    {
        /* JB is defined on the POPULATION moments (Jarque & Bera 1980;
           scipy.stats.jarque_bera; R tseries::jarque.bera.test):
             S = m3/m2^1.5 = 1.5486950099, K = m4/m2^2 - 3 = 1.4566009205
             JB = n/6 * (S^2 + K^2/4) = 4.8814629901
           Feeding it the bias-corrected moments asserted above instead yields
           10.8409, p = 0.0044 -- which would flip this sample from "cannot
           reject normality" to "reject" at alpha = 0.05. */
        stats_jarque_bera(kX, kN, &a, &b);
        expect_near("jarque_bera", a, 4.8814629901, 1e-9);
        expect_near("jarque_bera_p", b, 0.0870971170, 1e-9);
    }

    /* --- Student-t critical values vs a published table ------------------- */
    expect_near("t_crit(0.95, df=1)", stats_t_critical(0.95, 1.0), 12.7062047362, 1e-6);
    expect_near("t_crit(0.95, df=2)", stats_t_critical(0.95, 2.0), 4.3026527297, 1e-6);
    expect_near("t_crit(0.95, df=9)", stats_t_critical(0.95, 9.0), 2.2621571628, 1e-6);
    expect_near("t_crit(0.95, df=30)", stats_t_critical(0.95, 30.0), 2.0422724563, 1e-6);
    expect_near("t_crit(0.99, df=9)", stats_t_critical(0.99, 9.0), 3.2498355416, 1e-6);

    /* The CI must be the t interval, i.e. strictly wider than the normal one. */
    {
        double mean = 0.0, lo = 0.0, hi = 0.0;
        stats_mean_ci(kX, kN, &mean, &lo, &hi);
        expect_near("mean_ci_low", lo, 3.7260143588, 1e-9);
        expect_near("mean_ci_high", hi, 20.2739856412, 1e-9);
        const double normal_half = 1.959963985 * 11.5662343819 / std::sqrt(10.0);
        if (!((hi - lo) > 2.0 * normal_half)) {
            std::printf("FAIL mean_ci is not wider than the normal interval\n");
            ++g_failures;
        }
    }

    /* --- Student-t CDF (closed forms) ----------------------------------- */
    /* df=1 is Cauchy: F(1) = 1/2 + arctan(1)/π = 3/4. */
    expect_near("t_cdf(1, df=1)", stats_t_cdf(1.0, 1.0), 0.75, 1e-9);
    /* df=2: F(x) = 1/2 + x / (2 sqrt(2+x^2)); F(1) = 1/2 + 1/(2√3). */
    expect_near("t_cdf(1, df=2)", stats_t_cdf(1.0, 2.0),
               0.5 + 1.0 / (2.0 * std::sqrt(3.0)), 1e-9);
    expect_near("t_cdf(tcrit, df=9) is 0.975",
               stats_t_cdf(stats_t_critical(0.95, 9.0), 9.0), 0.975, 1e-6);

    /* --- Mann–Whitney U (midranks, published U formula) ----------------- */
    {
        /* U = n1 n2 + n1(n1+1)/2 − R1. Pooled midranks of kX vs kY:
           R1 = 105.5, so U = 100 + 55 − 105.5 = 49.5.
           Tie sizes {2,2,2,2} give Σ(t³−t) = 42, so
             σ² = 100/12 * ((20+1) − 42/(20*19))
             z  = (49.5 − 50) / σ
             p  = 2 (1 − Φ(|z|)). */
        stats_mannwhitney(kX, kN, kY, kN, &a, &b);
        expect_near("mannwhitney_u", a, 49.5, 1e-12);
        expect_near("mannwhitney_p", b, 0.9697703583, 1e-9);
    }

    /* --- Gaussian KDE, Scott bandwidth n^{-1/5} s ----------------------- */
    {
        /* Classic 8-point sample. s = √(32/7), h = 8^{-1/5} s.
           f(5) = (1/(n h)) Σ φ((5−x_i)/h)  (standard normal kernel). */
        const double classic[] = {2, 4, 4, 4, 5, 5, 7, 9};
        stats_kde(classic, 8, 5.0, &a);
        expect_near("kde_classic_at_mean", a, 0.1704511195, 1e-9);
        stats_kde(kX, kN, 12.0, &a);
        expect_near("kde_kX_at_mean", a, 0.0335623707, 1e-9);
    }

    /* --- Normal Q–Q, Blom positions + inverse erf ----------------------- */
    {
        const double classic[] = {2, 4, 4, 4, 5, 5, 7, 9};
        double sample_q[8], theo_q[8];
        stats_normal_qq(classic, 8, sample_q, theo_q);
        expect_near("qq_sample_first", sample_q[0], 2.0, 1e-12);
        expect_near("qq_sample_last", sample_q[7], 9.0, 1e-12);
        /* p = (1 − 0.375)/(8 + 0.25) = 0.075757...; Φ^{-1}(p). */
        expect_near("qq_theo_first", theo_q[0], -1.4342001597, 1e-9);
        expect_near("qq_theo_last", theo_q[7], 1.4342001597, 1e-9);
    }

    /* --- Two-sample KS: max_t |F_x(t)−F_y(t)|, F(t)=#{v≤t}/n ---------- */
    {
        /* Completely separated samples: the ECDFs differ by 1 at every
           point of the first sample. */
        const double left[] = {1.0, 2.0, 3.0};
        const double right[] = {4.0, 5.0, 6.0};
        stats_ks_2samp(left, 3, right, 3, &a);
        expect_near("ks_separated", a, 1.0, 1e-12);

        /* paired.csv columns: x={1,2,3,4,5}, y={2,4,6,8,10}.
           At t=5, F_x=1 and F_y=2/5, so D = 0.6. */
        const double px[] = {1.0, 2.0, 3.0, 4.0, 5.0};
        const double py[] = {2.0, 4.0, 6.0, 8.0, 10.0};
        stats_ks_2samp(px, 5, py, 5, &a);
        expect_near("ks_paired_csv", a, 0.6, 1e-12);

        /* Identical samples → D = 0. Ties must not inflate D. */
        stats_ks_2samp(kX, kN, kX, kN, &a);
        expect_near("ks_identical", a, 0.0, 1e-12);

        /* kX vs kY: the ECDFs never differ by more than 1/10. */
        stats_ks_2samp(kX, kN, kY, kN, &a);
        expect_near("ks_kX_kY", a, 0.1, 1e-12);
    }

    /* --- Paired t-test: d_i = x_i − y_i, t = mean(d)/(s_d/√n) ---------- */
    {
        double t = 0.0, df = 0.0;
        /* kX − kY = {1,1,-1,1,-2,1,-3,2,-3,5}: mean 0.2, Σ(d-m)² = 55.6,
           s_d = √(55.6/9), t = 6/√556, df = 9. */
        stats_ttest_rel(kX, kY, kN, &t, &df);
        expect_near("ttest_rel_t", t, 0.2544566789, 1e-9);
        expect_near("ttest_rel_df", df, 9.0, 1e-12);

        /* paired.csv: d = {-1,-2,-3,-4,-5}, mean −3, s_d = √2.5,
           t = −3 √5 / √2.5 = −3√2, df = 4. */
        const double px[] = {1.0, 2.0, 3.0, 4.0, 5.0};
        const double py[] = {2.0, 4.0, 6.0, 8.0, 10.0};
        stats_ttest_rel(px, py, 5, &t, &df);
        expect_near("ttest_rel_paired_csv", t, -4.2426406871, 1e-9);
        expect_near("ttest_rel_paired_df", df, 4.0, 1e-12);

        /* Equal series → t = 0. */
        stats_ttest_rel(kX, kX, kN, &t, &df);
        expect_near("ttest_rel_zero", t, 0.0, 1e-12);
    }

    /* --- Normal pdf: φ((x−μ)/σ)/σ; 0 when σ ≤ 0 ----------------------- */
    expect_near("normal_pdf(0,0,1)", stats_normal_pdf(0.0, 0.0, 1.0),
               0.3989422804, 1e-9);
    expect_near("normal_pdf(1,0,1)", stats_normal_pdf(1.0, 0.0, 1.0),
               0.2419707245, 1e-9);
    /* Peak of N(μ, σ²) is 1/(σ √(2π)). */
    expect_near("normal_pdf(5,5,2)", stats_normal_pdf(5.0, 5.0, 2.0),
               0.1994711402, 1e-9);
    expect_near("normal_pdf sd=0", stats_normal_pdf(0.0, 0.0, 0.0), 0.0, 1e-15);
    expect_near("normal_pdf sd<0", stats_normal_pdf(1.0, 0.0, -1.0), 0.0, 1e-15);

    /* --- Bootstrap percentile CI, xorshift64 seed 1, nboot 20 ----------- */
    {
        /* Independent enumeration of Marsaglia xorshift64 (seed 1) over
           {1,2,3} with nboot=20; linear-interpolation 2.5/97.5 percentiles
           of those 20 means. Mean is the sample mean 2. */
        const double tiny[] = {1.0, 2.0, 3.0};
        double lo = 0.0, hi = 0.0, m = 0.0;
        stats_bootstrap_mean_ci(tiny, 3, 20, 1ull, 0.95, &lo, &hi, &m);
        expect_near("bootstrap_mean", m, 2.0, 1e-12);
        expect_near("bootstrap_lo", lo, 1.1583333333, 1e-9);
        expect_near("bootstrap_hi", hi, 2.6666666667, 1e-9);
    }

    if (g_failures == 0) {
        std::printf("ALL REFERENCE VALUES MATCH\n");
        return 0;
    }
    std::printf("%d reference value(s) disagree\n", g_failures);
    return 1;
}
