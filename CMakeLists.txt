cmake_minimum_required(VERSION 3.16)

project(
  statlab
  VERSION 2.2.0
  DESCRIPTION "StatLab: a C17 statistics engine with a C++17 CLI and web studio"
  LANGUAGES C CXX
)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(stats_core src/stats_core.c)
target_include_directories(stats_core PUBLIC ${PROJECT_SOURCE_DIR}/include)

if(UNIX AND NOT APPLE)
  target_link_libraries(stats_core PUBLIC m)
endif()

add_executable(statlab src/main.cpp)
target_link_libraries(statlab PRIVATE stats_core)
# Keep --version in step with the project version automatically.
target_compile_definitions(statlab
  PRIVATE STATLAB_VERSION="${PROJECT_VERSION}")

if(MSVC)
  target_compile_options(stats_core PRIVATE /W4)
  target_compile_options(statlab PRIVATE /W4)
else()
  target_compile_options(stats_core PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(statlab PRIVATE -Wall -Wextra -Wpedantic)
endif()

enable_testing()

add_executable(stats_core_tests tests/stats_core_tests.cpp)
target_link_libraries(stats_core_tests PRIVATE stats_core)

# Expectations derived from published definitions rather than from this
# library's own output -- see the header of the file for why.
add_executable(reference_values_tests tests/reference_values_tests.cpp)
target_link_libraries(reference_values_tests PRIVATE stats_core)

if(MSVC)
  target_compile_options(stats_core_tests PRIVATE /W4)
  target_compile_options(reference_values_tests PRIVATE /W4)
else()
  target_compile_options(stats_core_tests PRIVATE -Wall -Wextra -Wpedantic)
  target_compile_options(reference_values_tests PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_test(NAME stats_core_tests COMMAND stats_core_tests)
add_test(NAME cli_text_file COMMAND statlab --file ${PROJECT_SOURCE_DIR}/examples/sample.csv)
add_test(NAME cli_json_precision COMMAND statlab --format json --precision 2 1 2 3 4)
add_test(NAME cli_csv_precision COMMAND statlab --format csv --precision 1 1,2,3,4)
add_test(NAME cli_histogram COMMAND statlab --histogram 5 1 2 3 4 5 6 7 8 9 10)
add_test(NAME reference_values COMMAND reference_values_tests)
add_test(NAME cli_version COMMAND statlab --version)
add_test(NAME cli_new_metrics COMMAND statlab --format csv --precision 4 2 4 4 4 5 5 7 9)
add_test(NAME cli_outliers COMMAND statlab --outliers --format text 1 2 3 4 5 100)
add_test(NAME cli_boxplot COMMAND statlab --boxplot 1 2 3 4 5 6 7 8 9 10)
add_test(NAME cli_trim COMMAND statlab --trim 0.1 --format csv 1 2 3 4 5 6 7 8 9 100)
add_test(NAME cli_correlate_cols COMMAND statlab --column 1 --column2 2 --correlate --skip-header --file ${PROJECT_SOURCE_DIR}/examples/paired.csv)
add_test(NAME cli_regression_cols COMMAND statlab --column 1 --column2 2 --regression --skip-header --file ${PROJECT_SOURCE_DIR}/examples/paired.csv)
add_test(NAME cli_fences COMMAND statlab --format csv --precision 4 2 4 4 4 5 5 7 9)
add_test(NAME cli_spearman_cols COMMAND statlab --column 1 --column2 2 --spearman --skip-header --file ${PROJECT_SOURCE_DIR}/examples/paired.csv)
add_test(NAME cli_cov_cols COMMAND statlab --column 1 --column2 2 --cov --skip-header --file ${PROJECT_SOURCE_DIR}/examples/paired.csv)
add_test(NAME cli_rolling COMMAND statlab --rolling 3 --format csv --precision 4 1 2 3 4 5)
add_test(NAME cli_ema COMMAND statlab --ema 0.5 --format csv --precision 4 1 2 3)
add_test(NAME cli_ci COMMAND statlab --ci --format csv --precision 4 2 4 4 4 5 5 7 9)
add_test(NAME cli_geomean COMMAND statlab --geomean --format csv --precision 4 1 3 9)
add_test(NAME cli_winsor COMMAND statlab --winsor 0.1 --format csv --precision 4 1 2 3 4 5 6 7 8 9 100)
add_test(NAME cli_entropy COMMAND statlab --entropy 4 --format csv --precision 4 1 1 2 2)
add_test(NAME cli_acf COMMAND statlab --acf 1 --format csv --precision 4 1 2 3 4 5)
add_test(NAME cli_acf_series COMMAND statlab --acf --format csv --precision 4 1 2 3 4 5)
add_test(NAME cli_ecdf COMMAND statlab --ecdf 3 --format csv --precision 4 1 2 3 4 5)
add_test(NAME cli_robust_z COMMAND statlab --robust-z --format csv --precision 4 1 2 3 4 5)
add_test(NAME cli_nunique COMMAND statlab --nunique --format csv --precision 4 1 2 2 3 1)
add_test(NAME cli_jb COMMAND statlab --jb --format csv --precision 4 2 4 4 4 5 5 7 9)

set_tests_properties(cli_text_file PROPERTIES PASS_REGULAR_EXPRESSION "median:[ ]*4\\.5000")
set_tests_properties(cli_json_precision PROPERTIES PASS_REGULAR_EXPRESSION "\"mean\": 2.50")
set_tests_properties(cli_csv_precision PROPERTIES PASS_REGULAR_EXPRESSION "sample_stddev,1.3")
# Derived from PROJECT_VERSION so a bump cannot leave this assertion behind.
string(REPLACE "." "\\." STATLAB_VERSION_REGEX "${PROJECT_VERSION}")
set_tests_properties(cli_version
  PROPERTIES PASS_REGULAR_EXPRESSION "${STATLAB_VERSION_REGEX}")
set_tests_properties(cli_new_metrics PROPERTIES PASS_REGULAR_EXPRESSION "iqr,1\\.5000.*mode,4\\.0000.*cv,0\\.4000")
set_tests_properties(cli_outliers PROPERTIES PASS_REGULAR_EXPRESSION "outlier_count")
set_tests_properties(cli_boxplot PROPERTIES PASS_REGULAR_EXPRESSION "fence_low")
set_tests_properties(cli_trim PROPERTIES PASS_REGULAR_EXPRESSION "trimmed_mean,5\\.5000")
set_tests_properties(cli_correlate_cols PROPERTIES PASS_REGULAR_EXPRESSION "pearson_r:[ ]*1\\.0000")
set_tests_properties(cli_regression_cols PROPERTIES PASS_REGULAR_EXPRESSION "slope:[ ]*2\\.0000")
set_tests_properties(cli_fences PROPERTIES PASS_REGULAR_EXPRESSION "fence_low,1\\.7500.*fence_high,7\\.7500.*outlier_count,1")
set_tests_properties(cli_spearman_cols PROPERTIES PASS_REGULAR_EXPRESSION "spearman_rho:[ ]*1\\.0000")
set_tests_properties(cli_cov_cols PROPERTIES PASS_REGULAR_EXPRESSION "sample_covariance")
set_tests_properties(cli_rolling PROPERTIES PASS_REGULAR_EXPRESSION "index,ma.*0,2\\.0000.*1,3\\.0000.*2,4\\.0000")
set_tests_properties(cli_ema PROPERTIES PASS_REGULAR_EXPRESSION "0,1\\.0000.*1,1\\.5000.*2,2\\.2500")
set_tests_properties(cli_ci PROPERTIES PASS_REGULAR_EXPRESSION "mean_ci_low,.*mean_ci_high,")
set_tests_properties(cli_geomean PROPERTIES PASS_REGULAR_EXPRESSION "geometric_mean,3\\.0000.*harmonic_mean,2\\.0769")
set_tests_properties(cli_winsor PROPERTIES PASS_REGULAR_EXPRESSION "winsorized_mean,")
set_tests_properties(cli_entropy PROPERTIES PASS_REGULAR_EXPRESSION "entropy,")
set_tests_properties(cli_acf PROPERTIES PASS_REGULAR_EXPRESSION "acf,0\\.4000")
set_tests_properties(cli_acf_series PROPERTIES PASS_REGULAR_EXPRESSION
                     "lag,acf")
set_tests_properties(cli_ecdf PROPERTIES PASS_REGULAR_EXPRESSION "ecdf,0\\.6000")
set_tests_properties(cli_robust_z PROPERTIES PASS_REGULAR_EXPRESSION "index,robust_z")
set_tests_properties(cli_nunique PROPERTIES PASS_REGULAR_EXPRESSION "nunique,3")
set_tests_properties(cli_jb PROPERTIES PASS_REGULAR_EXPRESSION "jarque_bera,")
add_test(NAME cli_rms COMMAND statlab --rms --format csv --precision 4 3 4 5)
add_test(NAME cli_diff COMMAND statlab --diff --format csv --precision 4 1 3 6)
add_test(NAME cli_cumsum COMMAND statlab --cumsum --format csv --precision 4 1 2 3)
add_test(NAME cli_ranks COMMAND statlab --ranks --format csv --precision 4 3 1 2)
add_test(NAME cli_argmin COMMAND statlab --argmin --format csv 5 1 3)
set_tests_properties(cli_rms PROPERTIES PASS_REGULAR_EXPRESSION "rms,")
set_tests_properties(cli_diff PROPERTIES PASS_REGULAR_EXPRESSION "index,diff")
set_tests_properties(cli_cumsum PROPERTIES PASS_REGULAR_EXPRESSION "index,cumsum")
set_tests_properties(cli_ranks PROPERTIES PASS_REGULAR_EXPRESSION "index,rank")
set_tests_properties(cli_argmin PROPERTIES PASS_REGULAR_EXPRESSION "argmin,1")

add_test(NAME cli_html COMMAND statlab --format html 1 2 3 4 5)
add_test(NAME cli_html_file COMMAND statlab --html ${CMAKE_CURRENT_BINARY_DIR}/statlab_report.html 1 2 3 4 5)
add_test(NAME cli_bootstrap COMMAND statlab --bootstrap 100 --seed 1 --format csv 1 2 3 4 5)
add_test(NAME cli_qq COMMAND statlab --qq --format csv 1 2 3 4 5)
add_test(NAME cli_kde COMMAND statlab --kde 8 --format csv 1 2 3 4 5)
add_test(NAME cli_mwu COMMAND statlab --mwu ${PROJECT_SOURCE_DIR}/examples/sample.csv --format csv 1 2 3 4 5 6 7 8 9 10)
set_tests_properties(cli_html PROPERTIES PASS_REGULAR_EXPRESSION "<html")
set_tests_properties(cli_html_file PROPERTIES PASS_REGULAR_EXPRESSION "StatLab")
set_tests_properties(cli_bootstrap PROPERTIES PASS_REGULAR_EXPRESSION "bootstrap_ci_low")
set_tests_properties(cli_qq PROPERTIES PASS_REGULAR_EXPRESSION "sample_q,theo_q")
set_tests_properties(cli_kde PROPERTIES PASS_REGULAR_EXPRESSION "x,density")
set_tests_properties(cli_mwu PROPERTIES PASS_REGULAR_EXPRESSION "mann_whitney_u")

add_test(NAME cli_ks COMMAND statlab --column 1 --column2 2 --ks --skip-header --format csv --file ${PROJECT_SOURCE_DIR}/examples/paired.csv)
add_test(NAME cli_paired COMMAND statlab --column 1 --column2 2 --paired --skip-header --format csv --file ${PROJECT_SOURCE_DIR}/examples/paired.csv)
set_tests_properties(cli_ks PROPERTIES PASS_REGULAR_EXPRESSION "ks_d,0.6000")
set_tests_properties(cli_paired PROPERTIES PASS_REGULAR_EXPRESSION "paired_t,-4.2426")

install(TARGETS statlab stats_core)
