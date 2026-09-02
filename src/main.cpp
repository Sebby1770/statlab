#include "stats_core.h"

#include <cctype>
#include <cerrno>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

/* Single source of truth is the CMake project version, injected as
   STATLAB_VERSION. The fallback only applies to ad-hoc builds that
   compile this file without CMake. */
#ifndef STATLAB_VERSION
#define STATLAB_VERSION "0.0.0-dev"
#endif
constexpr const char *kVersion = STATLAB_VERSION;

enum class OutputFormat { Text, Json, Csv, Html };

struct CliOptions {
  OutputFormat format = OutputFormat::Text;
  int precision = 4;
  std::vector<double> values;
  std::vector<double> values_y; /* second series for correlate / regression */
  std::vector<double> compare_values;
  bool has_compare = false;
  bool histogram = false;
  size_t histogram_bins = 10;
  int column = 0;  /* 0 = all / flattened stream; >0 = 1-based column index */
  int column2 = 0; /* second 1-based column for paired analysis */
  bool skip_header = false;
  std::vector<std::string> file_paths;
  bool correlate = false;
  std::string correlate_path; /* optional second file for --correlate */
  bool has_correlate_path = false;
  bool regression = false;
  bool use_trim = false;
  double trim_fraction = 0.0;
  bool outliers = false;
  bool boxplot = false;
  /* v1.4 */
  bool spearman = false;
  bool cov = false;
  bool cov_sample = true; /* default sample covariance */
  bool rolling = false;
  size_t rolling_window = 0;
  bool ema = false;
  double ema_alpha = 0.0;
  bool ci = false;
  bool geomean = false;
  /* v1.5 */
  bool winsor = false;
  double winsor_limits = 0.0;
  bool entropy = false;
  size_t entropy_bins = 10;
  bool acf = false;
  bool acf_lag_given = false;
  size_t acf_lag = 1;
  bool ecdf = false;
  double ecdf_x = 0.0;
  bool robust_z = false;
  bool nunique = false;
  bool jb = false;
  bool show_rms = false;
  bool show_diff = false;
  bool show_cumsum = false;
  bool show_ranks = false;
  bool show_argmin = false;
  bool show_argmax = false;
  /* v2.0 */
  bool write_html = false;
  std::string html_path;
  bool bootstrap = false;
  size_t bootstrap_nboot = 2000;
  uint64_t seed = 1;
  bool mwu = false;
  std::string mwu_path;
  bool has_mwu_path = false;
  bool qq = false;
  bool kde = false;
  size_t kde_bins = 50;
  /* v2.2 */
  bool ks = false;
  std::string ks_path;
  bool has_ks_path = false;
  bool paired = false;
};

void print_help() {
  std::cout
      << "statlab - calculate summary statistics for numeric data\n\n"
      << "Usage:\n"
      << "  statlab 1 2 3 4\n"
      << "  statlab --file examples/sample.csv\n"
      << "  statlab --format json --precision 2 1,2,3,4\n"
      << "  statlab --column 2 --skip-header --file data.csv\n"
      << "  statlab --compare other.csv --file data.csv\n"
      << "  statlab --correlate other.csv --file data.csv\n"
      << "  statlab --column 1 --column2 2 --correlate --file data.csv\n"
      << "  statlab --column 1 --column2 2 --regression --file data.csv\n"
      << "  statlab --column 1 --column2 2 --spearman --file data.csv\n"
      << "  statlab --column 1 --column2 2 --cov --file data.csv\n"
      << "  statlab --rolling 3 1 2 3 4 5 6\n"
      << "  statlab --ema 0.5 1 2 3 4 5\n"
      << "  statlab --ci --geomean 1 2 3 4 5\n"
      << "  statlab --winsor 0.05 --entropy 8 --jb --nunique 1 2 3 4 5\n"
      << "  statlab --acf 1 1 2 3 2 1\n"
      << "  statlab --acf --format csv 1 2 3 4 5 6\n"
      << "  statlab --ecdf 4.5 1 2 3 4 5 6 7 8 9\n"
      << "  statlab --robust-z 1 2 3 4 5 100\n"
      << "  statlab --trim 0.1 --file data.csv\n"
      << "  statlab --outliers --boxplot 1 2 3 4 5 100\n"
      << "  statlab --histogram 12 1 2 3 4 5\n"
      << "  statlab --bootstrap 2000 --seed 1 1 2 3 4 5\n"
      << "  statlab --qq --kde 40 1 2 3 4 5 6 7 8\n"
      << "  statlab --mwu other.csv --file data.csv\n"
      << "  statlab --ks other.csv --file data.csv\n"
      << "  statlab --column 1 --column2 2 --ks --file data.csv\n"
      << "  statlab --column 1 --column2 2 --paired --file data.csv\n"
      << "  statlab --html report.html --file data.csv\n"
      << "  printf '1,2,3,4' | statlab --format csv\n\n"
      << "Options:\n"
      << "  -f, --file <path>       Read numbers from a file\n"
      << "      --format <type>     Output as text, json, csv, or html\n"
      << "      --precision <n>     Decimal places for numeric output (0-12)\n"
      << "      --column <n>        Use 1-based column N from CSV/TSV files\n"
      << "      --column2 <n>       Second 1-based column (paired analyses)\n"
      << "      --skip-header       Skip the first line of each input file\n"
      << "      --compare <path>    Compare primary data with a second file\n"
      << "      --correlate [path]  Pearson r vs second file, or two columns\n"
      << "      --spearman [path]   Spearman rank correlation (same pairing)\n"
      << "      --cov [path]        Covariance (sample by default; use "
         "--cov-pop for population)\n"
      << "      --cov-pop           With --cov: population covariance (÷ n)\n"
      << "      --regression        OLS slope / intercept / R² (needs two "
         "series)\n"
      << "      --rolling <N>       Print simple moving average (window N; "
         "length n-N+1)\n"
      << "      --ema <alpha>       Print exponential moving average "
         "(alpha in (0,1])\n"
      << "      --ci                Include 95% CI for the mean (Student t)\n"
      << "      --geomean           Include geometric / harmonic means when "
         "all values > 0\n"
      << "      --trim <f>          Report trimmed mean (fraction per tail, "
         "0–0.5)\n"
      << "      --winsor <f>        Winsorize tails (fraction per side, "
         "0–0.5); report winsorized mean\n"
      << "      --entropy [bins]   Shannon entropy of histogram (default "
         "bins: 10)\n"
      << "      --acf [lag]        Autocorrelation at integer lag; omit lag "
         "to print lags 0..min(20, n-1) as CSV rows\n"
      << "      --ecdf <x>          Empirical CDF F(x) = (# values <= x) / n\n"
      << "      --robust-z         Print robust (MAD-scaled) z-score series\n"
      << "      --nunique          Include unique-value count in the report\n"
      << "      --jb               Include Jarque–Bera statistic in the report\n"
      << "      --rms              Include root-mean-square in the report\n"
      << "      --diff             Print first differences of the series\n"
      << "      --cumsum           Print cumulative sum of the series\n"
      << "      --ranks            Print average ranks (1-based midranks)\n"
      << "      --argmin           Print index of the minimum value\n"
      << "      --argmax           Print index of the maximum value\n"
      << "      --outliers          List / count Tukey fence outliers\n"
      << "      --boxplot           ASCII box-and-whisker plot on stderr\n"
      << "      --histogram [bins]  Print an ASCII histogram (default bins: 10)\n"
      << "      --bootstrap [nboot] Bootstrap 95% CI for the mean (default "
         "nboot: 2000)\n"
      << "      --seed <N>          RNG seed for --bootstrap (default: 1)\n"
      << "      --mwu [path]        Mann–Whitney U vs second file, or two "
         "columns\n"
      << "      --qq                Sample vs theoretical normal quantiles\n"
      << "      --kde [bins]        Gaussian KDE on a linspace of the range "
         "(default: 50)\n"
      << "      --ks [path]         Two-sample KS D vs second file, or two "
         "columns via --column2\n"
      << "      --paired            Paired t-test on two columns "
         "(--column / --column2)\n"
      << "      --html <path>       Write a self-contained HTML report\n"
      << "      --version           Show the program version\n"
      << "  -h, --help              Show this help text\n";
}

void print_version() { std::cout << "statlab " << kVersion << '\n'; }

bool looks_like_option(const std::string &arg) {
  if (arg.size() < 2 || arg[0] != '-') {
    return false;
  }

  const unsigned char next = static_cast<unsigned char>(arg[1]);
  return arg[1] == '-' || (!std::isdigit(next) && arg[1] != '.');
}

bool is_separator(char ch) {
  return ch == ',' || ch == ';' || ch == '\n' || ch == '\r' || ch == '\t' ||
         ch == ' ';
}

double parse_number(const std::string &token, const std::string &source) {
  char *end = nullptr;
  errno = 0;
  const double value = std::strtod(token.c_str(), &end);

  if (errno != 0 || end == token.c_str() || *end != '\0') {
    throw std::runtime_error("Could not parse number '" + token + "' from " +
                             source);
  }

  if (!std::isfinite(value)) {
    throw std::runtime_error("Non-finite number '" + token +
                             "' is not allowed");
  }

  return value;
}

/* Try to parse a number; return false without throwing if the token is bad. */
bool try_parse_number(const std::string &token, double *out) {
  if (token.empty() || out == nullptr) {
    return false;
  }

  char *end = nullptr;
  errno = 0;
  const double value = std::strtod(token.c_str(), &end);

  if (errno != 0 || end == token.c_str() || *end != '\0') {
    return false;
  }

  if (!std::isfinite(value)) {
    return false;
  }

  *out = value;
  return true;
}

std::vector<double> parse_stream(std::istream &input,
                                 const std::string &source) {
  std::vector<double> values;
  std::string token;
  char ch = '\0';

  while (input.get(ch)) {
    if (is_separator(ch)) {
      if (!token.empty()) {
        values.push_back(parse_number(token, source));
        token.clear();
      }
      continue;
    }

    token.push_back(ch);
  }

  if (!token.empty()) {
    values.push_back(parse_number(token, source));
  }

  return values;
}

/* Split a single line into fields on comma, semicolon, or tab. */
std::vector<std::string> split_fields(const std::string &line) {
  std::vector<std::string> fields;
  std::string field;

  for (char ch : line) {
    if (ch == ',' || ch == ';' || ch == '\t') {
      fields.push_back(field);
      field.clear();
    } else if (ch == '\r') {
      continue;
    } else {
      field.push_back(ch);
    }
  }
  fields.push_back(field);
  return fields;
}

