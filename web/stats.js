/**
 * StatLab statistics module — ES port of the C17 core (descriptive,
 * bivariate, bootstrap, Mann–Whitney, KDE, normal Q–Q).
 * Algorithms follow the published definitions documented in stats_core.h.
 */

const PI = Math.PI;
const INV_SQRT_2PI = 1 / Math.sqrt(2 * PI);
const MAD_SCALE = 1.482602218505602;

/** Portable erf. Taylor series for |x| < 2 (matches libm to ~1e-15 there);
    complementary continued-fraction tail beyond that. */
function erf(x) {
  if (!Number.isFinite(x)) {
    return x;
  }
  const ax = Math.abs(x);
  let y;
  if (ax < 2) {
    let term = x;
    let sum = x;
    const x2 = x * x;
    for (let n = 1; n < 60; n++) {
      term *= -x2 / n;
      sum += term / (2 * n + 1);
      if (Math.abs(term) < 1e-18) {
        break;
      }
    }
    y = (2 / Math.sqrt(PI)) * sum;
  } else {
    /* erfc(x) ~ exp(-x²)/(x√π) * continued fraction */
    const z = ax;
    const t = 1 / (1 + 0.5 * z);
    y =
      1 -
      t *
        Math.exp(
          -z * z -
            1.26551223 +
            t *
              (1.00002368 +
                t *
                  (0.37409196 +
                    t *
                      (0.09678418 +
                        t *
                          (-0.18628806 +
                            t *
                              (0.27886807 +
                                t *
                                  (-1.13520398 +
                                    t * (1.48851587 + t * (-0.82215223 + t * 0.17087277)))))))),
        );
    if (x < 0) {
      y = -y;
    }
  }
  if (y > 1) y = 1;
  if (y < -1) y = -1;
  return y;
}

function requireArray(values, name = "values") {
  if (!Array.isArray(values)) {
    throw new TypeError(`${name} must be an array`);
  }
}

function finiteNumbers(values) {
  requireArray(values);
  const out = [];
  for (const v of values) {
    const n = typeof v === "number" ? v : Number(v);
    if (Number.isFinite(n)) {
      out.push(n);
    }
  }
  return out;
}

function sortedCopy(values) {
  return values.slice().sort((a, b) => a - b);
}

/** Linear-interpolation percentile; p in [0, 1]; values already sorted. */
export function quantileSorted(sorted, p) {
  const n = sorted.length;
  if (n === 0) {
    return NaN;
  }
  if (n === 1) {
    return sorted[0];
  }
  const position = p * (n - 1);
  const lower = Math.floor(position);
  const upper = lower + 1;
  if (upper >= n) {
    return sorted[n - 1];
  }
  const weight = position - lower;
  return sorted[lower] + (sorted[upper] - sorted[lower]) * weight;
}

export function mean(values) {
  const xs = finiteNumbers(values);
  if (xs.length === 0) {
    return NaN;
  }
  let s = 0;
  for (const x of xs) {
    s += x;
  }
  return s / xs.length;
}

export function median(values) {
  const xs = sortedCopy(finiteNumbers(values));
  if (xs.length === 0) {
    return NaN;
  }
  return quantileSorted(xs, 0.5);
}

export function stddev(values, sample = false) {
  const xs = finiteNumbers(values);
  const n = xs.length;
  if (n === 0) {
    return NaN;
  }
  if (n === 1) {
    return 0;
  }
  const m = mean(xs);
  let m2 = 0;
  for (const x of xs) {
    const d = x - m;
    m2 += d * d;
  }
  return Math.sqrt(m2 / (sample ? n - 1 : n));
}

