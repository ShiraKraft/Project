#!/bin/bash

# ============================================================
# test.sh  -  OS Project Automated Test Harness
# Milestones 4, 5, 6
# ============================================================

# Colour helpers
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
CYAN='\033[0;36m'
RESET='\033[0m'

PASS=0
FAIL=0

pass() { echo -e "${GREEN}  [PASS]${RESET} $1"; ((PASS++)); }
fail() { echo -e "${RED}  [FAIL]${RESET} $1"; ((FAIL++)); }
info() { echo -e "${CYAN}  [INFO]${RESET} $1"; }

# ============================================================
# MILESTONE 4
# ============================================================

echo ""
echo -e "${YELLOW}=== Compiling Milestone 4 ===${RESET}"
make clean
make milestone4

echo -e "\n=== Running Test with imput.txt ==="
./sim imput.txt

# ============================================================
# MILESTONE 6 - Synchronisation Test Suites
# Usage:
#   ./test.sh                    run all suites
#   ./test.sh --skip-valgrind    skip valgrind (faster)
#   ./test.sh --only deadlock    run one suite only
# ============================================================

SKIP_VALGRIND=false
ONLY_SUITE=""
for arg in "$@"; do
    case "$arg" in
        --skip-valgrind) SKIP_VALGRIND=true ;;
        --only) shift; ONLY_SUITE="${1:-}" ;;
    esac
done

SIM="./sim"
LOG_FILE="/tmp/m6_node_access.log"
TMP_DIR="/tmp/m6_test_inputs"
NORMAL_GRAPH="$TMP_DIR/normal.txt"
HEAVY_GRAPH="./heavy_traffic.txt"
DISCONNECTED_GRAPH="$TMP_DIR/disconn.txt"
FULLY_CONN_GRAPH="$TMP_DIR/full.txt"

mkdir -p "$TMP_DIR"

run_sim_headless() {
    local input_file="$1"
    local seconds="$2"
    DISPLAY="" timeout "$seconds" "$SIM" "$input_file" \
        >/tmp/m6_sim_stdout.txt 2>/tmp/m6_sim_stderr.txt
    return $?
}

# ------------------------------------------------------------
# Suite 1: Compilation
# ------------------------------------------------------------
check_compilation() {
    echo ""
    echo -e "${YELLOW}=== Suite 1: Compilation ===${RESET}"
    rm -f /tmp/m6_make.log
    if make clean >>/tmp/m6_make.log 2>&1 && make milestone6 >>/tmp/m6_make.log 2>&1; then
        if grep -qiE "warning:|error:" /tmp/m6_make.log 2>/dev/null; then
            fail "Compilation produced warnings/errors (see /tmp/m6_make.log)"
        else
            pass "make milestone6 succeeded with zero warnings"
        fi
        if [[ -x "$SIM" ]]; then
            pass "Binary '$SIM' exists and is executable"
        else
            fail "Binary '$SIM' not found after make"
        fi
    else
        fail "make milestone6 returned non-zero exit code"
    fi
}

# ------------------------------------------------------------
# Suite 2: Sanity checks
# ------------------------------------------------------------
run_sanity_checks() {
    echo ""
    echo -e "${YELLOW}=== Suite 2: Sanity Checks ===${RESET}"

    cat > "$NORMAL_GRAPH" <<'EOF'
6 8
0 1 4
0 2 2
1 3 5
2 1 1
2 3 8
3 4 2
4 5 3
2 5 15
2
0 5
2 4
EOF
    info "Testing normal graph (2 travelers, 6 nodes)..."
    run_sim_headless "$NORMAL_GRAPH" 30
    local rc=$?
    if [[ $rc -eq 124 ]]; then
        fail "Normal graph - sim TIMED OUT (possible deadlock)"
    elif [[ $rc -eq 0 ]]; then
        pass "Normal graph - sim exited with code 0"
    else
        fail "Normal graph - sim exited with non-zero code $rc"
    fi

    cat > "$DISCONNECTED_GRAPH" <<'EOF'
4 2
0 1 3
2 3 5
2
0 3
2 0
EOF
    info "Testing disconnected graph..."
    run_sim_headless "$DISCONNECTED_GRAPH" 15
    local dc=$?
    if [[ $dc -eq 124 ]]; then
        fail "Disconnected graph - sim TIMED OUT"
    else
        pass "Disconnected graph - exited without hanging (code=$dc)"
    fi

    cat > "$FULLY_CONN_GRAPH" <<'EOF'
4 12
0 1 1
0 2 1
0 3 1
1 0 1
1 2 1
1 3 1
2 0 1
2 1 1
2 3 1
3 0 1
3 1 1
3 2 1
3
0 3
1 2
2 0
EOF
    info "Testing fully-connected 4-node graph (3 travelers)..."
    run_sim_headless "$FULLY_CONN_GRAPH" 30
    local fc=$?
    if [[ $fc -eq 124 ]]; then
        fail "Fully-connected graph - sim TIMED OUT"
    elif [[ $fc -eq 0 ]]; then
        pass "Fully-connected graph - sim exited with code 0"
    else
        fail "Fully-connected graph - sim exited with code $fc"
    fi
}