std::vector<double> parse_stream_column(std::istream &input,
                                        const std::string &source, int column,
                                        bool skip_header) {
  if (column < 1) {
    throw std::runtime_error("--column must be a 1-based column index (>= 1)");
  }

  std::vector<double> values;
  std::string line;
  bool first_line = true;
  const size_t col_index = static_cast<size_t>(column - 1);

  while (std::getline(input, line)) {
    if (first_line) {
      first_line = false;
      if (skip_header) {
        continue;
      }
    }

    if (line.empty()) {
      continue;
    }

    const std::vector<std::string> fields = split_fields(line);
    if (col_index >= fields.size()) {
      continue; /* short row — skip */
    }

    double value = 0.0;
    if (!try_parse_number(fields[col_index], &value)) {
      continue; /* non-numeric cell — skip */
    }
    values.push_back(value);
  }

  if (values.empty()) {
    throw std::runtime_error("No numeric values found in column " +
                             std::to_string(column) + " of " + source);
  }

  return values;
}

/* Load two columns as paired series (rows where both cells are numeric). */
void parse_stream_two_columns(std::istream &input, const std::string &source,
                              int column, int column2, bool skip_header,
                              std::vector<double> &out_x,
                              std::vector<double> &out_y) {
  if (column < 1 || column2 < 1) {
    throw std::runtime_error(
        "--column and --column2 must be 1-based column indices (>= 1)");
  }

  out_x.clear();
  out_y.clear();
  std::string line;
  bool first_line = true;
  const size_t c1 = static_cast<size_t>(column - 1);
  const size_t c2 = static_cast<size_t>(column2 - 1);

  while (std::getline(input, line)) {
    if (first_line) {
      first_line = false;
      if (skip_header) {
        continue;
      }
    }

    if (line.empty()) {
      continue;
    }

    const std::vector<std::string> fields = split_fields(line);
    if (c1 >= fields.size() || c2 >= fields.size()) {
      continue;
    }

    double vx = 0.0;
    double vy = 0.0;
    if (!try_parse_number(fields[c1], &vx) ||
        !try_parse_number(fields[c2], &vy)) {
      continue;
    }
    out_x.push_back(vx);
    out_y.push_back(vy);
  }

  if (out_x.empty()) {
    throw std::runtime_error(
        "No paired numeric values found in columns " + std::to_string(column) +
        " and " + std::to_string(column2) + " of " + source);
  }
}

std::vector<double> parse_stream_with_options(std::istream &input,
                                              const std::string &source,
                                              int column, bool skip_header) {
  if (column > 0) {
    return parse_stream_column(input, source, column, skip_header);
  }

  if (skip_header) {
    std::string header;
    std::getline(input, header);
  }

  return parse_stream(input, source);
}

std::vector<double> load_file(const std::string &path, int column,
                              bool skip_header) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("Could not open file: " + path);
  }
  return parse_stream_with_options(file, path, column, skip_header);
}

void load_file_two_columns(const std::string &path, int column, int column2,
                           bool skip_header, std::vector<double> &out_x,
                           std::vector<double> &out_y) {
  std::ifstream file(path);
  if (!file) {
    throw std::runtime_error("Could not open file: " + path);
  }
  parse_stream_two_columns(file, path, column, column2, skip_header, out_x,
                           out_y);
}

void append_values(std::vector<double> &target, std::vector<double> source) {
  target.insert(target.end(), source.begin(), source.end());
}

OutputFormat parse_format(const std::string &value) {
  if (value == "text") {
    return OutputFormat::Text;
  }
  if (value == "json") {
    return OutputFormat::Json;
  }
  if (value == "csv") {
    return OutputFormat::Csv;
  }
  if (value == "html") {
    return OutputFormat::Html;
  }
  throw std::runtime_error("Unsupported format '" + value +
                           "'. Use text, json, csv, or html.");
}

int parse_precision(const std::string &value) {
  char *end = nullptr;
  errno = 0;
  const long precision = std::strtol(value.c_str(), &end, 10);

  if (errno != 0 || end == value.c_str() || *end != '\0' || precision < 0 ||
      precision > 12) {
    throw std::runtime_error("Precision must be an integer from 0 to 12");
  }

  return static_cast<int>(precision);
}

int parse_positive_int(const std::string &value, const std::string &name) {
  char *end = nullptr;
  errno = 0;
  const long n = std::strtol(value.c_str(), &end, 10);

  if (errno != 0 || end == value.c_str() || *end != '\0' || n < 1 ||
      n > static_cast<long>(std::numeric_limits<int>::max())) {
    throw std::runtime_error(name + " must be a positive integer");
  }

  return static_cast<int>(n);
}

double parse_trim_fraction(const std::string &value) {
  char *end = nullptr;
  errno = 0;
  const double f = std::strtod(value.c_str(), &end);

  if (errno != 0 || end == value.c_str() || *end != '\0' || !std::isfinite(f) ||
      f < 0.0 || f >= 0.5) {
    throw std::runtime_error(
        "--trim requires a fraction in [0, 0.5), e.g. 0.1");
  }

  return f;
}

CliOptions parse_args(int argc, char **argv) {
  CliOptions options;
  bool saw_input = false;
  std::string compare_path;

  if (argc == 1) {
    append_values(options.values,
                  parse_stream_with_options(std::cin, "stdin", 0, false));
    return options;
  }

  for (int i = 1; i < argc; ++i) {
    const std::string arg = argv[i];

    if (arg == "-h" || arg == "--help") {
      print_help();
      std::exit(0);
    }

    if (arg == "--version") {
      print_version();
      std::exit(0);
    }

    if (arg == "-f" || arg == "--file") {
      if (i + 1 >= argc) {
        throw std::runtime_error(arg + " requires a file path");
      }
      options.file_paths.push_back(argv[++i]);
      saw_input = true;
      continue;
    }

    if (arg == "--format") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--format requires text, json, csv, or html");
      }
      options.format = parse_format(argv[++i]);
      continue;
    }

    if (arg == "--precision") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--precision requires a value from 0 to 12");
      }
      options.precision = parse_precision(argv[++i]);
      continue;
    }

    if (arg == "--column") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--column requires a 1-based column index");
      }
      options.column = parse_positive_int(argv[++i], "--column");
      continue;
    }

    if (arg == "--column2") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--column2 requires a 1-based column index");
      }
      options.column2 = parse_positive_int(argv[++i], "--column2");
      continue;
    }

    if (arg == "--skip-header") {
      options.skip_header = true;
      continue;
    }

    if (arg == "--compare") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--compare requires a file path");
      }
      compare_path = argv[++i];
      options.has_compare = true;
      continue;
    }

    if (arg == "--correlate") {
      options.correlate = true;
      /* Optional second-file path. */
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        options.correlate_path = argv[++i];
        options.has_correlate_path = true;
      }
      continue;
    }

    if (arg == "--regression") {
      options.regression = true;
      continue;
    }

    if (arg == "--spearman") {
      options.spearman = true;
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        /* Optional second-file path (same pattern as --correlate). */
        options.correlate_path = argv[++i];
        options.has_correlate_path = true;
      }
      continue;
    }

    if (arg == "--cov") {
      options.cov = true;
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        options.correlate_path = argv[++i];
        options.has_correlate_path = true;
      }
      continue;
    }

    if (arg == "--cov-pop") {
      options.cov_sample = false;
      continue;
    }

    if (arg == "--rolling") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--rolling requires a positive window size");
      }
      options.rolling_window =
          static_cast<size_t>(parse_positive_int(argv[++i], "--rolling"));
      options.rolling = true;
      continue;
    }

    if (arg == "--ema") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--ema requires alpha in (0, 1]");
      }
      char *end = nullptr;
      errno = 0;
      const double a = std::strtod(argv[++i], &end);
      if (errno != 0 || end == argv[i] || *end != '\0' || !std::isfinite(a) ||
          a <= 0.0 || a > 1.0) {
        throw std::runtime_error("--ema requires alpha in (0, 1]");
      }
      options.ema_alpha = a;
      options.ema = true;
      continue;
    }

    if (arg == "--ci") {
      options.ci = true;
      continue;
    }

    if (arg == "--geomean") {
      options.geomean = true;
      continue;
    }

    if (arg == "--trim") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--trim requires a fraction in [0, 0.5)");
      }
      options.trim_fraction = parse_trim_fraction(argv[++i]);
      options.use_trim = true;
      continue;
    }

    if (arg == "--winsor") {
      if (i + 1 >= argc) {
        throw std::runtime_error(
            "--winsor requires a fraction in [0, 0.5), e.g. 0.05");
      }
      options.winsor_limits = parse_trim_fraction(argv[++i]);
      options.winsor = true;
      continue;
    }

    if (arg == "--entropy") {
      options.entropy = true;
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        const std::string maybe = argv[i + 1];
        char *end = nullptr;
        errno = 0;
        const long n = std::strtol(maybe.c_str(), &end, 10);
        if (errno == 0 && end != maybe.c_str() && *end == '\0' && n >= 1 &&
            n <= 10000 && maybe.find('.') == std::string::npos) {
          options.entropy_bins = static_cast<size_t>(n);
          ++i;
        }
      }
      continue;
    }

    if (arg == "--acf") {
      options.acf = true;
      options.acf_lag = 1;
      options.acf_lag_given = false;
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        const std::string maybe = argv[i + 1];
        char *end = nullptr;
        errno = 0;
        const long n = std::strtol(maybe.c_str(), &end, 10);
        if (errno == 0 && end != maybe.c_str() && *end == '\0' && n >= 0 &&
            n <= static_cast<long>(std::numeric_limits<int>::max()) &&
            maybe.find('.') == std::string::npos) {
          options.acf_lag = static_cast<size_t>(n);
          options.acf_lag_given = true;
          ++i;
        }
      }
      continue;
    }

    if (arg == "--ecdf") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--ecdf requires a value x");
      }
      options.ecdf_x = parse_number(argv[++i], "--ecdf");
      options.ecdf = true;
      continue;
    }

    if (arg == "--robust-z") {
      options.robust_z = true;
      continue;
    }

    if (arg == "--nunique") {
      options.nunique = true;
      continue;
    }

    if (arg == "--jb") {
      options.jb = true;
      continue;
    }

    if (arg == "--rms") {
      options.show_rms = true;
      continue;
    }

    if (arg == "--diff") {
      options.show_diff = true;
      continue;
    }

    if (arg == "--cumsum") {
      options.show_cumsum = true;
      continue;
    }

    if (arg == "--ranks") {
      options.show_ranks = true;
      continue;
    }

    if (arg == "--argmin") {
      options.show_argmin = true;
      continue;
    }

    if (arg == "--argmax") {
      options.show_argmax = true;
      continue;
    }

    if (arg == "--outliers") {
      options.outliers = true;
      continue;
    }

    if (arg == "--boxplot") {
      options.boxplot = true;
      continue;
    }

    if (arg == "--histogram") {
      options.histogram = true;
      /* Optional bins argument: present and not another option. */
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        /* Only treat as bins if it looks like a positive integer, not data. */
        const std::string maybe = argv[i + 1];
        char *end = nullptr;
        errno = 0;
        const long n = std::strtol(maybe.c_str(), &end, 10);
        if (errno == 0 && end != maybe.c_str() && *end == '\0' && n >= 1 &&
            n <= 10000 && maybe.find('.') == std::string::npos) {
          options.histogram_bins = static_cast<size_t>(n);
          ++i;
        }
      }
      continue;
    }

    if (arg == "--html") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--html requires an output file path");
      }
      options.html_path = argv[++i];
      options.write_html = true;
      continue;
    }

    if (arg == "--bootstrap") {
      options.bootstrap = true;
      options.bootstrap_nboot = 2000;
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        const std::string maybe = argv[i + 1];
        char *end = nullptr;
        errno = 0;
        const long n = std::strtol(maybe.c_str(), &end, 10);
        if (errno == 0 && end != maybe.c_str() && *end == '\0' && n >= 1 &&
            n <= 1000000 && maybe.find('.') == std::string::npos) {
          options.bootstrap_nboot = static_cast<size_t>(n);
          ++i;
        }
      }
      continue;
    }

    if (arg == "--seed") {
      if (i + 1 >= argc) {
        throw std::runtime_error("--seed requires a non-negative integer");
      }
      const std::string value = argv[++i];
      char *end = nullptr;
      errno = 0;
      const unsigned long long n = std::strtoull(value.c_str(), &end, 10);
      if (errno != 0 || end == value.c_str() || *end != '\0') {
        throw std::runtime_error("--seed requires a non-negative integer");
      }
      options.seed = static_cast<uint64_t>(n);
      continue;
    }

    if (arg == "--mwu") {
      options.mwu = true;
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        options.mwu_path = argv[++i];
        options.has_mwu_path = true;
      }
      continue;
    }

    if (arg == "--qq") {
      options.qq = true;
      continue;
    }

    if (arg == "--kde") {
      options.kde = true;
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        const std::string maybe = argv[i + 1];
        char *end = nullptr;
        errno = 0;
        const long n = std::strtol(maybe.c_str(), &end, 10);
        if (errno == 0 && end != maybe.c_str() && *end == '\0' && n >= 1 &&
            n <= 10000 && maybe.find('.') == std::string::npos) {
          options.kde_bins = static_cast<size_t>(n);
          ++i;
        }
      }
      continue;
    }

    if (arg == "--ks") {
      options.ks = true;
      if (i + 1 < argc && !looks_like_option(argv[i + 1])) {
        options.ks_path = argv[++i];
        options.has_ks_path = true;
      }
      continue;
    }

    if (arg == "--paired") {
      options.paired = true;
      continue;
    }

    if (looks_like_option(arg)) {
      throw std::runtime_error("Unknown option: " + arg);
    }

    std::istringstream literal_stream(arg);
    saw_input = true;
    append_values(options.values,
                  parse_stream(literal_stream, "command line"));
  }

  const bool need_two_cols =
      options.column2 > 0 &&
      (options.correlate || options.regression || options.spearman ||
       options.cov || options.mwu || options.ks || options.paired ||
       options.write_html || options.format == OutputFormat::Html);

  /* Load files after options are fully known (column / skip-header). */
  if (need_two_cols) {
    if (options.column < 1) {
      throw std::runtime_error(
          "--column2 requires --column for paired column analysis");
    }
    if (options.file_paths.empty()) {
      /* stdin two-column */
      parse_stream_two_columns(std::cin, "stdin", options.column,
                               options.column2, options.skip_header,
                               options.values, options.values_y);
      saw_input = true;
    } else {
      for (const std::string &path : options.file_paths) {
        std::vector<double> x;
        std::vector<double> y;
        load_file_two_columns(path, options.column, options.column2,
                              options.skip_header, x, y);
        append_values(options.values, std::move(x));
        append_values(options.values_y, std::move(y));
      }
    }
  } else {
    for (const std::string &path : options.file_paths) {
      append_values(options.values,
                    load_file(path, options.column, options.skip_header));
    }
  }

  if (options.has_compare) {
    options.compare_values =
        load_file(compare_path, options.column, options.skip_header);
  }

  if (options.has_correlate_path) {
    options.values_y = load_file(options.correlate_path, options.column,
                                 options.skip_header);
  }

  if (options.has_mwu_path) {
    options.values_y =
        load_file(options.mwu_path, options.column, options.skip_header);
  }

  if (options.has_ks_path) {
    options.values_y =
        load_file(options.ks_path, options.column, options.skip_header);
  }

  if (!saw_input && !options.has_compare && options.values.empty()) {
    append_values(options.values,
                  parse_stream_with_options(std::cin, "stdin", options.column,
                                            options.skip_header));
  }

  return options;
}

