#!/usr/bin/env bash
# Build and run all ngPost native test suites (CryptoTest, ArticleTest, NzbTest)
# and CLI offline NNTP integration tests against vendored fixtures. Builds in a per-user
# scratch directory (reused across runs) so the source tree stays clean.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="${TMPDIR:-/tmp}/ngpost-tests-$(id -u)"
mkdir -p "$BUILD_DIR"

# Ensure test-vectors symlink is available in build directory for tests
# that look for fixtures relative to applicationDirPath() or cwd.
ln -sfn "$SCRIPT_DIR/test-vectors" "$BUILD_DIR/test-vectors"

SUITES=("CryptoTest" "ArticleTest" "NzbTest")
for suite in "${SUITES[@]}"; do
    echo "=== Building and running $suite ==="
    SUITE_BUILD_DIR="$BUILD_DIR/$suite"
    mkdir -p "$SUITE_BUILD_DIR"
    ln -sfn "$SCRIPT_DIR/test-vectors" "$SUITE_BUILD_DIR/test-vectors"
    (
        cd "$SUITE_BUILD_DIR"
        qmake "$SCRIPT_DIR/$suite.pro"
        make -j$(nproc 2>/dev/null || echo 2)
        "./$suite"
    )
done

echo "=== Building and running CLI offline integration tests ==="
CLI_BUILD_DIR="$BUILD_DIR/cli"
mkdir -p "$CLI_BUILD_DIR"
(
    cd "$CLI_BUILD_DIR"
    qmake "$SCRIPT_DIR/../src/ngPost_cmd.pro"
    make -j$(nproc 2>/dev/null || echo 2)
)
python3 "$SCRIPT_DIR/test_cli_integration.py" --bin "$CLI_BUILD_DIR/ngPost"
