import assert from "node:assert/strict";
import { test } from "node:test";
import {
  acf,
  acfSeries,
  bootstrapMeanCi,
  ecdf,
  kde,
  linreg,
  mannwhitney,
  mean,
  median,
  normalQQ,
  pearson,
  spearman,
  stddev,
  summary,
} from "../../web/stats.js";

/* Golden fixture from tests/reference_values_tests.cpp (published defs). */
const kX = [2, 4, 4, 5, 7, 9, 12, 15, 22, 40];
const kY = [1, 3, 5, 4, 9, 8, 15, 13, 25, 35];

test("mean matches C reference", () => {
  assert.ok(Math.abs(mean(kX) - 12) < 1e-12);
});

test("median matches C reference", () => {
  assert.ok(Math.abs(median(kX) - 8) < 1e-12);
});

test("population stddev matches C reference", () => {
  assert.ok(Math.abs(stddev(kX, false) - 10.9726933795) < 1e-9);
});

test("sample stddev matches C reference", () => {
  assert.ok(Math.abs(stddev(kX, true) - 11.5662343819) < 1e-9);
});

test("pearson matches C reference", () => {
  assert.ok(Math.abs(pearson(kX, kY) - 0.9776678902) < 1e-9);
});

test("linreg matches C reference", () => {
  const fit = linreg(kX, kY);
  assert.ok(Math.abs(fit.slope - 0.9119601329) < 1e-9);
  assert.ok(Math.abs(fit.intercept - 0.8564784053) < 1e-9);
  assert.ok(Math.abs(fit.r2 - 0.9558345035) < 1e-9);
});

test("mannwhitney matches C reference (U formula + tie-corrected p)", () => {
  const { u, p } = mannwhitney(kX, kY);
  assert.ok(Math.abs(u - 49.5) < 1e-12);
  assert.ok(Math.abs(p - 0.9697703583) < 1e-8);
});

test("summary fences match the classic eight-point sample", () => {
  const s = summary([2, 4, 4, 4, 5, 5, 7, 9]);
  assert.equal(s.count, 8);
  assert.ok(Math.abs(s.mean - 5) < 1e-12);
  assert.ok(Math.abs(s.median - 4.5) < 1e-12);
  assert.ok(Math.abs(s.iqr - 1.5) < 1e-12);
  assert.ok(Math.abs(s.fenceLow - 1.75) < 1e-12);
  assert.ok(Math.abs(s.fenceHigh - 7.75) < 1e-12);
  assert.equal(s.outlierCount, 1);
});

test("bootstrap mean CI is deterministic xorshift64", () => {
  const { mean: m, lo, hi } = bootstrapMeanCi([1, 2, 3], 20, 1n, 0.95);
  assert.ok(Math.abs(m - 2) < 1e-12);
  assert.ok(Math.abs(lo - 1.1583333333) < 1e-9);
  assert.ok(Math.abs(hi - 2.6666666667) < 1e-9);
});

test("KDE Scott bandwidth at the classic mean", () => {
  const d = kde([2, 4, 4, 4, 5, 5, 7, 9], 5);
  assert.ok(Math.abs(d - 0.1704511195) < 1e-9);
});

test("normal QQ uses Blom positions", () => {
  const qq = normalQQ([2, 4, 4, 4, 5, 5, 7, 9]);
  assert.equal(qq.sample[0], 2);
  assert.equal(qq.sample[7], 9);
  assert.ok(Math.abs(qq.theo[0] + qq.theo[7]) < 1e-10);
});

test("ecdf matches C reference", () => {
  assert.ok(Math.abs(ecdf(kX, 7) - 0.5) < 1e-12);
  assert.ok(Math.abs(ecdf(kX, 4) - 0.3) < 1e-12);
  assert.equal(ecdf(kX, 1), 0);
  assert.equal(ecdf(kX, 40), 1);
  assert.ok(Math.abs(ecdf([5, 1, 3, 2, 4], 3) - 0.6) < 1e-12);
});

test("acfSeries matches C reference", () => {
  const series = acfSeries(kX, 2);
  assert.equal(series.length, 3);
  assert.ok(Math.abs(series[0] - 1) < 1e-12);
  assert.ok(Math.abs(series[1] - 0.4651162791) < 1e-9);
  assert.ok(Math.abs(series[2] - 0.2259136213) < 1e-9);
  assert.ok(Math.abs(acf([1, 2, 3, 4, 5], 1) - 0.4) < 1e-12);
  const short = acfSeries([1, 2, 3, 4, 5], 2);
  assert.ok(Math.abs(short[2] + 0.1) < 1e-12);
});

test("spearman uses midranks on ties (C golden)", () => {
  const tx = [1, 2, 2, 3, 5, 5, 7];
  const ty = [10, 20, 20, 15, 15, 30, 25];
  assert.ok(Math.abs(spearman(tx, ty) - 0.5833333333) < 1e-9);
});