void print_number(std::ostream &os, double value, int precision) {
  if (std::isnan(value)) {
    os << "nan";
    return;
  }
  if (std::isinf(value)) {
    os << (value > 0 ? "inf" : "-inf");
    return;
  }
  os << std::fixed << std::setprecision(precision) << value;
}

void print_json_number(std::ostream &os, double value, int precision) {
  if (std::isnan(value) || std::isinf(value)) {
    os << "null";
    return;
  }
  os << std::fixed << std::setprecision(precision) << value;
}

struct ExtraSummary {
  bool use_trim = false;
  double trimmed_mean = 0.0;
  bool use_ci = false;
  double ci_lo = 0.0;
  double ci_hi = 0.0;
  bool use_geomean = false;
  double geometric_mean = 0.0;
  double harmonic_mean = 0.0;
  /* v1.5 extras */
  bool use_winsor = false;
  double winsorized_mean = 0.0;
  bool use_entropy = false;
  double entropy = 0.0;
  bool use_acf = false;
  double acf = 0.0;
  size_t acf_lag = 1;
  bool use_ecdf = false;
  double ecdf = 0.0;
  double ecdf_x = 0.0;
  bool use_nunique = false;
  size_t nunique = 0;
  bool use_jb = false;
  double jarque_bera = 0.0;
  double jarque_bera_p = 0.0;
  /* v1.6 */
  bool use_rms = false;
  double rms = 0.0;
  bool use_argmin = false;
  size_t argmin = 0;
  bool use_argmax = false;
  size_t argmax = 0;
  /* v2.0 */
  bool use_bootstrap = false;
  size_t bootstrap_nboot = 0;
  double bootstrap_lo = 0.0;
  double bootstrap_hi = 0.0;
};

void print_text(const StatsSummary &summary, int precision,
                const ExtraSummary &extra) {
  auto line = [&](const char *name, double value) {
    std::cout << std::left << std::setw(17) << (std::string(name) + ":")
              << std::right;
    print_number(std::cout, value, precision);
    std::cout << '\n';
  };

  std::cout << "statlab report\n";
  std::cout << std::left << std::setw(17) << "count:" << std::right
            << summary.count << '\n';
  if (extra.use_nunique) {
    std::cout << std::left << std::setw(17) << "nunique:" << std::right
              << extra.nunique << '\n';
  }
  line("sum", summary.sum);
  line("min", summary.min);
  line("max", summary.max);
  line("range", summary.range);
  line("mean", summary.mean);
  if (extra.use_ci) {
    line("mean_ci_low", extra.ci_lo);
    line("mean_ci_high", extra.ci_hi);
  }
  if (extra.use_bootstrap) {
    std::cout << std::left << std::setw(17) << "bootstrap_nboot:" << std::right
              << extra.bootstrap_nboot << '\n';
    line("bootstrap_ci_low", extra.bootstrap_lo);
    line("bootstrap_ci_high", extra.bootstrap_hi);
  }
  if (extra.use_trim) {
    line("trimmed_mean", extra.trimmed_mean);
  }
  if (extra.use_winsor) {
    line("winsorized_mean", extra.winsorized_mean);
  }
  if (extra.use_geomean) {
    line("geometric_mean", extra.geometric_mean);
    line("harmonic_mean", extra.harmonic_mean);
  }
  line("median", summary.median);
  line("q1/p25", summary.q1);
  line("q3/p75", summary.q3);
  line("iqr", summary.iqr);
  line("fence_low", summary.fence_low);
  line("fence_high", summary.fence_high);
  std::cout << std::left << std::setw(17) << "outlier_count:" << std::right
            << summary.outlier_count << '\n';
  line("mad", summary.mad);
  line("mode", summary.mode);
  line("p10", summary.p10);
  line("p90", summary.p90);
  line("variance", summary.variance);
  line("stddev", summary.stddev);
  line("sample_variance", summary.sample_variance);
  line("sample_stddev", summary.sample_stddev);
  line("skewness", summary.skewness);
  line("kurtosis", summary.kurtosis);
  line("cv", summary.cv);
  if (extra.use_entropy) {
    line("entropy", extra.entropy);
  }
  if (extra.use_acf) {
    line("acf", extra.acf);
  }
  if (extra.use_ecdf) {
    line("ecdf_x", extra.ecdf_x);
    line("ecdf", extra.ecdf);
  }
  if (extra.use_jb) {
    line("jarque_bera", extra.jarque_bera);
    line("jarque_bera_p", extra.jarque_bera_p);
  }
  if (extra.use_rms) {
    line("rms", extra.rms);
  }
  if (extra.use_argmin) {
    std::cout << std::left << std::setw(17) << "argmin:" << std::right
              << extra.argmin << '\n';
  }
  if (extra.use_argmax) {
    std::cout << std::left << std::setw(17) << "argmax:" << std::right
              << extra.argmax << '\n';
  }
}

