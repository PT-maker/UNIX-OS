#!/bin/sh

TMP_DIR=""

remove_tmp() {
    if [ -n "$TMP_DIR" ] && [ -d "$TMP_DIR" ]; then
        rm -rf -- "$TMP_DIR"
    fi
}

trap remove_tmp EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 131' QUIT
trap 'exit 141' PIPE
trap 'exit 143' TERM

fail() {
    echo "[ОШИБКА] $2" >&2
    exit "$1"
}

if [ $# -lt 1 ]; then
    echo "Использование: $0 <исходный_файл>" >&2
    exit 1
fi

INPUT=$1

if [ ! -f "$INPUT" ]; then
    fail 2 "исходный файл '$INPUT' отсутствует"
fi

echo "[1/5] Читаю файл $INPUT"

RESULT_NAME=$(sed -n 's|^[[:space:]]*[/%#*]*[[:space:]]*Output:[[:space:]]*\([^[:space:]*]*\).*|\1|p' "$INPUT" | head -n 1)

if [ -z "$RESULT_NAME" ]; then
    fail 3 "в файле нет строки вида 'Output: имя_файла'"
fi

case "$RESULT_NAME" in
    */*) fail 3 "в имени результата нельзя использовать '/'" ;;
esac

echo "[2/5] Имя результата: $RESULT_NAME"

TMP_DIR=$(mktemp -d)

if [ -z "$TMP_DIR" ] || [ ! -d "$TMP_DIR" ]; then
    fail 4 "не получилось создать временный каталог"
fi

echo "[3/5] Временный каталог: $TMP_DIR"

SRC_DIR=$(cd "$(dirname "$INPUT")" && pwd)
SRC_FILE=$(basename "$INPUT")

cp "$INPUT" "$TMP_DIR/" || fail 4 "не удалось скопировать исходник во временный каталог"
cd "$TMP_DIR" || fail 4 "не удалось перейти во временный каталог"

case "$SRC_FILE" in
    *.c)            KIND=c ;;
    *.cpp|*.cc|*.cxx) KIND=cpp ;;
    *.tex)          KIND=tex ;;
    *)              KIND=unknown ;;
esac

echo "[4/5] Тип файла: $KIND, запускаю сборку"

case "$KIND" in
    c)
        cc -o "$RESULT_NAME" "$SRC_FILE" > build.log 2>&1
        STATUS=$?
        ;;
    cpp)
        c++ -o "$RESULT_NAME" "$SRC_FILE" > build.log 2>&1
        STATUS=$?
        ;;
    tex)
        BASE=${RESULT_NAME%.pdf}
        RESULT_NAME=$BASE.pdf
        TEXINPUTS="$SRC_DIR:"
        export TEXINPUTS
        pdflatex -interaction=nonstopmode -halt-on-error -jobname="$BASE" "$SRC_FILE" > build.log 2>&1
        STATUS=$?
        ;;
    *)
        fail 5 "расширение файла не поддерживается (нужно .c, .cpp, .cc, .cxx или .tex)"
        ;;
esac

if [ "$STATUS" -ne 0 ]; then
    echo "----- журнал сборки -----" >&2
    cat build.log >&2
    echo "-------------------------" >&2
    fail 6 "сборка завершилась неудачно (код $STATUS)"
fi

if [ ! -f "$RESULT_NAME" ]; then
    fail 7 "после сборки файл '$RESULT_NAME' не найден"
fi

cp "$RESULT_NAME" "$SRC_DIR/$RESULT_NAME" || fail 7 "не удалось сохранить результат в $SRC_DIR"

echo "[5/5] Готово: $SRC_DIR/$RESULT_NAME"
exit 0