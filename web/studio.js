import {
  bootstrapMeanCi,
  histogram,
  kde,
  linreg,
  linspace,
  mannwhitney,
  meanCi,
  normalQQ,
  numericColumn,
  parseCsv,
  pearson,
  summary,
} from "./stats.js";

const INK = "#0e1110";
const PAPER = "#e7e1d4";
const COPPER = "#c4843a";
const TEAL = "#2f6f6a";
const PHOSPHOR = "#c6f35a";

const els = {
  file: document.getElementById("file"),
  sample: document.getElementById("sample"),
  fileName: document.getElementById("file-name"),
  colA: document.getElementById("col-a"),
  colB: document.getElementById("col-b"),
  status: document.getElementById("status"),
  metrics: document.getElementById("metrics"),
  compare: document.getElementById("compare"),
  exportJson: document.getElementById("export-json"),
  exportHtml: document.getElementById("export-html"),
  hist: document.getElementById("chart-hist"),
  kde: document.getElementById("chart-kde"),
  box: document.getElementById("chart-box"),
  qq: document.getElementById("chart-qq"),
  scatter: document.getElementById("chart-scatter"),
};

const state = {
  parsed: null,
  sourceName: "bench sample",
  x: [],
  y: [],
  xName: "",
  yName: "",
};

function fmt(n, digits = 4) {
  if (!Number.isFinite(n)) {
    return "—";
  }
  const abs = Math.abs(n);
  if (abs !== 0 && (abs < 0.0001 || abs >= 1e6)) {
    return n.toExponential(3);
  }
  return n.toFixed(digits);
}

function setStatus(text) {
  els.status.textContent = text;
}

function fillSelect(select, names, extra) {
  const current = select.value;
  select.innerHTML = "";
  if (extra) {
    const opt = document.createElement("option");
    opt.value = "";
    opt.textContent = extra;
    select.appendChild(opt);
  }
  for (const name of names) {
    const opt = document.createElement("option");
    opt.value = name;
    opt.textContent = name;
    select.appendChild(opt);
  }
  if (names.includes(current)) {
    select.value = current;
  }
}

function metricCard(label, value, alert = false) {
  const wrap = document.createElement("div");
  wrap.className = alert ? "metric alert" : "metric";
  wrap.innerHTML = `<dt>${label}</dt><dd>${value}</dd>`;
  return wrap;
}

function renderMetrics(s, ci, boot) {
  els.metrics.replaceChildren(
    metricCard("count", String(s.count)),
    metricCard("mean", fmt(s.mean)),
    metricCard("median", fmt(s.median)),
    metricCard("stddev", fmt(s.sampleStddev)),
    metricCard("IQR", fmt(s.iqr)),
    metricCard("outliers", String(s.outlierCount), s.outlierCount > 0),
    metricCard("t CI lo", fmt(ci.lo)),
    metricCard("t CI hi", fmt(ci.hi)),
    metricCard("boot CI lo", fmt(boot.lo)),
    metricCard("boot CI hi", fmt(boot.hi)),
    metricCard("min", fmt(s.min)),
    metricCard("max", fmt(s.max)),
  );
}

function sizeCanvas(canvas) {
  const ratio = Math.max(1, window.devicePixelRatio || 1);
  const width = canvas.clientWidth || 640;
  const height = Number(canvas.dataset.height) || 220;
  canvas.width = Math.round(width * ratio);
  canvas.height = Math.round(height * ratio);
  const ctx = canvas.getContext("2d");
  ctx.setTransform(ratio, 0, 0, ratio, 0, 0);
  return { ctx, width, height };
}

function clearPlot(ctx, width, height) {
  ctx.fillStyle = "#0b0d0c";
  ctx.fillRect(0, 0, width, height);
  ctx.strokeStyle = "rgba(47,111,106,0.35)";
  ctx.lineWidth = 1;
  for (let x = 40; x < width; x += 32) {
    ctx.beginPath();
    ctx.moveTo(x, 12);
    ctx.lineTo(x, height - 28);
    ctx.stroke();
  }
  for (let y = 12; y < height - 28; y += 24) {
    ctx.beginPath();
    ctx.moveTo(36, y);
    ctx.lineTo(width - 12, y);
    ctx.stroke();
  }
}