# ------------------------------------------------------------
# Suite 3: Deadlock detection
# ------------------------------------------------------------
detect_deadlocks() {
    echo ""
    echo -e "${YELLOW}=== Suite 3: Deadlock Detection ===${RESET}"

    if [[ ! -f "$HEAVY_GRAPH" ]]; then
        fail "heavy_traffic.txt not found - place it in the project root"
        return
    fi

    info "Running heavy_traffic.txt with 45s timeout (4 travelers, bottleneck node 4)..."
    run_sim_headless "$HEAVY_GRAPH" 45
    local hc=$?
    if [[ $hc -eq 124 ]]; then
        fail "DEADLOCK DETECTED - sim did not finish within 45s"
    elif [[ $hc -eq 0 ]]; then
        pass "No deadlock - heavy_traffic.txt completed"
    else
        fail "sim exited with non-zero code $hc on heavy_traffic.txt"
    fi

    local STAR_GRAPH="$TMP_DIR/star.txt"
    cat > "$STAR_GRAPH" <<'EOF'
5 4
0 4 1
1 4 1
2 4 1
4 3 1
3
0 3
1 3
2 3
EOF
    info "Running starvation probe (3-way race to star centre)..."
    run_sim_headless "$STAR_GRAPH" 30
    local sc=$?
    if [[ $sc -eq 124 ]]; then
        fail "STARVATION SUSPECTED - star-graph sim hung for 30s"
    elif [[ $sc -eq 0 ]]; then
        pass "No starvation - star-graph completed"
    else
        fail "star-graph exited with non-zero code $sc"
    fi
}

# ------------------------------------------------------------
# Suite 4: Error handling
# ------------------------------------------------------------
check_error_handling() {
    echo ""
    echo -e "${YELLOW}=== Suite 4: Error Handling ===${RESET}"

    info "Passing a non-existent file..."
    if DISPLAY="" "$SIM" "/tmp/does_not_exist_xyz987.txt" >/dev/null 2>&1; then
        fail "sim exited 0 on missing file (should return non-zero)"
    else
        pass "sim exited non-zero on missing input file"
    fi

    info "Running sim with no arguments..."
    if DISPLAY="" "$SIM" >/dev/null 2>&1; then
        fail "sim exited 0 with no arguments"
    else
        pass "sim exited non-zero when called with no arguments"
    fi

    local BAD_GRAPH="$TMP_DIR/bad.txt"
    echo "not a valid graph file !@#" > "$BAD_GRAPH"
    info "Passing a malformed graph file..."
    DISPLAY="" timeout 5 "$SIM" "$BAD_GRAPH" >/dev/null 2>&1
    local bc=$?
    if [[ $bc -eq 124 ]]; then
        fail "sim HUNG on malformed input"
    elif [[ $bc -eq 0 ]]; then
        fail "sim exited 0 on malformed input"
    else
        pass "sim exited non-zero on malformed graph file (code=$bc)"
    fi
}

# ------------------------------------------------------------
# Suite 5: Lock integrity (requires tester_m6.c hooks compiled in)
# ------------------------------------------------------------
check_lock_integrity() {
    echo ""
    echo -e "${YELLOW}=== Suite 5: Lock Integrity ===${RESET}"

    if [[ ! -f "$HEAVY_GRAPH" ]]; then
        fail "heavy_traffic.txt not found - skipping"
        return
    fi

    rm -f "$LOG_FILE"
    info "Running heavy_traffic.txt to generate node-access log..."
    run_sim_headless "$HEAVY_GRAPH" 45
    local hc=$?

    if [[ $hc -eq 124 ]]; then
        fail "sim timed out - cannot verify lock integrity"
        return
    fi

    if [[ ! -f "$LOG_FILE" ]]; then
        info "Log file not found at $LOG_FILE"
        info "Add log hooks to milestone5.c and recompile to enable this check"
        info "(see README for instructions)"
        return
    fi

    info "Analysing $LOG_FILE for concurrent-node violations..."
    python3 - "$LOG_FILE" <<'PYEOF'
import sys, collections
log_path = sys.argv[1]
pending  = {}
intervals = []
with open(log_path) as fh:
    for lineno, line in enumerate(fh, 1):
        parts = line.split()
        if len(parts) < 4:
            continue
        tag, tid, nid, ts = parts[0], int(parts[1]), int(parts[2]), float(parts[3])
        key = (tid, nid)
        if tag == "ENTER":
            pending[key] = ts
        elif tag == "EXIT" and key in pending:
            intervals.append((nid, pending.pop(key), ts, tid))
by_node = collections.defaultdict(list)
for nid, ent, ext, tid in intervals:
    by_node[nid].append((ent, ext, tid))
total_violations = 0
for nid in sorted(by_node):
    ivs = sorted(by_node[nid], key=lambda x: x[0])
    for i in range(len(ivs) - 1):
        ent_a, ext_a, tid_a = ivs[i]
        ent_b, ext_b, tid_b = ivs[i+1]
        if ext_a > ent_b:
            print(f"  VIOLATION node={nid}: traveler {tid_a} [{ent_a:.3f}-{ext_a:.3f}] overlaps traveler {tid_b} entry {ent_b:.3f}")
            total_violations += 1
print(f"\n  Intervals checked : {len(intervals)}")
if total_violations == 0:
    print("  Result            : PASS - no overlap violations")
else:
    print(f"  Result            : FAIL - {total_violations} violation(s)")
sys.exit(0 if total_violations == 0 else 1)
PYEOF

    if [[ $? -eq 0 ]]; then
        pass "Lock integrity check PASSED - no concurrent-node violations"
    else
        fail "Lock integrity check FAILED - overlap violations detected"
    fi
}