void print_json(const StatsSummary &summary, int precision,
                const ExtraSummary &extra) {
  auto field = [&](const char *name, double value, bool last = false) {
    std::cout << "  \"" << name << "\": ";
    print_json_number(std::cout, value, precision);
    if (!last) {
      std::cout << ',';
    }
    std::cout << '\n';
  };

  std::cout << "{\n";
  std::cout << "  \"count\": " << summary.count << ",\n";
  if (extra.use_nunique) {
    std::cout << "  \"nunique\": " << extra.nunique << ",\n";
  }
  field("sum", summary.sum);
  field("min", summary.min);
  field("max", summary.max);
  field("range", summary.range);
  field("mean", summary.mean);
  if (extra.use_ci) {
    field("mean_ci_low", extra.ci_lo);
    field("mean_ci_high", extra.ci_hi);
  }
  if (extra.use_bootstrap) {
    std::cout << "  \"bootstrap_nboot\": " << extra.bootstrap_nboot << ",\n";
    field("bootstrap_ci_low", extra.bootstrap_lo);
    field("bootstrap_ci_high", extra.bootstrap_hi);
  }
  if (extra.use_trim) {
    field("trimmed_mean", extra.trimmed_mean);
  }
  if (extra.use_winsor) {
    field("winsorized_mean", extra.winsorized_mean);
  }
  if (extra.use_geomean) {
    field("geometric_mean", extra.geometric_mean);
    field("harmonic_mean", extra.harmonic_mean);
  }
  field("median", summary.median);
  field("q1", summary.q1);
  field("q3", summary.q3);
  field("p25", summary.q1);
  field("p75", summary.q3);
  field("iqr", summary.iqr);
  field("fence_low", summary.fence_low);
  field("fence_high", summary.fence_high);
  std::cout << "  \"outlier_count\": " << summary.outlier_count << ",\n";
  field("mad", summary.mad);
  field("mode", summary.mode);
  field("p10", summary.p10);
  field("p90", summary.p90);
  field("variance", summary.variance);
  field("stddev", summary.stddev);
  field("sample_variance", summary.sample_variance);
  field("sample_stddev", summary.sample_stddev);
  field("skewness", summary.skewness);
  field("kurtosis", summary.kurtosis);
  const bool more = extra.use_entropy || extra.use_acf || extra.use_ecdf ||
                    extra.use_jb || extra.use_rms || extra.use_argmin ||
                    extra.use_argmax;
  field("cv", summary.cv, !more);
  if (extra.use_entropy) {
    const bool last = !extra.use_acf && !extra.use_ecdf && !extra.use_jb &&
                      !extra.use_rms && !extra.use_argmin && !extra.use_argmax;
    field("entropy", extra.entropy, last);
  }
  if (extra.use_acf) {
    field("acf", extra.acf,
          !extra.use_ecdf && !extra.use_jb && !extra.use_rms &&
              !extra.use_argmin && !extra.use_argmax);
  }
  if (extra.use_ecdf) {
    field("ecdf_x", extra.ecdf_x);
    field("ecdf", extra.ecdf,
          !extra.use_jb && !extra.use_rms && !extra.use_argmin &&
              !extra.use_argmax);
  }
  if (extra.use_jb) {
    field("jarque_bera", extra.jarque_bera);
    field("jarque_bera_p", extra.jarque_bera_p,
          !extra.use_rms && !extra.use_argmin && !extra.use_argmax);
  }
  if (extra.use_rms) {
    field("rms", extra.rms, !extra.use_argmin && !extra.use_argmax);
  }
  if (extra.use_argmin) {
    std::cout << "  \"argmin\": " << extra.argmin
              << (extra.use_argmax ? ",\n" : "\n");
  }
  if (extra.use_argmax) {
    std::cout << "  \"argmax\": " << extra.argmax << "\n";
  }
  std::cout << "}\n";
}

void print_csv(const StatsSummary &summary, int precision,
               const ExtraSummary &extra) {
  auto row = [&](const char *name, double value) {
    std::cout << name << ',';
    print_number(std::cout, value, precision);
    std::cout << '\n';
  };

  std::cout << "metric,value\n";
  std::cout << "count," << summary.count << '\n';
  if (extra.use_nunique) {
    std::cout << "nunique," << extra.nunique << '\n';
  }
  row("sum", summary.sum);
  row("min", summary.min);
  row("max", summary.max);
  row("range", summary.range);
  row("mean", summary.mean);
  if (extra.use_ci) {
    row("mean_ci_low", extra.ci_lo);
    row("mean_ci_high", extra.ci_hi);
  }
  if (extra.use_bootstrap) {
    std::cout << "bootstrap_nboot," << extra.bootstrap_nboot << '\n';
    row("bootstrap_ci_low", extra.bootstrap_lo);
    row("bootstrap_ci_high", extra.bootstrap_hi);
  }
  if (extra.use_trim) {
    row("trimmed_mean", extra.trimmed_mean);
  }
  if (extra.use_winsor) {
    row("winsorized_mean", extra.winsorized_mean);
  }
  if (extra.use_geomean) {
    row("geometric_mean", extra.geometric_mean);
    row("harmonic_mean", extra.harmonic_mean);
  }
  row("median", summary.median);
  row("q1", summary.q1);
  row("q3", summary.q3);
  row("p25", summary.q1);
  row("p75", summary.q3);
  row("iqr", summary.iqr);
  row("fence_low", summary.fence_low);
  row("fence_high", summary.fence_high);
  std::cout << "outlier_count," << summary.outlier_count << '\n';
  row("mad", summary.mad);
  row("mode", summary.mode);
  row("p10", summary.p10);
  row("p90", summary.p90);
  row("variance", summary.variance);
  row("stddev", summary.stddev);
  row("sample_variance", summary.sample_variance);
  row("sample_stddev", summary.sample_stddev);
  row("skewness", summary.skewness);
  row("kurtosis", summary.kurtosis);
  row("cv", summary.cv);
  if (extra.use_entropy) {
    row("entropy", extra.entropy);
  }
  if (extra.use_acf) {
    row("acf", extra.acf);
  }
  if (extra.use_ecdf) {
    row("ecdf_x", extra.ecdf_x);
    row("ecdf", extra.ecdf);
  }
  if (extra.use_jb) {
    row("jarque_bera", extra.jarque_bera);
    row("jarque_bera_p", extra.jarque_bera_p);
  }
  if (extra.use_rms) {
    row("rms", extra.rms);
  }
  if (extra.use_argmin) {
    std::cout << "argmin," << extra.argmin << "\n";
  }
  if (extra.use_argmax) {
    std::cout << "argmax," << extra.argmax << "\n";
  }
}

void print_summary(const StatsSummary &summary, OutputFormat format,
                   int precision, const ExtraSummary &extra) {
  switch (format) {
  case OutputFormat::Text:
    print_text(summary, precision, extra);
    return;
  case OutputFormat::Json:
    print_json(summary, precision, extra);
    return;
  case OutputFormat::Csv:
    print_csv(summary, precision, extra);
    return;
  case OutputFormat::Html:
    return;
  }
}

void print_histogram(const std::vector<double> &values, size_t bins,
                     int precision) {
  if (values.empty() || bins == 0) {
    return;
  }

  std::vector<size_t> counts(bins, 0);
  double min = 0.0;
  double max = 0.0;
  const StatsStatus status =
      stats_histogram(values.data(), values.size(), bins, counts.data(), &min,
                      &max);

  if (status != STATS_OK) {
    std::cerr << "Could not build histogram: " << stats_status_message(status)
              << ".\n";
    return;
  }

  size_t max_count = 0;
  for (size_t c : counts) {
    if (c > max_count) {
      max_count = c;
    }
  }

  constexpr size_t kBarWidth = 40;
  std::cerr << "\nhistogram (" << bins << " bins, n=" << values.size() << ")\n";
  std::cerr << std::fixed << std::setprecision(precision);

  const double width =
      (max > min) ? (max - min) / static_cast<double>(bins) : 0.0;

  for (size_t i = 0; i < bins; ++i) {
    const double lo = min + width * static_cast<double>(i);
    const double hi =
        (i + 1 == bins) ? max : (min + width * static_cast<double>(i + 1));

    std::cerr << '[';
    print_number(std::cerr, lo, precision);
    std::cerr << ", ";
    print_number(std::cerr, hi, precision);
    std::cerr << (i + 1 == bins ? ']' : ')');
    std::cerr << "  " << std::setw(6) << counts[i] << " |";

    size_t bar = 0;
    if (max_count > 0) {
      bar = (counts[i] * kBarWidth) / max_count;
    }
    for (size_t b = 0; b < bar; ++b) {
      std::cerr << '#';
    }
    std::cerr << '\n';
  }
}

/* ASCII box-and-whisker on stderr. */
void print_boxplot(const StatsSummary &summary, int precision) {
  std::cerr << "\nboxplot (Tukey)\n";
  std::cerr << "  whisker_low ≈ max(min, fence_low); whisker_high ≈ "
               "min(max, fence_high)\n";

  const double w_lo =
      summary.min > summary.fence_low ? summary.min : summary.fence_low;
  const double w_hi =
      summary.max < summary.fence_high ? summary.max : summary.fence_high;

  auto num = [&](double v) {
    print_number(std::cerr, v, precision);
  };

  std::cerr << "  min=";
  num(summary.min);
  std::cerr << "  fence_low=";
  num(summary.fence_low);
  std::cerr << "  q1=";
  num(summary.q1);
  std::cerr << "  median=";
  num(summary.median);
  std::cerr << "  q3=";
  num(summary.q3);
  std::cerr << "  fence_high=";
  num(summary.fence_high);
  std::cerr << "  max=";
  num(summary.max);
  std::cerr << "  outliers=" << summary.outlier_count << '\n';

  /* Scale a fixed-width ASCII plot over [min, max] or fences if wider. */
  double lo = summary.min;
  double hi = summary.max;
  if (summary.fence_low < lo) {
    lo = summary.fence_low;
  }
  if (summary.fence_high > hi) {
    hi = summary.fence_high;
  }
  if (hi <= lo) {
    hi = lo + 1.0;
  }

  constexpr int kWidth = 50;
  auto col = [&](double v) -> int {
    int c = static_cast<int>(std::llround((v - lo) / (hi - lo) * (kWidth - 1)));
    if (c < 0) {
      c = 0;
    }
    if (c >= kWidth) {
      c = kWidth - 1;
    }
    return c;
  };

  const int c_wlo = col(w_lo);
  const int c_q1 = col(summary.q1);
  const int c_med = col(summary.median);
  const int c_q3 = col(summary.q3);
  const int c_whi = col(w_hi);

  std::string line(static_cast<size_t>(kWidth), ' ');
  for (int i = c_wlo; i <= c_whi; ++i) {
    line[static_cast<size_t>(i)] = '-';
  }
  for (int i = c_q1; i <= c_q3; ++i) {
    line[static_cast<size_t>(i)] = '=';
  }
  line[static_cast<size_t>(c_wlo)] = '|';
  line[static_cast<size_t>(c_whi)] = '|';
  line[static_cast<size_t>(c_q1)] = '[';
  line[static_cast<size_t>(c_q3)] = ']';
  line[static_cast<size_t>(c_med)] = '+';

  std::cerr << "  " << line << '\n';
  std::cerr << "  |---- whiskers ----|  [==== box ====]  + median\n";
}

void print_outliers(const std::vector<double> &values,
                    const StatsSummary &summary, int precision,
                    OutputFormat format) {
  std::vector<double> outs;
  for (double v : values) {
    if (v < summary.fence_low || v > summary.fence_high) {
      outs.push_back(v);
    }
  }

  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"outlier_count\": " << outs.size()
              << ",\n  \"fence_low\": ";
    print_json_number(std::cout, summary.fence_low, precision);
    std::cout << ",\n  \"fence_high\": ";
    print_json_number(std::cout, summary.fence_high, precision);
    std::cout << ",\n  \"outliers\": [";
    for (size_t i = 0; i < outs.size(); ++i) {
      if (i > 0) {
        std::cout << ", ";
      }
      print_json_number(std::cout, outs[i], precision);
    }
    std::cout << "]\n}\n";
    return;
  }

  if (format == OutputFormat::Csv) {
    std::cout << "metric,value\n";
    std::cout << "outlier_count," << outs.size() << '\n';
    std::cout << "fence_low,";
    print_number(std::cout, summary.fence_low, precision);
    std::cout << "\nfence_high,";
    print_number(std::cout, summary.fence_high, precision);
    std::cout << '\n';
    for (size_t i = 0; i < outs.size(); ++i) {
      std::cout << "outlier,";
      print_number(std::cout, outs[i], precision);
      std::cout << '\n';
    }
    return;
  }

  std::cout << "Tukey outliers\n";
  std::cout << "fence_low:       ";
  print_number(std::cout, summary.fence_low, precision);
  std::cout << "\nfence_high:      ";
  print_number(std::cout, summary.fence_high, precision);
  std::cout << "\noutlier_count:   " << outs.size() << '\n';
  if (outs.empty()) {
    std::cout << "(none)\n";
  } else {
    std::cout << "values:";
    for (double v : outs) {
      std::cout << ' ';
      print_number(std::cout, v, precision);
    }
    std::cout << '\n';
  }
}