function axes(ctx, width, height, x0, x1, y0, y1) {
  ctx.strokeStyle = COPPER;
  ctx.lineWidth = 1.2;
  ctx.beginPath();
  ctx.moveTo(x0, y1);
  ctx.lineTo(x1, y1);
  ctx.moveTo(x0, y0);
  ctx.lineTo(x0, y1);
  ctx.stroke();
}

function mapX(v, lo, hi, x0, x1) {
  if (hi <= lo) return (x0 + x1) / 2;
  return x0 + ((v - lo) / (hi - lo)) * (x1 - x0);
}

function mapY(v, lo, hi, y0, y1) {
  if (hi <= lo) return (y0 + y1) / 2;
  return y1 - ((v - lo) / (hi - lo)) * (y1 - y0);
}

function drawHistogram(canvas, values) {
  const { ctx, width, height } = sizeCanvas(canvas);
  clearPlot(ctx, width, height);
  const bins = Math.max(8, Math.min(24, Math.round(Math.sqrt(values.length) * 2)));
  const hist = histogram(values, bins);
  const x0 = 44;
  const y0 = 16;
  const x1 = width - 16;
  const y1 = height - 32;
  axes(ctx, width, height, x0, x1, y0, y1);
  const maxC = Math.max(1, ...hist.counts);
  const bw = (x1 - x0) / hist.counts.length;
  hist.counts.forEach((c, i) => {
    const h = (c / maxC) * (y1 - y0);
    const x = x0 + i * bw;
    ctx.fillStyle = TEAL;
    ctx.strokeStyle = PHOSPHOR;
    ctx.lineWidth = 1;
    ctx.fillRect(x + 1.5, y1 - h, Math.max(1, bw - 3), h);
    ctx.strokeRect(x + 1.5, y1 - h, Math.max(1, bw - 3), h);
  });
  ctx.fillStyle = PAPER;
  ctx.font = "11px 'IBM Plex Mono', monospace";
  ctx.fillText(fmt(hist.min, 2), x0, height - 12);
  ctx.fillText(fmt(hist.max, 2), x1 - 48, height - 12);
}

function drawKDE(canvas, values) {
  const { ctx, width, height } = sizeCanvas(canvas);
  clearPlot(ctx, width, height);
  const s = summary(values);
  const pad = s.range === 0 ? 1 : s.range * 0.08;
  const xs = linspace(s.min - pad, s.max + pad, 160);
  const ys = xs.map((x) => kde(values, x));
  const yMax = Math.max(...ys, 1e-12);
  const x0 = 44;
  const y0 = 16;
  const x1 = width - 16;
  const y1 = height - 32;
  axes(ctx, width, height, x0, x1, y0, y1);
  ctx.beginPath();
  xs.forEach((x, i) => {
    const px = mapX(x, xs[0], xs[xs.length - 1], x0, x1);
    const py = mapY(ys[i], 0, yMax, y0, y1);
    if (i === 0) ctx.moveTo(px, py);
    else ctx.lineTo(px, py);
  });
  ctx.strokeStyle = PHOSPHOR;
  ctx.lineWidth = 2;
  ctx.shadowColor = "rgba(198,243,90,0.35)";
  ctx.shadowBlur = 8;
  ctx.stroke();
  ctx.shadowBlur = 0;
  ctx.lineTo(x1, y1);
  ctx.lineTo(x0, y1);
  ctx.closePath();
  ctx.fillStyle = "rgba(47,111,106,0.28)";
  ctx.fill();
}