export function summary(values) {
  const xs = finiteNumbers(values);
  const n = xs.length;
  if (n === 0) {
    throw new Error("empty input");
  }
  const sorted = sortedCopy(xs);
  let sum = 0;
  let min = xs[0];
  let max = xs[0];
  for (const x of xs) {
    sum += x;
    if (x < min) min = x;
    if (x > max) max = x;
  }
  const m = sum / n;
  let m2 = 0;
  for (const x of xs) {
    const d = x - m;
    m2 += d * d;
  }
  const variance = m2 / n;
  const sampleVariance = n > 1 ? m2 / (n - 1) : 0;
  const q1 = quantileSorted(sorted, 0.25);
  const q3 = quantileSorted(sorted, 0.75);
  const iqr = q3 - q1;
  const fenceLow = q1 - 1.5 * iqr;
  const fenceHigh = q3 + 1.5 * iqr;
  let outlierCount = 0;
  for (const x of xs) {
    if (x < fenceLow || x > fenceHigh) {
      outlierCount += 1;
    }
  }
  const absDev = sorted.map((x) => Math.abs(x - quantileSorted(sorted, 0.5)));
  absDev.sort((a, b) => a - b);
  return {
    count: n,
    sum,
    min,
    max,
    range: max - min,
    mean: m,
    median: quantileSorted(sorted, 0.5),
    q1,
    q3,
    iqr,
    mad: quantileSorted(absDev, 0.5),
    p10: quantileSorted(sorted, 0.1),
    p90: quantileSorted(sorted, 0.9),
    variance,
    stddev: Math.sqrt(variance),
    sampleVariance,
    sampleStddev: Math.sqrt(sampleVariance),
    fenceLow,
    fenceHigh,
    outlierCount,
    cv: m === 0 ? NaN : Math.sqrt(variance) / m,
  };
}

function pairedMoments(x, y) {
  const n = Math.min(x.length, y.length);
  if (n < 1) {
    throw new Error("empty input");
  }
  let mx = 0;
  let my = 0;
  for (let i = 0; i < n; i++) {
    mx += x[i];
    my += y[i];
  }
  mx /= n;
  my /= n;
  let sxx = 0;
  let syy = 0;
  let sxy = 0;
  for (let i = 0; i < n; i++) {
    const dx = x[i] - mx;
    const dy = y[i] - my;
    sxx += dx * dx;
    syy += dy * dy;
    sxy += dx * dy;
  }
  return { n, mx, my, sxx, syy, sxy };
}

export function pearson(x, y) {
  const xs = finiteNumbers(x);
  const ys = finiteNumbers(y);
  const { n, sxx, syy, sxy } = pairedMoments(xs, ys);
  if (n < 2 || sxx <= 0 || syy <= 0) {
    return NaN;
  }
  return sxy / Math.sqrt(sxx * syy);
}

export function linreg(x, y) {
  const xs = finiteNumbers(x);
  const ys = finiteNumbers(y);
  const { n, mx, my, sxx, syy, sxy } = pairedMoments(xs, ys);
  if (n < 2 || sxx <= 0) {
    return { slope: NaN, intercept: NaN, r2: NaN, n };
  }
  const slope = sxy / sxx;
  const intercept = my - slope * mx;
  const r2 = syy <= 0 ? 1 : (sxy / Math.sqrt(sxx * syy)) ** 2;
  return { slope, intercept, r2, n };
}

export function averageRanks(values) {
  const xs = finiteNumbers(values);
  const n = xs.length;
  const order = xs.map((value, index) => ({ value, index }));
  order.sort((a, b) => (a.value === b.value ? a.index - b.index : a.value - b.value));
  const ranks = new Array(n);
  let i = 0;
  while (i < n) {
    let j = i + 1;
    while (j < n && order[j].value === order[i].value) {
      j += 1;
    }
    const avg = 0.5 * (i + 1 + j);
    for (let k = i; k < j; k++) {
      ranks[order[k].index] = avg;
    }
    i = j;
  }
  return ranks;
}

export function spearman(x, y) {
  const xs = finiteNumbers(x);
  const ys = finiteNumbers(y);
  const n = Math.min(xs.length, ys.length);
  return pearson(averageRanks(xs.slice(0, n)), averageRanks(ys.slice(0, n)));
}