void print_correlation(double r, size_t n, int precision, OutputFormat format) {
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n\": " << n << ",\n  \"pearson_r\": ";
    print_json_number(std::cout, r, precision);
    std::cout << "\n}\n";
    return;
  }
  if (format == OutputFormat::Csv) {
    std::cout << "metric,value\nn," << n << "\npearson_r,";
    print_number(std::cout, r, precision);
    std::cout << '\n';
    return;
  }
  std::cout << "Pearson correlation\n";
  std::cout << "n:               " << n << '\n';
  std::cout << "pearson_r:       ";
  print_number(std::cout, r, precision);
  std::cout << '\n';
}

void print_spearman(double r, size_t n, int precision, OutputFormat format) {
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n\": " << n << ",\n  \"spearman_rho\": ";
    print_json_number(std::cout, r, precision);
    std::cout << "\n}\n";
    return;
  }
  if (format == OutputFormat::Csv) {
    std::cout << "metric,value\nn," << n << "\nspearman_rho,";
    print_number(std::cout, r, precision);
    std::cout << '\n';
    return;
  }
  std::cout << "Spearman rank correlation\n";
  std::cout << "n:               " << n << '\n';
  std::cout << "spearman_rho:    ";
  print_number(std::cout, r, precision);
  std::cout << '\n';
}

void print_covariance(double cov, size_t n, bool sample, int precision,
                      OutputFormat format) {
  const char *label = sample ? "sample_covariance" : "population_covariance";
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n\": " << n << ",\n  \"" << label << "\": ";
    print_json_number(std::cout, cov, precision);
    std::cout << "\n}\n";
    return;
  }
  if (format == OutputFormat::Csv) {
    std::cout << "metric,value\nn," << n << '\n' << label << ',';
    print_number(std::cout, cov, precision);
    std::cout << '\n';
    return;
  }
  std::cout << (sample ? "Sample covariance\n" : "Population covariance\n");
  std::cout << "n:               " << n << '\n';
  std::cout << std::left << std::setw(17) << (std::string(label) + ":")
            << std::right;
  print_number(std::cout, cov, precision);
  std::cout << '\n';
}

void print_acf_series(const std::vector<double> &series, int precision,
                      OutputFormat format) {
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n\": " << series.size() << ",\n  \"values\": [\n";
    for (size_t i = 0; i < series.size(); ++i) {
      std::cout << "    {\"lag\": " << i << ", \"acf\": ";
      print_json_number(std::cout, series[i], precision);
      std::cout << "}";
      if (i + 1 != series.size()) {
        std::cout << ',';
      }
      std::cout << '\n';
    }
    std::cout << "  ]\n}\n";
    return;
  }
  std::cout << "lag,acf\n";
  for (size_t i = 0; i < series.size(); ++i) {
    std::cout << i << ',';
    print_number(std::cout, series[i], precision);
    std::cout << '\n';
  }
}

void print_series(const char *title, const char *value_name,
                  const std::vector<double> &series, int precision,
                  OutputFormat format) {
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"name\": \"" << title << "\",\n  \"n\": " << series.size()
              << ",\n  \"values\": [";
    for (size_t i = 0; i < series.size(); ++i) {
      if (i > 0) {
        std::cout << ", ";
      }
      print_json_number(std::cout, series[i], precision);
    }
    std::cout << "]\n}\n";
    return;
  }
  if (format == OutputFormat::Csv) {
    std::cout << "index," << value_name << '\n';
    for (size_t i = 0; i < series.size(); ++i) {
      std::cout << i << ',';
      print_number(std::cout, series[i], precision);
      std::cout << '\n';
    }
    return;
  }
  std::cout << title << " (n=" << series.size() << ")\n";
  for (size_t i = 0; i < series.size(); ++i) {
    std::cout << std::setw(6) << i << ": ";
    print_number(std::cout, series[i], precision);
    std::cout << '\n';
  }
}

void print_regression(double slope, double intercept, double r2, size_t n,
                      int precision, OutputFormat format) {
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n\": " << n << ",\n  \"slope\": ";
    print_json_number(std::cout, slope, precision);
    std::cout << ",\n  \"intercept\": ";
    print_json_number(std::cout, intercept, precision);
    std::cout << ",\n  \"r2\": ";
    print_json_number(std::cout, r2, precision);
    std::cout << "\n}\n";
    return;
  }
  if (format == OutputFormat::Csv) {
    std::cout << "metric,value\nn," << n << "\nslope,";
    print_number(std::cout, slope, precision);
    std::cout << "\nintercept,";
    print_number(std::cout, intercept, precision);
    std::cout << "\nr2,";
    print_number(std::cout, r2, precision);
    std::cout << '\n';
    return;
  }
  std::cout << "Linear regression (OLS)\n";
  std::cout << "n:               " << n << '\n';
  std::cout << "slope:           ";
  print_number(std::cout, slope, precision);
  std::cout << "\nintercept:       ";
  print_number(std::cout, intercept, precision);
  std::cout << "\nR²:              ";
  print_number(std::cout, r2, precision);
  std::cout << '\n';
}

void print_compare(const StatsSummary &a, const StatsSummary &b, int precision,
                   OutputFormat format) {
  auto metric = [&](const char *name, double left, double right) {
    if (format == OutputFormat::Json) {
      return; /* handled below */
    }
    if (format == OutputFormat::Csv) {
      std::cout << name << ',';
      print_number(std::cout, left, precision);
      std::cout << ',';
      print_number(std::cout, right, precision);
      std::cout << '\n';
      return;
    }
    std::cout << std::left << std::setw(10) << name << std::right;
    std::cout << std::setw(14);
    print_number(std::cout, left, precision);
    std::cout << std::setw(14);
    print_number(std::cout, right, precision);
    std::cout << '\n';
  };

  if (format == OutputFormat::Json) {
    std::cout << "{\n";
    std::cout << "  \"a\": {\n";
    std::cout << "    \"count\": " << a.count << ",\n";
    std::cout << "    \"mean\": ";
    print_json_number(std::cout, a.mean, precision);
    std::cout << ",\n    \"median\": ";
    print_json_number(std::cout, a.median, precision);
    std::cout << ",\n    \"stddev\": ";
    print_json_number(std::cout, a.stddev, precision);
    std::cout << "\n  },\n  \"b\": {\n";
    std::cout << "    \"count\": " << b.count << ",\n";
    std::cout << "    \"mean\": ";
    print_json_number(std::cout, b.mean, precision);
    std::cout << ",\n    \"median\": ";
    print_json_number(std::cout, b.median, precision);
    std::cout << ",\n    \"stddev\": ";
    print_json_number(std::cout, b.stddev, precision);
    std::cout << "\n  }\n}\n";
    return;
  }

  if (format == OutputFormat::Csv) {
    std::cout << "metric,a,b\n";
    std::cout << "count," << a.count << ',' << b.count << '\n';
    metric("mean", a.mean, b.mean);
    metric("median", a.median, b.median);
    metric("stddev", a.stddev, b.stddev);
    return;
  }

  std::cout << "statlab compare\n";
  std::cout << std::left << std::setw(10) << "metric" << std::right
            << std::setw(14) << "a" << std::setw(14) << "b" << '\n';
  std::cout << std::left << std::setw(10) << "count" << std::right
            << std::setw(14) << a.count << std::setw(14) << b.count << '\n';
  metric("mean", a.mean, b.mean);
  metric("median", a.median, b.median);
  metric("stddev", a.stddev, b.stddev);
}

std::string html_escape(const std::string &s) {
  std::string out;
  out.reserve(s.size());
  for (char ch : s) {
    switch (ch) {
    case '&':
      out += "&amp;";
      break;
    case '<':
      out += "&lt;";
      break;
    case '>':
      out += "&gt;";
      break;
    case '"':
      out += "&quot;";
      break;
    default:
      out.push_back(ch);
    }
  }
  return out;
}

std::string format_fixed(double value, int precision) {
  if (std::isnan(value)) {
    return "nan";
  }
  if (std::isinf(value)) {
    return value > 0 ? "inf" : "-inf";
  }
  std::ostringstream os;
  os << std::fixed << std::setprecision(precision) << value;
  return os.str();
}

std::string build_ascii_histogram(const std::vector<double> &values,
                                  size_t bins, int precision) {
  std::ostringstream os;
  if (values.empty() || bins == 0) {
    return "";
  }
  std::vector<size_t> counts(bins, 0);
  double min = 0.0;
  double max = 0.0;
  if (stats_histogram(values.data(), values.size(), bins, counts.data(), &min,
                      &max) != STATS_OK) {
    return "";
  }
  size_t max_count = 0;
  for (size_t c : counts) {
    if (c > max_count) {
      max_count = c;
    }
  }
  constexpr size_t kBarWidth = 40;
  const double width = (max > min) ? (max - min) / static_cast<double>(bins) : 0.0;
  os << std::fixed << std::setprecision(precision);
  os << "histogram (" << bins << " bins, n=" << values.size() << ")\n";
  for (size_t i = 0; i < bins; ++i) {
    const double lo = min + width * static_cast<double>(i);
    const double hi =
        (i + 1 == bins) ? max : (min + width * static_cast<double>(i + 1));
    os << '[' << format_fixed(lo, precision) << ", "
       << format_fixed(hi, precision) << (i + 1 == bins ? ']' : ')');
    os << "  " << std::setw(6) << counts[i] << " |";
    size_t bar = 0;
    if (max_count > 0) {
      bar = (counts[i] * kBarWidth) / max_count;
    }
    os << std::string(bar, '#') << '\n';
  }
  return os.str();
}

double svg_x(double v, double lo, double hi, double x0, double x1) {
  if (hi <= lo) {
    return 0.5 * (x0 + x1);
  }
  double t = (v - lo) / (hi - lo);
  if (t < 0.0) {
    t = 0.0;
  }
  if (t > 1.0) {
    t = 1.0;
  }
  return x0 + t * (x1 - x0);
}