function drawBox(canvas, values) {
  const { ctx, width, height } = sizeCanvas(canvas);
  clearPlot(ctx, width, height);
  const s = summary(values);
  const x0 = 36;
  const x1 = width - 20;
  const mid = height / 2;
  let lo = Math.min(s.min, s.fenceLow);
  let hi = Math.max(s.max, s.fenceHigh);
  if (hi <= lo) hi = lo + 1;
  const X = (v) => mapX(v, lo, hi, x0, x1);
  const wLo = Math.max(s.min, s.fenceLow);
  const wHi = Math.min(s.max, s.fenceHigh);
  ctx.strokeStyle = COPPER;
  ctx.lineWidth = 2;
  ctx.beginPath();
  ctx.moveTo(X(wLo), mid);
  ctx.lineTo(X(wHi), mid);
  ctx.moveTo(X(wLo), mid - 16);
  ctx.lineTo(X(wLo), mid + 16);
  ctx.moveTo(X(wHi), mid - 16);
  ctx.lineTo(X(wHi), mid + 16);
  ctx.stroke();
  ctx.fillStyle = TEAL;
  ctx.strokeStyle = PHOSPHOR;
  ctx.lineWidth = 1.5;
  const boxX = X(s.q1);
  const boxW = Math.max(2, X(s.q3) - X(s.q1));
  ctx.fillRect(boxX, mid - 22, boxW, 44);
  ctx.strokeRect(boxX, mid - 22, boxW, 44);
  ctx.beginPath();
  ctx.moveTo(X(s.median), mid - 22);
  ctx.lineTo(X(s.median), mid + 22);
  ctx.stroke();
  ctx.fillStyle = PHOSPHOR;
  values.forEach((v) => {
    if (v < s.fenceLow || v > s.fenceHigh) {
      ctx.beginPath();
      ctx.arc(X(v), mid, 3.2, 0, Math.PI * 2);
      ctx.fill();
    }
  });
}

function drawQQ(canvas, values) {
  const { ctx, width, height } = sizeCanvas(canvas);
  clearPlot(ctx, width, height);
  const qq = normalQQ(values);
  const x0 = 44;
  const y0 = 16;
  const x1 = width - 16;
  const y1 = height - 32;
  const tMin = Math.min(...qq.theo);
  const tMax = Math.max(...qq.theo);
  const sMin = Math.min(...qq.sample);
  const sMax = Math.max(...qq.sample);
  axes(ctx, width, height, x0, x1, y0, y1);
  ctx.strokeStyle = COPPER;
  ctx.setLineDash([4, 4]);
  ctx.beginPath();
  ctx.moveTo(mapX(tMin, tMin, tMax, x0, x1), mapY(sMin, sMin, sMax, y0, y1));
  ctx.lineTo(mapX(tMax, tMin, tMax, x0, x1), mapY(sMax, sMin, sMax, y0, y1));
  ctx.stroke();
  ctx.setLineDash([]);
  ctx.fillStyle = PHOSPHOR;
  qq.theo.forEach((t, i) => {
    ctx.beginPath();
    ctx.arc(
      mapX(t, tMin, tMax, x0, x1),
      mapY(qq.sample[i], sMin, sMax, y0, y1),
      3,
      0,
      Math.PI * 2,
    );
    ctx.fill();
  });
}