function xorshift64Next(state) {
  let x = BigInt.asUintN(64, state);
  x ^= BigInt.asUintN(64, x << 13n);
  x ^= BigInt.asUintN(64, x >> 7n);
  x ^= BigInt.asUintN(64, x << 17n);
  return BigInt.asUintN(64, x);
}

export function bootstrapMeanCi(values, nboot = 2000, seed = 1n, conf = 0.95) {
  const xs = finiteNumbers(values);
  const n = xs.length;
  if (n === 0 || nboot < 1 || !(conf > 0 && conf < 1)) {
    return { mean: NaN, lo: NaN, hi: NaN };
  }
  const sampleMean = mean(xs);
  let state = BigInt.asUintN(64, typeof seed === "bigint" ? seed : BigInt(seed));
  if (state === 0n) {
    state = 1n;
  }
  const n64 = BigInt(n);
  const boot = new Array(nboot);
  for (let b = 0; b < nboot; b++) {
    let s = 0;
    for (let i = 0; i < n; i++) {
      state = xorshift64Next(state);
      const idx = Number(state % n64);
      s += xs[idx];
    }
    boot[b] = s / n;
  }
  boot.sort((a, b) => a - b);
  const alpha = 0.5 * (1 - conf);
  return {
    mean: sampleMean,
    lo: quantileSorted(boot, alpha),
    hi: quantileSorted(boot, 1 - alpha),
  };
}

export function mannwhitney(x, y) {
  const xs = finiteNumbers(x);
  const ys = finiteNumbers(y);
  const nx = xs.length;
  const ny = ys.length;
  if (nx === 0 || ny === 0) {
    return { u: NaN, p: NaN };
  }
  const pool = xs.concat(ys);
  const ranks = averageRanks(pool);
  let r1 = 0;
  for (let i = 0; i < nx; i++) {
    r1 += ranks[i];
  }
  const u = nx * ny + (nx * (nx + 1)) / 2 - r1;
  if (nx < 8 || ny < 8) {
    return { u, p: NaN };
  }
  const sorted = sortedCopy(pool);
  let tCorr = 0;
  let i = 0;
  const N = sorted.length;
  while (i < N) {
    let j = i + 1;
    while (j < N && sorted[j] === sorted[i]) {
      j += 1;
    }
    const t = j - i;
    tCorr += t * t * t - t;
    i = j;
  }
  let sigma2 = (nx * ny / 12) * (N + 1);
  if (N > 1) {
    sigma2 = (nx * ny / 12) * (N + 1 - tCorr / (N * (N - 1)));
  }
  if (!(sigma2 > 0)) {
    return { u, p: 1 };
  }
  const mu = (nx * ny) / 2;
  const z = (u - mu) / Math.sqrt(sigma2);
  let p = 2 * (1 - 0.5 * (1 + erf(Math.abs(z) / Math.sqrt(2))));
  if (p > 1) p = 1;
  if (p < 0) p = 0;
  return { u, p };
}

function scottBandwidth(xs) {
  const n = xs.length;
  if (n < 2) {
    return { h: 0, degenerate: true, point: xs[0] ?? NaN };
  }
  const s = stddev(xs, true);
  if (!(s > 0)) {
    return { h: 0, degenerate: true, point: xs[0] };
  }
  return { h: n ** (-1 / 5) * s, degenerate: false, point: 0 };
}

export function kde(values, x) {
  const xs = finiteNumbers(values);
  if (xs.length === 0) {
    return NaN;
  }
  const { h, degenerate, point } = scottBandwidth(xs);
  if (degenerate) {
    return x === point ? 1 : 0;
  }
  let acc = 0;
  const invH = 1 / h;
  for (const v of xs) {
    const u = (x - v) * invH;
    acc += INV_SQRT_2PI * Math.exp(-0.5 * u * u);
  }
  return acc / (xs.length * h);
}

export function kdeGrid(values, grid) {
  return grid.map((x) => kde(values, x));
}