std::string svg_histogram(const std::vector<double> &values, size_t bins) {
  std::ostringstream os;
  if (values.empty() || bins == 0) {
    return "";
  }
  std::vector<size_t> counts(bins, 0);
  double min = 0.0;
  double max = 0.0;
  if (stats_histogram(values.data(), values.size(), bins, counts.data(), &min,
                      &max) != STATS_OK) {
    return "";
  }
  size_t max_count = 1;
  for (size_t c : counts) {
    if (c > max_count) {
      max_count = c;
    }
  }
  const double x0 = 48.0;
  const double y0 = 16.0;
  const double x1 = 620.0;
  const double y1 = 168.0;
  const double bw = (x1 - x0) / static_cast<double>(bins);
  os << "<svg viewBox=\"0 0 640 200\" role=\"img\" aria-label=\"Histogram\">";
  os << "<rect x=\"0\" y=\"0\" width=\"640\" height=\"200\" fill=\"#0e1110\"/>";
  os << "<line x1=\"" << x0 << "\" y1=\"" << y1 << "\" x2=\"" << x1 << "\" y2=\""
     << y1 << "\" stroke=\"#c4843a\" stroke-width=\"1.2\"/>";
  for (size_t i = 0; i < bins; ++i) {
    const double h =
        (static_cast<double>(counts[i]) / static_cast<double>(max_count)) *
        (y1 - y0);
    const double x = x0 + bw * static_cast<double>(i);
    os << "<rect x=\"" << x + 1.5 << "\" y=\"" << (y1 - h)
       << "\" width=\"" << (bw - 3.0) << "\" height=\"" << h
       << "\" fill=\"#2f6f6a\" stroke=\"#c6f35a\" stroke-width=\"0.8\"/>";
  }
  os << "<text x=\"" << x0 << "\" y=\"192\" fill=\"#e7e1d4\" font-size=\"11\" "
        "font-family=\"IBM Plex Mono, monospace\">"
     << format_fixed(min, 2) << "</text>";
  os << "<text x=\"" << (x1 - 40) << "\" y=\"192\" fill=\"#e7e1d4\" "
        "font-size=\"11\" font-family=\"IBM Plex Mono, monospace\">"
     << format_fixed(max, 2) << "</text>";
  os << "</svg>";
  return os.str();
}

std::string svg_boxplot(const StatsSummary &s) {
  std::ostringstream os;
  const double w_lo = s.min > s.fence_low ? s.min : s.fence_low;
  const double w_hi = s.max < s.fence_high ? s.max : s.fence_high;
  double lo = s.min < s.fence_low ? s.min : s.fence_low;
  double hi = s.max > s.fence_high ? s.max : s.fence_high;
  if (hi <= lo) {
    hi = lo + 1.0;
  }
  const double x0 = 40.0;
  const double x1 = 600.0;
  auto X = [&](double v) { return svg_x(v, lo, hi, x0, x1); };
  os << "<svg viewBox=\"0 0 640 120\" role=\"img\" aria-label=\"Box plot\">";
  os << "<rect x=\"0\" y=\"0\" width=\"640\" height=\"120\" fill=\"#0e1110\"/>";
  os << "<line x1=\"" << X(w_lo) << "\" y1=\"60\" x2=\"" << X(w_hi)
     << "\" y2=\"60\" stroke=\"#c4843a\" stroke-width=\"2\"/>";
  os << "<line x1=\"" << X(w_lo) << "\" y1=\"44\" x2=\"" << X(w_lo)
     << "\" y2=\"76\" stroke=\"#c4843a\" stroke-width=\"2\"/>";
  os << "<line x1=\"" << X(w_hi) << "\" y1=\"44\" x2=\"" << X(w_hi)
     << "\" y2=\"76\" stroke=\"#c4843a\" stroke-width=\"2\"/>";
  const double box_x = X(s.q1);
  const double box_w = X(s.q3) - X(s.q1);
  os << "<rect x=\"" << box_x << "\" y=\"36\" width=\"" << (box_w < 1 ? 1 : box_w)
     << "\" height=\"48\" fill=\"#2f6f6a\" stroke=\"#c6f35a\" "
        "stroke-width=\"1.5\"/>";
  os << "<line x1=\"" << X(s.median) << "\" y1=\"36\" x2=\"" << X(s.median)
     << "\" y2=\"84\" stroke=\"#c6f35a\" stroke-width=\"2.5\"/>";
  os << "</svg>";
  return os.str();
}

std::string svg_scatter(const std::vector<double> &x,
                        const std::vector<double> &y) {
  const size_t n = x.size() < y.size() ? x.size() : y.size();
  if (n < 2) {
    return "";
  }
  double xmin = x[0], xmax = x[0], ymin = y[0], ymax = y[0];
  for (size_t i = 1; i < n; ++i) {
    if (x[i] < xmin) xmin = x[i];
    if (x[i] > xmax) xmax = x[i];
    if (y[i] < ymin) ymin = y[i];
    if (y[i] > ymax) ymax = y[i];
  }
  if (xmax <= xmin) xmax = xmin + 1.0;
  if (ymax <= ymin) ymax = ymin + 1.0;
  const double x0 = 48.0, y0 = 16.0, x1 = 620.0, y1 = 220.0;
  auto X = [&](double v) { return svg_x(v, xmin, xmax, x0, x1); };
  auto Y = [&](double v) { return svg_x(v, ymin, ymax, y1, y0); };
  std::ostringstream os;
  os << "<svg viewBox=\"0 0 640 250\" role=\"img\" aria-label=\"Scatter plot\">";
  os << "<rect x=\"0\" y=\"0\" width=\"640\" height=\"250\" fill=\"#0e1110\"/>";
  os << "<line x1=\"" << x0 << "\" y1=\"" << y1 << "\" x2=\"" << x1 << "\" y2=\""
     << y1 << "\" stroke=\"#c4843a\" stroke-width=\"1\"/>";
  os << "<line x1=\"" << x0 << "\" y1=\"" << y0 << "\" x2=\"" << x0 << "\" y2=\""
     << y1 << "\" stroke=\"#c4843a\" stroke-width=\"1\"/>";
  double slope = 0.0, intercept = 0.0, r2 = 0.0;
  if (stats_linreg(x.data(), y.data(), n, &slope, &intercept, &r2) ==
      STATS_OK) {
    os << "<line x1=\"" << X(xmin) << "\" y1=\"" << Y(slope * xmin + intercept)
       << "\" x2=\"" << X(xmax) << "\" y2=\"" << Y(slope * xmax + intercept)
       << "\" stroke=\"#c4843a\" stroke-width=\"1.6\"/>";
  }
  for (size_t i = 0; i < n; ++i) {
    os << "<circle cx=\"" << X(x[i]) << "\" cy=\"" << Y(y[i])
       << "\" r=\"3.2\" fill=\"#c6f35a\" opacity=\"0.85\"/>";
  }
  os << "</svg>";
  return os.str();
}

void write_html_report(std::ostream &out, const std::vector<double> &values,
                       const std::vector<double> &values_y,
                       const StatsSummary &summary, const ExtraSummary &extra,
                       int precision) {
  out << "<!DOCTYPE html>\n<html lang=\"en\">\n<head>\n<meta charset=\"utf-8\">\n"
         "<meta name=\"viewport\" content=\"width=device-width, "
         "initial-scale=1\">\n<title>StatLab report</title>\n<style>\n"
         ":root{--ink:#0e1110;--paper:#e7e1d4;--copper:#c4843a;--teal:#2f6f6a;"
         "--phosphor:#c6f35a;}\n"
         "body{margin:0;background:var(--ink);color:var(--paper);"
         "font-family:\"IBM Plex Sans\",ui-sans-serif,sans-serif;}\n"
         "main{max-width:920px;margin:0 auto;padding:2rem 1.25rem 4rem;}\n"
         "h1{font-family:Syne,ui-sans-serif,sans-serif;font-weight:800;"
         "letter-spacing:.08em;text-transform:uppercase;color:var(--phosphor);"
         "margin:0 0 .25rem;}\n"
         ".tag{color:var(--copper);font-family:\"IBM Plex Mono\",monospace;"
         "font-size:.8rem;}\n"
         "table{width:100%;border-collapse:collapse;margin:1.25rem 0 2rem;}\n"
         "th,td{text-align:left;padding:.4rem .6rem;border-bottom:1px solid "
         "#1c2422;font-variant-numeric:tabular-nums;}\n"
         "th{color:var(--teal);font-size:.75rem;letter-spacing:.12em;"
         "text-transform:uppercase;}\n"
         "pre{background:#141816;color:var(--phosphor);padding:1rem;"
         "overflow:auto;font-size:.78rem;border-left:3px solid var(--copper);}\n"
         "figure{margin:0 0 2rem;}\n"
         "figcaption{color:var(--copper);font-size:.8rem;margin:.4rem 0 "
         ".6rem;letter-spacing:.08em;text-transform:uppercase;}\n"
         "svg{width:100%;height:auto;border:1px solid #24302e;}\n"
         "</style>\n</head>\n<body>\n<main>\n"
         "<p class=\"tag\">desk 2.0 · self-contained</p>\n"
         "<h1>StatLab report</h1>\n<table>\n<thead><tr><th>metric</th>"
         "<th>value</th></tr></thead>\n<tbody>\n";

  auto row = [&](const char *name, const std::string &value) {
    out << "<tr><td>" << name << "</td><td>" << html_escape(value)
        << "</td></tr>\n";
  };
  row("count", std::to_string(summary.count));
  row("mean", format_fixed(summary.mean, precision));
  row("median", format_fixed(summary.median, precision));
  row("stddev", format_fixed(summary.stddev, precision));
  row("sample_stddev", format_fixed(summary.sample_stddev, precision));
  row("min", format_fixed(summary.min, precision));
  row("q1", format_fixed(summary.q1, precision));
  row("q3", format_fixed(summary.q3, precision));
  row("max", format_fixed(summary.max, precision));
  row("iqr", format_fixed(summary.iqr, precision));
  row("fence_low", format_fixed(summary.fence_low, precision));
  row("fence_high", format_fixed(summary.fence_high, precision));
  row("outlier_count", std::to_string(summary.outlier_count));
  row("skewness", format_fixed(summary.skewness, precision));
  row("kurtosis", format_fixed(summary.kurtosis, precision));
  if (extra.use_ci) {
    row("mean_ci_low", format_fixed(extra.ci_lo, precision));
    row("mean_ci_high", format_fixed(extra.ci_hi, precision));
  }
  if (extra.use_bootstrap) {
    row("bootstrap_nboot", std::to_string(extra.bootstrap_nboot));
    row("bootstrap_ci_low", format_fixed(extra.bootstrap_lo, precision));
    row("bootstrap_ci_high", format_fixed(extra.bootstrap_hi, precision));
  }
  out << "</tbody></table>\n";

  out << "<figure><figcaption>ASCII histogram</figcaption><pre>"
      << html_escape(build_ascii_histogram(values, 10, precision))
      << "</pre></figure>\n";
  out << "<figure><figcaption>Histogram</figcaption>"
      << svg_histogram(values, 12) << "</figure>\n";
  out << "<figure><figcaption>Box plot</figcaption>" << svg_boxplot(summary)
      << "</figure>\n";

  if (values_y.size() >= 2 && values.size() >= 2) {
    out << "<figure><figcaption>Scatter + OLS</figcaption>"
        << svg_scatter(values, values_y) << "</figure>\n";
  }

  out << "<p class=\"tag\">generated by StatLab " << kVersion << "</p>\n"
      << "</main>\n</body>\n</html>\n";
}

