#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

if [ "$#" -ne 3 ]; then
    echo "Uso: $0 N X R" >&2
    exit 1
fi

N="$1"
X="$2"
R="$3"

if ! [[ "$N" =~ ^[0-9]+$ ]] || ! [[ "$X" =~ ^[0-9]+$ ]] || ! [[ "$R" =~ ^[0-9]+$ ]]; then
    echo "Erro: N, X e R devem ser inteiros positivos." >&2
    exit 1
fi

if [ "$X" -le 0 ] || [ "$R" -le 0 ]; then
    echo "Erro: X e R devem ser maiores que zero." >&2
    exit 1
fi

BASE_CFLAGS="-Wall -Wextra -O2 -std=gnu11 -I./src"

run_policy() {
    local name="$1"
    local extra_flags="$2"

    make clean >/dev/null 2>&1 || true
    make CFLAGS="$BASE_CFLAGS $extra_flags" all >/dev/null 2>&1 || {
        echo "Erro ao compilar a versão $name." >&2
        return 1
    }

    python3 - "$N" "$X" "$R" "$name" <<'PY'
import re
import statistics
import subprocess
import sys

n = int(sys.argv[1])
x = int(sys.argv[2])
r = int(sys.argv[3])
name = sys.argv[4]

rates = []
times = []

for _ in range(r):
    proc = subprocess.run(["./atendimento", str(n), str(x)], capture_output=True, text=True)
    output = (proc.stdout or "") + (proc.stderr or "")

    if proc.returncode != 0:
        raise SystemExit(f"{name} falhou com retorno {proc.returncode}:\n{output}")

    match_rate = re.search(r"Taxa de satisfacao:\s*([0-9]+(?:\.[0-9]+)?)%\s*\((\d+)/(\d+)\)", output)
    if match_rate is None:
        raise SystemExit(f"{name} nao produziu taxa de satisfacao:\n{output}")
    rates.append(float(match_rate.group(1)))

    match_time = re.search(r"Tempo total:\s*(\d+)\s*ms", output)
    if match_time is None:
        raise SystemExit(f"{name} nao produziu tempo total:\n{output}")
    times.append(int(match_time.group(1)))

mean_rate = statistics.mean(rates)
std_rate = statistics.pstdev(rates) if len(rates) > 1 else 0.0
mean_time = statistics.mean(times)
std_time = statistics.pstdev(times) if len(times) > 1 else 0.0

print(f"{name}\t{mean_rate:.2f}\t{std_rate:.2f}\t{mean_time:.2f}\t{std_time:.2f}")
PY
}

printf 'Benchmark: N=%s X=%s R=%s\n' "$N" "$X" "$R"
printf '%s\t%s\t%s\t%s\t%s\n' "Policy" "mean_rate_%" "std_rate_%" "mean_time_ms" "std_time_ms"

run_policy "EDF" "" || exit 1
run_policy "FIFO" "-DPOLICY_FIFO" || exit 1

make clean >/dev/null 2>&1 || true