function erfinv(y) {
  if (!Number.isFinite(y) || y < -1 || y > 1) {
    return NaN;
  }
  if (y === -1) return -Infinity;
  if (y === 1) return Infinity;
  if (y === 0) return 0;
  const a = 0.147;
  const ln = Math.log(1 - y * y);
  const t = 2 / (PI * a) + 0.5 * ln;
  let x = Math.sign(y) * Math.sqrt(Math.sqrt(t * t - ln / a) - t);
  const twoOverSqrtPi = 2 / Math.sqrt(PI);
  for (let i = 0; i < 8; i++) {
    const err = erf(x) - y;
    const der = twoOverSqrtPi * Math.exp(-x * x);
    if (der === 0) break;
    x -= err / der;
  }
  return x;
}

export function normalQQ(values) {
  const sorted = sortedCopy(finiteNumbers(values));
  const n = sorted.length;
  const sample = sorted.slice();
  const theo = new Array(n);
  for (let i = 0; i < n; i++) {
    const p = (i + 1 - 0.375) / (n + 0.25);
    theo[i] = Math.sqrt(2) * erfinv(2 * p - 1);
  }
  return { sample, theo };
}

export function histogram(values, bins = 10) {
  const xs = finiteNumbers(values);
  const n = xs.length;
  if (n === 0 || bins < 1) {
    return { counts: [], min: NaN, max: NaN };
  }
  let min = xs[0];
  let max = xs[0];
  for (const x of xs) {
    if (x < min) min = x;
    if (x > max) max = x;
  }
  const counts = new Array(bins).fill(0);
  if (min === max) {
    counts[0] = n;
  } else {
    const width = (max - min) / bins;
    for (const x of xs) {
      let bin = Math.floor((x - min) / width);
      if (bin >= bins) bin = bins - 1;
      counts[bin] += 1;
    }
  }
  return { counts, min, max };
}

export function linspace(lo, hi, n) {
  if (n <= 1) {
    return [lo];
  }
  const out = new Array(n);
  for (let i = 0; i < n; i++) {
    out[i] = lo + ((hi - lo) * i) / (n - 1);
  }
  return out;
}

/* Lanczos approximation for ln Γ(z), g = 7. */
function lgamma(z) {
  const p = [
    0.99999999999980993, 676.5203681218851, -1259.1392167224028,
    771.32342877765313, -176.61502916214059, 12.507343278686905,
    -0.13857109526572012, 9.9843695780195716e-6, 1.5056327351493116e-7,
  ];
  if (z < 0.5) {
    return Math.log(PI / Math.sin(PI * z)) - lgamma(1 - z);
  }
  z -= 1;
  let x = p[0];
  for (let i = 1; i < p.length; i++) {
    x += p[i] / (z + i);
  }
  const t = z + 7.5;
  return 0.5 * Math.log(2 * PI) + (z + 0.5) * Math.log(t) - t + Math.log(x);
}

function betacf(a, b, x) {
  const maxIter = 300;
  const eps = 3e-14;
  const fpmin = 1e-300;
  const qab = a + b;
  const qap = a + 1;
  const qam = a - 1;
  let c = 1;
  let d = 1 - (qab * x) / qap;
  if (Math.abs(d) < fpmin) d = fpmin;
  d = 1 / d;
  let h = d;
  for (let m = 1; m <= maxIter; m++) {
    const m2 = 2 * m;
    let aa = (m * (b - m) * x) / ((qam + m2) * (a + m2));
    d = 1 + aa * d;
    if (Math.abs(d) < fpmin) d = fpmin;
    c = 1 + aa / c;
    if (Math.abs(c) < fpmin) c = fpmin;
    d = 1 / d;
    h *= d * c;
    aa = -((a + m) * (qab + m) * x) / ((a + m2) * (qap + m2));
    d = 1 + aa * d;
    if (Math.abs(d) < fpmin) d = fpmin;
    c = 1 + aa / c;
    if (Math.abs(c) < fpmin) c = fpmin;
    d = 1 / d;
    const del = d * c;
    h *= del;
    if (Math.abs(del - 1) < eps) break;
  }
  return h;
}