void print_qq(const std::vector<double> &values, int precision,
              OutputFormat format) {
  std::vector<double> sample_q(values.size());
  std::vector<double> theo_q(values.size());
  const StatsStatus st = stats_normal_qq(values.data(), values.size(),
                                         sample_q.data(), theo_q.data());
  if (st != STATS_OK) {
    throw std::runtime_error(std::string("Q-Q failed: ") +
                             stats_status_message(st));
  }
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n\": " << values.size() << ",\n  \"pairs\": [\n";
    for (size_t i = 0; i < values.size(); ++i) {
      std::cout << "    {\"sample_q\": ";
      print_json_number(std::cout, sample_q[i], precision);
      std::cout << ", \"theo_q\": ";
      print_json_number(std::cout, theo_q[i], precision);
      std::cout << "}";
      if (i + 1 != values.size()) {
        std::cout << ',';
      }
      std::cout << '\n';
    }
    std::cout << "  ]\n}\n";
    return;
  }
  std::cout << "sample_q,theo_q\n";
  for (size_t i = 0; i < values.size(); ++i) {
    print_number(std::cout, sample_q[i], precision);
    std::cout << ',';
    print_number(std::cout, theo_q[i], precision);
    std::cout << '\n';
  }
}

void print_kde_grid(const std::vector<double> &values, size_t bins,
                    int precision, OutputFormat format) {
  if (bins < 1) {
    bins = 50;
  }
  double min = values[0];
  double max = values[0];
  for (double v : values) {
    if (v < min) min = v;
    if (v > max) max = v;
  }
  std::vector<double> xs(bins);
  std::vector<double> dens(bins);
  if (bins == 1 || max <= min) {
    xs[0] = min;
  } else {
    for (size_t i = 0; i < bins; ++i) {
      xs[i] = min + (max - min) * static_cast<double>(i) /
                        static_cast<double>(bins - 1);
    }
  }
  const StatsStatus st =
      stats_kde_grid(values.data(), values.size(), xs.data(), bins, dens.data());
  if (st != STATS_OK) {
    throw std::runtime_error(std::string("KDE failed: ") +
                             stats_status_message(st));
  }
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n\": " << bins << ",\n  \"points\": [\n";
    for (size_t i = 0; i < bins; ++i) {
      std::cout << "    {\"x\": ";
      print_json_number(std::cout, xs[i], precision);
      std::cout << ", \"density\": ";
      print_json_number(std::cout, dens[i], precision);
      std::cout << "}";
      if (i + 1 != bins) {
        std::cout << ',';
      }
      std::cout << '\n';
    }
    std::cout << "  ]\n}\n";
    return;
  }
  std::cout << "x,density\n";
  for (size_t i = 0; i < bins; ++i) {
    print_number(std::cout, xs[i], precision);
    std::cout << ',';
    print_number(std::cout, dens[i], precision);
    std::cout << '\n';
  }
}

void print_mwu(const std::vector<double> &x, const std::vector<double> &y,
               int precision, OutputFormat format) {
  double u = 0.0;
  double p = 0.0;
  const StatsStatus st =
      stats_mannwhitney(x.data(), x.size(), y.data(), y.size(), &u, &p);
  if (st != STATS_OK) {
    throw std::runtime_error(std::string("Mann-Whitney failed: ") +
                             stats_status_message(st));
  }
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n_x\": " << x.size() << ",\n  \"n_y\": " << y.size()
              << ",\n  \"mann_whitney_u\": ";
    print_json_number(std::cout, u, precision);
    std::cout << ",\n  \"mann_whitney_p\": ";
    print_json_number(std::cout, p, precision);
    std::cout << "\n}\n";
    return;
  }
  if (format == OutputFormat::Csv || format == OutputFormat::Html) {
    std::cout << "metric,value\nn_x," << x.size() << "\nn_y," << y.size()
              << "\nmann_whitney_u,";
    print_number(std::cout, u, precision);
    std::cout << "\nmann_whitney_p,";
    print_number(std::cout, p, precision);
    std::cout << '\n';
    return;
  }
  std::cout << "Mann-Whitney U\n";
  std::cout << "n_x:             " << x.size() << '\n';
  std::cout << "n_y:             " << y.size() << '\n';
  std::cout << "mann_whitney_u:  ";
  print_number(std::cout, u, precision);
  std::cout << "\nmann_whitney_p:  ";
  print_number(std::cout, p, precision);
  std::cout << '\n';
}

StatsSummary require_summary(const std::vector<double> &values,
                             const char *label) {
  StatsSummary summary{};
  const StatsStatus status =
      stats_summary(values.data(), values.size(), &summary);

  if (status == STATS_ERR_EMPTY) {
    throw std::runtime_error(std::string("No numeric values were provided") +
                             (label ? std::string(" for ") + label : "") +
                             ".");
  }

  if (status != STATS_OK) {
    throw std::runtime_error(std::string("Could not calculate statistics: ") +
                             stats_status_message(status) + ".");
  }

  return summary;
}

/* Align two series to the shorter length for paired analyses. */
size_t paired_n(const std::vector<double> &x, const std::vector<double> &y) {
  return x.size() < y.size() ? x.size() : y.size();
}

void print_ks(const std::vector<double> &x, const std::vector<double> &y,
              int precision, OutputFormat format) {
  double d = 0.0;
  const StatsStatus st =
      stats_ks_2samp(x.data(), x.size(), y.data(), y.size(), &d);
  if (st != STATS_OK) {
    throw std::runtime_error(std::string("KS two-sample failed: ") +
                             stats_status_message(st));
  }
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n_x\": " << x.size() << ",\n  \"n_y\": " << y.size()
              << ",\n  \"ks_d\": ";
    print_json_number(std::cout, d, precision);
    std::cout << "\n}\n";
    return;
  }
  if (format == OutputFormat::Csv || format == OutputFormat::Html) {
    std::cout << "metric,value\nn_x," << x.size() << "\nn_y," << y.size()
              << "\nks_d,";
    print_number(std::cout, d, precision);
    std::cout << '\n';
    return;
  }
  std::cout << "Two-sample Kolmogorov-Smirnov\n";
  std::cout << "n_x:             " << x.size() << '\n';
  std::cout << "n_y:             " << y.size() << '\n';
  std::cout << "ks_d:            ";
  print_number(std::cout, d, precision);
  std::cout << '\n';
}

void print_paired(const std::vector<double> &x, const std::vector<double> &y,
                  int precision, OutputFormat format) {
  const size_t n = paired_n(x, y);
  double t = 0.0;
  double df = 0.0;
  const StatsStatus st = stats_ttest_rel(x.data(), y.data(), n, &t, &df);
  if (st != STATS_OK) {
    throw std::runtime_error(std::string("Paired t-test failed: ") +
                             stats_status_message(st));
  }
  if (format == OutputFormat::Json) {
    std::cout << "{\n  \"n\": " << n << ",\n  \"paired_t\": ";
    print_json_number(std::cout, t, precision);
    std::cout << ",\n  \"paired_df\": ";
    print_json_number(std::cout, df, precision);
    std::cout << "\n}\n";
    return;
  }
  if (format == OutputFormat::Csv || format == OutputFormat::Html) {
    std::cout << "metric,value\nn," << n << "\npaired_t,";
    print_number(std::cout, t, precision);
    std::cout << "\npaired_df,";
    print_number(std::cout, df, precision);
    std::cout << '\n';
    return;
  }
  std::cout << "Paired t-test\n";
  std::cout << "n:               " << n << '\n';
  std::cout << "paired_t:        ";
  print_number(std::cout, t, precision);
  std::cout << "\npaired_df:       ";
  print_number(std::cout, df, precision);
  std::cout << '\n';
}

} // namespace