function drawScatter(canvas, x, y) {
  const { ctx, width, height } = sizeCanvas(canvas);
  clearPlot(ctx, width, height);
  const n = Math.min(x.length, y.length);
  if (n < 2) {
    ctx.fillStyle = PAPER;
    ctx.font = "13px 'IBM Plex Sans', sans-serif";
    ctx.fillText("Pick a second channel to plot the joint field.", 48, height / 2);
    return;
  }
  const xs = x.slice(0, n);
  const ys = y.slice(0, n);
  const xmin = Math.min(...xs);
  const xmax = Math.max(...xs);
  const ymin = Math.min(...ys);
  const ymax = Math.max(...ys);
  const x0 = 48;
  const y0 = 16;
  const x1 = width - 16;
  const y1 = height - 32;
  axes(ctx, width, height, x0, x1, y0, y1);
  const fit = linreg(xs, ys);
  if (Number.isFinite(fit.slope)) {
    ctx.strokeStyle = COPPER;
    ctx.lineWidth = 1.8;
    ctx.beginPath();
    ctx.moveTo(mapX(xmin, xmin, xmax, x0, x1), mapY(fit.slope * xmin + fit.intercept, ymin, ymax, y0, y1));
    ctx.lineTo(mapX(xmax, xmin, xmax, x0, x1), mapY(fit.slope * xmax + fit.intercept, ymin, ymax, y0, y1));
    ctx.stroke();
  }
  ctx.fillStyle = PHOSPHOR;
  for (let i = 0; i < n; i++) {
    ctx.beginPath();
    ctx.arc(
      mapX(xs[i], xmin, xmax, x0, x1),
      mapY(ys[i], ymin, ymax, y0, y1),
      3.1,
      0,
      Math.PI * 2,
    );
    ctx.fill();
  }
  ctx.fillStyle = PAPER;
  ctx.font = "11px 'IBM Plex Mono', monospace";
  const r = pearson(xs, ys);
  ctx.fillText(`OLS  y = ${fmt(fit.slope, 3)} x + ${fmt(fit.intercept, 3)}   R² ${fmt(fit.r2, 3)}   r ${fmt(r, 3)}`, x0 + 8, 18);
}

function renderCompare(x, y, xName, yName) {
  if (y.length < 2) {
    els.compare.classList.remove("visible");
    els.compare.innerHTML = "";
    return;
  }
  const sx = summary(x);
  const sy = summary(y);
  const mwu = mannwhitney(x, y);
  const r = pearson(x, y);
  const fit = linreg(x, y);
  els.compare.classList.add("visible");
  els.compare.innerHTML = `
    <article class="plate panel">
      <h2>Channel A · ${escapeHtml(xName)}</h2>
      <p class="note">n ${sx.count} · mean ${fmt(sx.mean)} · median ${fmt(sx.median)} · s ${fmt(sx.sampleStddev)}</p>
    </article>
    <article class="plate panel">
      <h2>Channel B · ${escapeHtml(yName)}</h2>
      <p class="note">n ${sy.count} · mean ${fmt(sy.mean)} · median ${fmt(sy.median)} · s ${fmt(sy.sampleStddev)}</p>
      <p class="note">Mann–Whitney U ${fmt(mwu.u, 3)} · p ${Number.isFinite(mwu.p) ? fmt(mwu.p, 4) : "n/a (&lt; 8)"} · Pearson r ${fmt(r, 4)} · slope ${fmt(fit.slope, 4)}</p>
    </article>`;
}

function escapeHtml(s) {
  return String(s)
    .replaceAll("&", "&amp;")
    .replaceAll("<", "&lt;")
    .replaceAll(">", "&gt;")
    .replaceAll('"', "&quot;");
}

function currentPair() {
  if (!state.parsed) return;
  const names = state.parsed.numericColumns;
  const a = els.colA.value || names[0];
  const b = els.colB.value;
  state.xName = a;
  state.yName = b;
  state.x = numericColumn(state.parsed.columns, a);
  state.y = b ? numericColumn(state.parsed.columns, b) : [];
}

function render() {
  currentPair();
  if (state.x.length === 0) {
    setStatus("No numeric values on this channel.");
    return;
  }
  const s = summary(state.x);
  const ci = meanCi(state.x);
  const boot = bootstrapMeanCi(state.x, 800, 1n, 0.95);
  renderMetrics(s, ci, boot);
  drawHistogram(els.hist, state.x);
  drawKDE(els.kde, state.x);
  drawBox(els.box, state.x);
  drawQQ(els.qq, state.x);
  drawScatter(els.scatter, state.x, state.y);
  renderCompare(state.x, state.y, state.xName, state.yName);
  setStatus(
    `${state.sourceName} · ${state.x.length} readings on ${state.xName}` +
      (state.yName ? ` vs ${state.yName}` : ""),
  );
}

function loadParsed(parsed, name) {
  state.parsed = parsed;
  state.sourceName = name;
  const names = parsed.numericColumns;
  if (names.length === 0) {
    setStatus("No numeric columns detected.");
    return;
  }
  fillSelect(els.colA, names);
  fillSelect(els.colB, names, "none — univariate");
  els.colA.value = names[0];
  els.colB.value = names[1] || "";
  render();
}