# ------------------------------------------------------------
# Suite 6: Valgrind
# ------------------------------------------------------------
check_valgrind() {
    echo ""
    echo -e "${YELLOW}=== Suite 6: Valgrind ===${RESET}"

    if $SKIP_VALGRIND; then
        info "Valgrind skipped (--skip-valgrind flag)"
        return
    fi

    if ! command -v valgrind &>/dev/null; then
        info "valgrind not installed - skipping (sudo apt-get install valgrind)"
        return
    fi

    if [[ ! -f "$NORMAL_GRAPH" ]]; then
        cat > "$NORMAL_GRAPH" <<'EOF'
6 8
0 1 4
0 2 2
1 3 5
2 1 1
2 3 8
3 4 2
4 5 3
2 5 15
2
0 5
2 4
EOF
    fi

    info "Running sim under valgrind --leak-check=full (output: /tmp/m6_valgrind.log)..."
    DISPLAY="" timeout 90 valgrind \
        --leak-check=full \
        --track-origins=yes \
        --error-exitcode=1 \
        --log-file=/tmp/m6_valgrind.log \
        "$SIM" "$NORMAL_GRAPH" >/dev/null 2>&1
    local vc=$?

    if [[ $vc -eq 124 ]]; then
        fail "Valgrind run TIMED OUT after 90s"
        return
    fi

    if grep -q "definitely lost: 0 bytes" /tmp/m6_valgrind.log && \
       ! grep -qv "0 errors" <(grep "ERROR SUMMARY:" /tmp/m6_valgrind.log); then
        pass "Valgrind: zero memory leaks, zero errors"
    else
        fail "Valgrind reported leaks or errors - see /tmp/m6_valgrind.log"
    fi
}

# ------------------------------------------------------------
# Suite 7: Repeatability
# ------------------------------------------------------------
check_repeatability() {
    echo ""
    echo -e "${YELLOW}=== Suite 7: Repeatability (3x heavy_traffic.txt) ===${RESET}"

    if [[ ! -f "$HEAVY_GRAPH" ]]; then
        info "heavy_traffic.txt not found - skipping"
        return
    fi

    for run in 1 2 3; do
        info "Run $run/3..."
        run_sim_headless "$HEAVY_GRAPH" 45
        local rc=$?
        if [[ $rc -eq 124 ]]; then
            fail "Run $run/3 TIMED OUT (intermittent deadlock?)"
        elif [[ $rc -eq 0 ]]; then
            pass "Run $run/3 completed successfully"
        else
            fail "Run $run/3 exited with code $rc"
        fi
    done
}

# ============================================================
# DISPATCH
# ============================================================

echo ""
echo -e "${YELLOW}=== Milestone 6 Synchronisation Test Harness ===${RESET}"

case "$ONLY_SUITE" in
    compile)   check_compilation    ;;
    sanity)    run_sanity_checks    ;;
    deadlock)  detect_deadlocks     ;;
    error)     check_error_handling ;;
    integrity) check_lock_integrity ;;
    valgrind)  check_valgrind       ;;
    repeat)    check_repeatability  ;;
    "")
        check_compilation
        run_sanity_checks
        detect_deadlocks
        check_error_handling
        check_lock_integrity
        check_valgrind
        check_repeatability
        ;;
    *)
        echo "Unknown suite '$ONLY_SUITE'"
        echo "Options: compile | sanity | deadlock | error | integrity | valgrind | repeat"
        exit 1
        ;;
esac

# ============================================================
# Summary
# ============================================================
echo ""
echo -e "${YELLOW}======================================${RESET}"
echo -e "  Results: ${GREEN}${PASS} PASSED${RESET} / ${RED}${FAIL} FAILED${RESET} / $((PASS+FAIL)) TOTAL"
echo -e "${YELLOW}======================================${RESET}"
echo ""

[[ $FAIL -eq 0 ]] && exit 0 || exit 1