int main(int argc, char **argv) {
  try {
    const CliOptions options = parse_args(argc, argv);

    if (options.has_compare) {
      const StatsSummary a =
          require_summary(options.values, "primary dataset");
      const StatsSummary b =
          require_summary(options.compare_values, "compare dataset");
      print_compare(a, b, options.precision, options.format);
      return 0;
    }

    /* Series transforms: rolling / EMA / robust-z (stdout only). */
    if (options.rolling) {
      if (options.values.empty()) {
        throw std::runtime_error("No numeric values were provided.");
      }
      if (options.rolling_window > options.values.size()) {
        throw std::runtime_error(
            "--rolling window must be <= number of values");
      }
      const size_t out_n =
          options.values.size() - options.rolling_window + 1;
      std::vector<double> series(out_n);
      const StatsStatus st = stats_moving_average(
          options.values.data(), options.values.size(), options.rolling_window,
          series.data());
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("Moving average failed: ") +
                                 stats_status_message(st));
      }
      print_series("Moving average", "ma", series, options.precision,
                   options.format);
      return 0;
    }

    if (options.ema) {
      if (options.values.empty()) {
        throw std::runtime_error("No numeric values were provided.");
      }
      std::vector<double> series(options.values.size());
      const StatsStatus st =
          stats_ema(options.values.data(), options.values.size(),
                    options.ema_alpha, series.data());
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("EMA failed: ") +
                                 stats_status_message(st));
      }
      print_series("EMA", "ema", series, options.precision, options.format);
      return 0;
    }

    if (options.robust_z) {
      if (options.values.empty()) {
        throw std::runtime_error("No numeric values were provided.");
      }
      std::vector<double> series(options.values.size());
      const StatsStatus st = stats_robust_zscore(
          options.values.data(), options.values.size(), series.data());
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("Robust z-score failed: ") +
                                 stats_status_message(st));
      }
      print_series("Robust z-scores", "robust_z", series, options.precision,
                   options.format);
      return 0;
    }

    if (options.show_diff) {
      if (options.values.size() < 2) {
        throw std::runtime_error("--diff requires at least 2 values.");
      }
      std::vector<double> series(options.values.size() - 1);
      const StatsStatus st =
          stats_diff(options.values.data(), options.values.size(), series.data());
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("diff failed: ") +
                                 stats_status_message(st));
      }
      print_series("First differences", "diff", series, options.precision,
                   options.format);
      return 0;
    }

    if (options.show_cumsum) {
      if (options.values.empty()) {
        throw std::runtime_error("No numeric values were provided.");
      }
      std::vector<double> series(options.values.size());
      const StatsStatus st = stats_cumsum(options.values.data(),
                                          options.values.size(), series.data());
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("cumsum failed: ") +
                                 stats_status_message(st));
      }
      print_series("Cumulative sum", "cumsum", series, options.precision,
                   options.format);
      return 0;
    }

    if (options.show_ranks) {
      if (options.values.empty()) {
        throw std::runtime_error("No numeric values were provided.");
      }
      std::vector<double> series(options.values.size());
      const StatsStatus st =
          stats_rank(options.values.data(), options.values.size(), series.data());
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("rank failed: ") +
                                 stats_status_message(st));
      }
      print_series("Ranks", "rank", series, options.precision, options.format);
      return 0;
    }

    if (options.acf && !options.acf_lag_given) {
      if (options.values.empty()) {
        throw std::runtime_error("No numeric values were provided.");
      }
      const size_t n = options.values.size();
      const size_t max_lag = n > 1 ? (n - 1 < 20 ? n - 1 : 20) : 0;
      std::vector<double> series(max_lag + 1);
      const StatsStatus st =
          stats_acf_series(options.values.data(), n, max_lag, series.data());
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("ACF failed: ") +
                                 stats_status_message(st));
      }
      print_acf_series(series, options.precision, options.format);
      return 0;
    }

    const bool html_out =
        options.write_html || options.format == OutputFormat::Html;

    bool specialized = false;
    const bool tables = options.format != OutputFormat::Html;
    if (options.qq) {
      if (options.values.empty()) {
        throw std::runtime_error("No numeric values were provided.");
      }
      if (tables) {
        print_qq(options.values, options.precision, options.format);
      }
      specialized = true;
    }

    if (options.kde) {
      if (options.values.empty()) {
        throw std::runtime_error("No numeric values were provided.");
      }
      if (tables) {
        print_kde_grid(options.values, options.kde_bins, options.precision,
                       options.format);
      }
      specialized = true;
    }

    if (options.mwu) {
      if (options.values.empty() || options.values_y.empty()) {
        throw std::runtime_error(
            "Two series required for --mwu: pass --mwu <file>, or "
            "--column N --column2 M with a multi-column file");
      }
      if (tables) {
        print_mwu(options.values, options.values_y, options.precision,
                  options.format);
      }
      specialized = true;
    }

    if (options.ks) {
      if (options.values.empty() || options.values_y.empty()) {
        throw std::runtime_error(
            "Two series required for --ks: pass --ks <file>, or "
            "--column N --column2 M with a multi-column file");
      }
      if (tables) {
        print_ks(options.values, options.values_y, options.precision,
                 options.format);
      }
      specialized = true;
    }

    if (options.paired) {
      if (options.values.empty() || options.values_y.empty()) {
        throw std::runtime_error(
            "Two series required for --paired: use --column N --column2 M "
            "with a multi-column file");
      }
      const size_t n = paired_n(options.values, options.values_y);
      if (n < 2) {
        throw std::runtime_error(
            "Paired t-test requires at least 2 aligned observations");
      }
      if (tables) {
        print_paired(options.values, options.values_y, options.precision,
                     options.format);
      }
      specialized = true;
    }

    if (specialized && !html_out) {
      return 0;
    }

    /* Paired analyses: correlation, Spearman, covariance, and/or regression. */
    if (options.correlate || options.regression || options.spearman ||
        options.cov) {
      if (options.values_y.empty()) {
        throw std::runtime_error(
            "Two series required: use --correlate/--spearman/--cov <file>, "
            "or --column N --column2 M with a multi-column file");
      }

      const size_t n = paired_n(options.values, options.values_y);
      if (n < 2) {
        throw std::runtime_error(
            "Paired analysis requires at least 2 aligned observations");
      }

      bool printed = false;

      if (options.correlate) {
        double r = 0.0;
        const StatsStatus st = stats_correlation(
            options.values.data(), options.values_y.data(), n, &r);
        if (st != STATS_OK) {
          throw std::runtime_error(std::string("Correlation failed: ") +
                                   stats_status_message(st));
        }
        print_correlation(r, n, options.precision, options.format);
        printed = true;
      }

      if (options.spearman) {
        double rho = 0.0;
        const StatsStatus st = stats_spearman(
            options.values.data(), options.values_y.data(), n, &rho);
        if (st != STATS_OK) {
          throw std::runtime_error(std::string("Spearman failed: ") +
                                   stats_status_message(st));
        }
        if (printed) {
          std::cout << '\n';
        }
        print_spearman(rho, n, options.precision, options.format);
        printed = true;
      }

      if (options.cov) {
        double c = 0.0;
        const StatsStatus st = stats_covariance(
            options.values.data(), options.values_y.data(), n,
            options.cov_sample ? 1 : 0, &c);
        if (st != STATS_OK) {
          throw std::runtime_error(std::string("Covariance failed: ") +
                                   stats_status_message(st));
        }
        if (printed) {
          std::cout << '\n';
        }
        print_covariance(c, n, options.cov_sample, options.precision,
                         options.format);
        printed = true;
      }

      if (options.regression) {
        double slope = 0.0;
        double intercept = 0.0;
        double r2 = 0.0;
        const StatsStatus st =
            stats_linreg(options.values.data(), options.values_y.data(), n,
                         &slope, &intercept, &r2);
        if (st != STATS_OK) {
          throw std::runtime_error(std::string("Regression failed: ") +
                                   stats_status_message(st));
        }
        if (printed) {
          std::cout << '\n';
        }
        print_regression(slope, intercept, r2, n, options.precision,
                         options.format);
      }

      return 0;
    }

    /* --outliers as primary specialized report (still allows boxplot). */
    if (options.outliers && !options.use_trim && !options.ci &&
        !options.geomean && !options.winsor && !options.entropy &&
        !options.acf && !options.ecdf && !options.nunique && !options.jb &&
        options.format != OutputFormat::Text) {
      const StatsSummary summary = require_summary(options.values, nullptr);
      print_outliers(options.values, summary, options.precision, options.format);
      if (options.boxplot) {
        print_boxplot(summary, options.precision);
      }
      return 0;
    }

    const StatsSummary summary = require_summary(options.values, nullptr);

    ExtraSummary extra;
    extra.use_trim = options.use_trim;
    if (options.use_trim) {
      const StatsStatus st = stats_trimmed_mean(
          options.values.data(), options.values.size(), options.trim_fraction,
          &extra.trimmed_mean);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("Trimmed mean failed: ") +
                                 stats_status_message(st));
      }
    }

    if (options.ci) {
      double mean = 0.0;
      const StatsStatus st =
          stats_mean_ci(options.values.data(), options.values.size(), &mean,
                        &extra.ci_lo, &extra.ci_hi);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("Mean CI failed: ") +
                                 stats_status_message(st));
      }
      extra.use_ci = true;
    }

    if (options.geomean) {
      double g = 0.0;
      double h = 0.0;
      const StatsStatus sg = stats_geometric_mean(
          options.values.data(), options.values.size(), &g);
      const StatsStatus sh = stats_harmonic_mean(
          options.values.data(), options.values.size(), &h);
      if (sg == STATS_OK && sh == STATS_OK) {
        extra.use_geomean = true;
        extra.geometric_mean = g;
        extra.harmonic_mean = h;
      }
      /* If any non-positive values, skip quietly (report remains valid). */
    }

    if (options.winsor) {
      std::vector<double> w(options.values.size());
      const StatsStatus st =
          stats_winsorize(options.values.data(), options.values.size(),
                          options.winsor_limits, w.data());
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("Winsorize failed: ") +
                                 stats_status_message(st));
      }
      double sum = 0.0;
      for (double v : w) {
        sum += v;
      }
      extra.winsorized_mean = sum / static_cast<double>(w.size());
      extra.use_winsor = true;
    }

    if (options.entropy) {
      const StatsStatus st =
          stats_entropy(options.values.data(), options.values.size(),
                        options.entropy_bins, &extra.entropy);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("Entropy failed: ") +
                                 stats_status_message(st));
      }
      extra.use_entropy = true;
    }

    if (options.acf && options.acf_lag_given) {
      if (options.acf_lag >= options.values.size()) {
        throw std::runtime_error("--acf lag must be < number of values");
      }
      const StatsStatus st =
          stats_acf(options.values.data(), options.values.size(),
                    options.acf_lag, &extra.acf);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("ACF failed: ") +
                                 stats_status_message(st));
      }
      extra.use_acf = true;
      extra.acf_lag = options.acf_lag;
    }

    if (options.ecdf) {
      const StatsStatus st =
          stats_ecdf(options.values.data(), options.values.size(),
                     options.ecdf_x, &extra.ecdf);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("ECDF failed: ") +
                                 stats_status_message(st));
      }
      extra.use_ecdf = true;
      extra.ecdf_x = options.ecdf_x;
    }

    if (options.nunique) {
      const StatsStatus st = stats_nunique(
          options.values.data(), options.values.size(), &extra.nunique);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("nunique failed: ") +
                                 stats_status_message(st));
      }
      extra.use_nunique = true;
    }

    if (options.jb) {
      const StatsStatus st = stats_jarque_bera(
          options.values.data(), options.values.size(), &extra.jarque_bera,
          &extra.jarque_bera_p);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("Jarque-Bera failed: ") +
                                 stats_status_message(st));
      }
      extra.use_jb = true;
    }

    if (options.show_rms) {
      const StatsStatus st =
          stats_rms(options.values.data(), options.values.size(), &extra.rms);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("RMS failed: ") +
                                 stats_status_message(st));
      }
      extra.use_rms = true;
    }

    if (options.show_argmin) {
      const StatsStatus st = stats_argmin(
          options.values.data(), options.values.size(), &extra.argmin);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("argmin failed: ") +
                                 stats_status_message(st));
      }
      extra.use_argmin = true;
    }

    if (options.show_argmax) {
      const StatsStatus st = stats_argmax(
          options.values.data(), options.values.size(), &extra.argmax);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("argmax failed: ") +
                                 stats_status_message(st));
      }
      extra.use_argmax = true;
    }

    if (options.bootstrap) {
      double bmean = 0.0;
      const StatsStatus st = stats_bootstrap_mean_ci(
          options.values.data(), options.values.size(), options.bootstrap_nboot,
          options.seed, 0.95, &extra.bootstrap_lo, &extra.bootstrap_hi, &bmean);
      if (st != STATS_OK) {
        throw std::runtime_error(std::string("Bootstrap failed: ") +
                                 stats_status_message(st));
      }
      extra.use_bootstrap = true;
      extra.bootstrap_nboot = options.bootstrap_nboot;
    }

    if (options.format != OutputFormat::Html) {
      print_summary(summary, options.format, options.precision, extra);
    }

    if (html_out) {
      if (options.format == OutputFormat::Html) {
        write_html_report(std::cout, options.values, options.values_y, summary,
                          extra, options.precision);
      }
      if (options.write_html) {
        std::ofstream html_file(options.html_path);
        if (!html_file) {
          throw std::runtime_error("Could not write HTML report: " +
                                   options.html_path);
        }
        write_html_report(html_file, options.values, options.values_y, summary,
                          extra, options.precision);
        if (options.format != OutputFormat::Html) {
          std::cout << "Wrote StatLab HTML report: " << options.html_path
                    << '\n';
        }
      }
    }

    if (options.outliers && options.format == OutputFormat::Text) {
      std::cout << '\n';
      print_outliers(options.values, summary, options.precision,
                     OutputFormat::Text);
    }

    if (options.histogram) {
      print_histogram(options.values, options.histogram_bins,
                      options.precision);
    }

    if (options.boxplot) {
      print_boxplot(summary, options.precision);
    }

    return 0;
  } catch (const std::exception &error) {
    std::cerr << "Error: " << error.what() << '\n';
    return 1;
  }
}
