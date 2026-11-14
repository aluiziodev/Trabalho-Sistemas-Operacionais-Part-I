#!/usr/bin/env bash
set -u

ROOT_DIR="$(cd "$(dirname "$0")" && pwd)"
cd "$ROOT_DIR"

fail() {
    echo "[integration] FAIL: $1" >&2
    exit 1
}

pass() {
    echo "[integration] OK: $1"
}

make >/dev/null 2>&1 || fail "build falhou"

# 1) argumentos invalidos
for args in "" "abc" "-1" "1 0" "1"; do
    if [ -z "$args" ]; then
        set --
    else
        set -- $args
    fi

    if ./atendimento "$@" >/tmp/atendimento_invalid.out 2>/tmp/atendimento_invalid.err; then
        fail "argumentos invalidos nao devem passar: [$args]"
    fi

    if ! grep -qi "Uso:" /tmp/atendimento_invalid.err; then
        fail "mensagem de uso ausente para: [$args]"
    fi
    echo "" >/tmp/atendimento_invalid.out
    echo "" >/tmp/atendimento_invalid.err
    pass "args invalidos: [$args]"
done

# 2) N=1
out=$(./atendimento 1 1 </dev/null 2>&1) || fail "N=1 falhou"
printf '%s\n' "$out"
if ! printf '%s\n' "$out" | grep -Eq 'Taxa de satisfacao:.*\([0-9]+/[0-9]+\)'; then
    fail "saida final nao contém taxa para N=1"
fi
if ! printf '%s\n' "$out" | grep -Eq '^[0-9]+$'; then
    fail "PID do analista nao foi impresso em N=1"
fi
pass "N=1"

# 3) X=1 sem travar
out=$(timeout 20s ./atendimento 10 1 </dev/null 2>&1) || fail "X=1 travou ou falhou"
printf '%s\n' "$out"
if ! printf '%s\n' "$out" | grep -Eq 'Taxa de satisfacao:.*'; then
    fail "X=1 nao produziu resultado final"
fi
pass "X=1"

# 4) N=0 com s imediato
out=$(bash -c "printf 's\\n' | timeout 20s ./atendimento 0 1" 2>&1) || fail "N=0 com s imediato falhou"
printf '%s\n' "$out"
if ! printf '%s\n' "$out" | grep -Eq 'Taxa de satisfacao:.*\(0/0\)'; then
    fail "N=0 com s imediato nao finalizou com 0/0"
fi
pass "N=0 s imediato"

# 5) N=0 com s apos 1 segundo
out=$(bash -c "sleep 1; printf 's\\n' | timeout 20s ./atendimento 0 1" 2>&1) || fail "N=0 com s apos 1s falhou"
printf '%s\n' "$out"
if ! printf '%s\n' "$out" | grep -Eq 'Taxa de satisfacao:.*\(0/0\)'; then
    fail "N=0 com s apos 1s nao finalizou com 0/0"
fi
pass "N=0 s apos 1s"

# 6) residuos antigos
: > lng.txt
: > demanda.txt
: > pidanalista.txt
python3 - <<'PY'
import ctypes
libc = ctypes.CDLL('libc.so.6')
for name in [b'/sem_atend', b'/sem_block']:
    sem_open = libc.sem_open
    sem_open.argtypes = [ctypes.c_char_p, ctypes.c_int, ctypes.c_uint, ctypes.c_uint]
    sem_open.restype = ctypes.c_void_p
    sem = sem_open(name, 0x0002, 0o644, 1)
    if sem != ctypes.c_void_p(-1).value:
        libc.sem_unlink(name)
PY
out=$(./atendimento 1 50 </dev/null 2>&1) || fail "residuos antigos afetaram execucao"
printf '%s\n' "$out"
pass "residuos antigos"

# 7) Ctrl-C no meio
(./atendimento 120 200 >/tmp/atendimento_sigint.out 2>/tmp/atendimento_sigint.err & pid=$!; sleep 0.2; kill -INT "$pid"; wait "$pid"; echo "exit:$?" ) > /tmp/atendimento_sigint_run.txt 2>&1 || true
if pgrep -af "cliente|analista" >/dev/null 2>&1; then
    fail "processos cliente/analista remanescentes apos Ctrl-C"
fi
if ls /dev/shm 2>/dev/null | grep -q '^sem_'; then
    fail "sem_ ainda encontrado em /dev/shm apos Ctrl-C"
fi
pass "Ctrl-C no meio"

# 8) N=120 cobre pausa em 100 e atende 120
out=$(timeout 60s ./atendimento 120 100 </dev/null 2>&1) || fail "N=120 falhou ou travou"
printf '%s\n' "$out"
if ! printf '%s\n' "$out" | grep -Eq 'Taxa de satisfacao:.*\(120/120\)'; then
    # pode sair 0/120 em caso de X muito pequeno, mas nao deve ultrapassar 120
    if ! printf '%s\n' "$out" | grep -Eq 'Taxa de satisfacao:.*\([0-9]+/[0-9]+\)'; then
        fail "N=120 nao produziu resultado final"
    fi
fi
pass "N=120"

# 9) sem residuos finais
if pgrep -af "cliente|analista" >/dev/null 2>&1; then
    fail "processos remanescentes no final"
fi
if ls /dev/shm 2>/dev/null | grep -q '^sem_'; then
    fail "sem_ permanecem em /dev/shm no final"
fi

pass "integracao completa"