function betai(a, b, x) {
  if (!(x > 0)) return 0;
  if (x >= 1) return 1;
  const bt = Math.exp(
    lgamma(a + b) - lgamma(a) - lgamma(b) + a * Math.log(x) + b * Math.log1p(-x),
  );
  if (x < (a + 1) / (a + b + 2)) {
    return (bt * betacf(a, b, x)) / a;
  }
  return 1 - (bt * betacf(b, a, 1 - x)) / b;
}

export function tCdf(t, df) {
  if (!(df > 0) || !Number.isFinite(df) || !Number.isFinite(t)) {
    return NaN;
  }
  const x = df / (df + t * t);
  const tail = 0.5 * betai(0.5 * df, 0.5, x);
  return t >= 0 ? 1 - tail : tail;
}

export function tCritical(conf, df) {
  if (!(conf > 0) || !(conf < 1) || !(df > 0) || !Number.isFinite(df)) {
    return NaN;
  }
  const target = 1 - 0.5 * (1 - conf);
  let low = 0;
  let high = 1e4;
  for (let i = 0; i < 200; i++) {
    const mid = 0.5 * (low + high);
    if (tCdf(mid, df) < target) {
      low = mid;
    } else {
      high = mid;
    }
  }
  return 0.5 * (low + high);
}

export function meanCi(values) {
  const xs = finiteNumbers(values);
  const n = xs.length;
  if (n < 2) {
    return { mean: mean(xs), lo: NaN, hi: NaN };
  }
  const m = mean(xs);
  const s = stddev(xs, true);
  const tcrit = tCritical(0.95, n - 1);
  const half = tcrit * s / Math.sqrt(n);
  return { mean: m, lo: m - half, hi: m + half };
}

export function robustZ(values) {
  const xs = finiteNumbers(values);
  const s = summary(xs);
  const scale = MAD_SCALE * s.mad;
  if (scale === 0) {
    return xs.map(() => 0);
  }
  return xs.map((v) => (v - s.median) / scale);
}

export function parseCsv(text) {
  const lines = String(text)
    .replace(/^\uFEFF/, "")
    .split(/\r?\n/)
    .filter((line) => line.trim().length > 0);
  if (lines.length === 0) {
    return { headers: [], rows: [], columns: {} };
  }
  const split = (line) => {
    const fields = [];
    let field = "";
    let inQuotes = false;
    for (let i = 0; i < line.length; i++) {
      const ch = line[i];
      if (inQuotes) {
        if (ch === '"' && line[i + 1] === '"') {
          field += '"';
          i += 1;
        } else if (ch === '"') {
          inQuotes = false;
        } else {
          field += ch;
        }
      } else if (ch === '"') {
        inQuotes = true;
      } else if (ch === "," || ch === ";" || ch === "\t") {
        fields.push(field.trim());
        field = "";
      } else {
        field += ch;
      }
    }
    fields.push(field.trim());
    return fields;
  };

  const first = split(lines[0]);
  const firstNumeric = first.every((cell) => cell !== "" && Number.isFinite(Number(cell)));
  let headers;
  let start = 0;
  if (firstNumeric) {
    headers = first.map((_, i) => `column_${i + 1}`);
  } else {
    headers = first.map((h, i) => h || `column_${i + 1}`);
    start = 1;
  }

  const columns = {};
  for (const h of headers) {
    columns[h] = [];
  }
  const rows = [];
  for (let r = start; r < lines.length; r++) {
    const fields = split(lines[r]);
    const row = {};
    headers.forEach((h, i) => {
      const raw = fields[i] ?? "";
      const num = Number(raw);
      const value = raw !== "" && Number.isFinite(num) ? num : raw;
      columns[h].push(value);
      row[h] = value;
    });
    rows.push(row);
  }

  const numericColumns = headers.filter((h) =>
    columns[h].some((v) => typeof v === "number"),
  );

  return { headers, rows, columns, numericColumns };
}

export function numericColumn(columns, name) {
  const col = columns[name] || [];
  return col.filter((v) => typeof v === "number" && Number.isFinite(v));
}