async function loadSample() {
  const res = await fetch("./sample.csv");
  const text = await res.text();
  loadParsed(parseCsv(text), "phosphor bench sample");
}

function loadFile(file) {
  const reader = new FileReader();
  reader.onload = () => {
    loadParsed(parseCsv(String(reader.result)), file.name);
  };
  reader.readAsText(file);
}

function exportJson() {
  if (state.x.length === 0) return;
  const s = summary(state.x);
  const payload = {
    source: state.sourceName,
    column: state.xName,
    column2: state.yName || null,
    summary: s,
    meanCi: meanCi(state.x),
    bootstrap: bootstrapMeanCi(state.x, 800, 1n, 0.95),
    bivariate: state.y.length
      ? {
          pearson: pearson(state.x, state.y),
          linreg: linreg(state.x, state.y),
          mannwhitney: mannwhitney(state.x, state.y),
        }
      : null,
  };
  const blob = new Blob([JSON.stringify(payload, null, 2)], {
    type: "application/json",
  });
  downloadBlob(blob, "statlab-export.json");
}

function exportHtml() {
  if (state.x.length === 0) return;
  const s = summary(state.x);
  const images = [els.hist, els.kde, els.box, els.qq, els.scatter]
    .map((c) => `<img alt="" src="${c.toDataURL("image/png")}">`)
    .join("");
  const html = `<!DOCTYPE html><html lang="en"><head><meta charset="utf-8">
<title>StatLab snapshot</title>
<style>
body{margin:0;background:${INK};color:${PAPER};font-family:"IBM Plex Sans",sans-serif;padding:2rem;}
h1{font-family:Syne,sans-serif;letter-spacing:.12em;text-transform:uppercase;color:${PHOSPHOR};}
table{border-collapse:collapse;width:min(640px,100%);}
td,th{border-bottom:1px solid #24302e;padding:.4rem .6rem;text-align:left;}
img{width:100%;max-width:640px;display:block;margin:1rem 0;background:#0b0d0c;}
</style></head><body>
<h1>StatLab snapshot</h1>
<p>${escapeHtml(state.sourceName)} · ${escapeHtml(state.xName)}</p>
<table><tbody>
<tr><th>count</th><td>${s.count}</td></tr>
<tr><th>mean</th><td>${fmt(s.mean)}</td></tr>
<tr><th>median</th><td>${fmt(s.median)}</td></tr>
<tr><th>stddev</th><td>${fmt(s.sampleStddev)}</td></tr>
<tr><th>IQR</th><td>${fmt(s.iqr)}</td></tr>
<tr><th>outliers</th><td>${s.outlierCount}</td></tr>
</tbody></table>
${images}
</body></html>`;
  downloadBlob(new Blob([html], { type: "text/html" }), "statlab-snapshot.html");
}

function downloadBlob(blob, name) {
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = name;
  a.click();
  URL.revokeObjectURL(a.href);
}

els.sample.addEventListener("click", () => {
  loadSample().catch((err) => setStatus(err.message));
});
els.file.addEventListener("change", () => {
  const file = els.file.files && els.file.files[0];
  if (file) {
    els.fileName.textContent = file.name;
    loadFile(file);
  }
});
els.colA.addEventListener("change", render);
els.colB.addEventListener("change", render);
els.exportJson.addEventListener("click", exportJson);
els.exportHtml.addEventListener("click", exportHtml);
window.addEventListener("resize", () => {
  if (state.x.length) {
    render();
  }
});

document.addEventListener("keydown", (event) => {
  if (event.target && ["INPUT", "SELECT", "BUTTON", "TEXTAREA"].includes(event.target.tagName)) {
    return;
  }
  if (event.key === "s") {
    event.preventDefault();
    loadSample();
  }
  if (event.key === "e") {
    event.preventDefault();
    exportJson();
  }
});

loadSample().catch((err) => setStatus(err.message));
