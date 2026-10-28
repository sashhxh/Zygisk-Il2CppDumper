#!/system/bin/sh
# Standalone launcher v8 / r12f-safe-guard-hooks for Oxide 1.16.11978 ARM64.
# Android ARM64, root; both native files are embedded below.
set -eu

fail() { printf 'Ошибка: %s\n' "$*" >&2; exit 1; }

case "${1:-}" in
    -h|--help)
        printf '%s\n' \
            'Запуск: su -c "sh /sdcard/Download/start.sh"' \
            'Требуются Android ARM64, root и установленная Oxide.' \
            'Библиотека и cfinject уже внутри скрипта; сборка и интернет не нужны.' \
            'Скрипт полностью перезапускает игру с -force-gles30 и загружает libcfesp.so.' \
            'Политика устройства должна разрешать ptrace и загрузку библиотеки.'
        exit 0 ;;
    '') [ "$#" -eq 0 ] || fail 'Лишние аргументы.' ;;
    *) fail 'Неизвестный аргумент. Используй --help.' ;;
esac

[ -x /system/bin/am ] || fail 'Запускай этот файл на Android.'
[ "$(id -u)" = 0 ] || fail 'Нужен root: su -c "sh /sdcard/Download/start.sh"'
case "$(uname -m)" in aarch64|arm64) ;; *) fail 'Нужен Android ARM64.' ;; esac
for tool in base64 sha256sum mktemp pidof tr head tail od grep awk dirname readlink cp ls sed; do
    command -v "$tool" >/dev/null 2>&1 || fail "Нет команды $tool."
done

PACKAGE=com.catsbit.oxidesurvivalisland
USER_ID=$(am get-current-user)
case "$USER_ID" in ''|*[!0-9]*) fail 'Не удалось определить пользователя Android.' ;; esac
COMPONENT=$(cmd package resolve-activity --brief --user "$USER_ID" \
    -a android.intent.action.MAIN -c android.intent.category.LAUNCHER \
    "$PACKAGE" 2>/dev/null | tail -n 1)
case "$COMPONENT" in "$PACKAGE"/*) ;; *) fail 'Не найдена установленная игра Oxide.' ;; esac

RUNTIME=''
LIBRARY_DIR=''
cleanup() {
    if [ -n "$LIBRARY_DIR" ]; then
        rm -f "$LIBRARY_DIR/libcfesp.so"
        rmdir "$LIBRARY_DIR" 2>/dev/null || :
    fi
    if [ -n "$RUNTIME" ]; then
        rm -f "$RUNTIME/cfinject" "$RUNTIME/libcfesp.so"
        rmdir "$RUNTIME" 2>/dev/null || :
    fi
}
trap cleanup EXIT
trap 'exit 129' HUP
trap 'exit 130' INT
trap 'exit 143' TERM
umask 077
RUNTIME=$(mktemp -d /data/local/tmp/cfesp-launch.XXXXXXXX) || fail 'Не создать рабочую папку.'

@@PAYLOAD_FUNCTION@@

unpack_payload
ACTUAL=$(sha256sum "$RUNTIME/cfinject")
[ "${ACTUAL%% *}" = '@@INJECTOR_SHA@@' ] || fail 'Повреждён встроенный cfinject.'
ACTUAL=$(sha256sum "$RUNTIME/libcfesp.so")
[ "${ACTUAL%% *}" = '@@LIBRARY_SHA@@' ] || fail 'Повреждена встроенная библиотека.'
chmod 755 "$RUNTIME" "$RUNTIME/cfinject"
chmod 644 "$RUNTIME/libcfesp.so"

find_game_pid() {
    GAME_PID=''
    for candidate in $(pidof "$PACKAGE" 2>/dev/null || :); do
        case "$candidate" in ''|*[!0-9]*) continue ;; esac
        [ -r "/proc/$candidate/cmdline" ] || continue
        name=$(tr '\000' '\n' < "/proc/$candidate/cmdline" | head -n 1)
        if [ "$name" = "$PACKAGE" ]; then
            [ -z "$GAME_PID" ] || fail 'Открыто несколько экземпляров игры. Оставь один.'
            GAME_PID=$candidate
        fi
    done
}

restart_game() {
    printf '%s\n' 'Лоадер v8 (r12f-safe-guard-hooks): перезапускаю Oxide с -force-gles30. Текущая сессия игры закроется.'
    find_game_pid
    if [ -n "$GAME_PID" ]; then
        printf 'Останавливаю старый процесс: PID %s\n' "$GAME_PID"
    fi
    am force-stop --user "$USER_ID" "$PACKAGE" || fail 'Android не остановил игру.'
    stop_wait=10
    while :; do
        find_game_pid
        [ -n "$GAME_PID" ] || break
        [ "$stop_wait" -gt 0 ] || fail 'Процесс игры остался запущен. Загрузка отменена, чтобы не использовать старый рендерер.'
        sleep 1
        stop_wait=$((stop_wait - 1))
    done
    # Unity reads this extra at process startup. Bringing an existing task to
    # the foreground does not switch its already initialized graphics backend.
    am start --user "$USER_ID" -W -n "$COMPONENT" \
        -a android.intent.action.MAIN -c android.intent.category.LAUNCHER \
        --es unity '-force-gles30' || fail 'Android не запустил игру.'
}

restart_game
printf '%s\n' 'Жду процесс игры и libil2cpp.so (до 90 секунд)...'
remaining=90
READY=0
while [ "$remaining" -gt 0 ]; do
    find_game_pid
    if [ -n "$GAME_PID" ] && [ -r "/proc/$GAME_PID/maps" ]; then
        if grep -q '/libcfesp\.so\( \|$\)' "/proc/$GAME_PID/maps"; then
            printf '%s\n' 'libcfesp.so уже загружена. Повторную загрузку пропускаю.'
            exit 0
        fi
        if grep -q 'libil2cpp\.so' "/proc/$GAME_PID/maps"; then
            READY=1
            break
        fi
    fi
    sleep 1
    remaining=$((remaining - 1))
done
[ "$READY" = 1 ] || fail 'Не дождался libil2cpp.so. Игра могла завершиться; загрузчик не запускался.'
ELF_CLASS=$(od -An -tu1 -j4 -N1 "/proc/$GAME_PID/exe" | tr -d ' \n')
[ "$ELF_CLASS" = 2 ] || fail 'Процесс игры должен быть 64-битным.'

# /data/local/tmp is commonly shell_data_file: ptrace can succeed while the
# target app is denied executable mappings from it. Use the installed game's
# own native-library directory and copy the reference library's SELinux label.
# Only the new temporary directory and our own file are changed or removed.
prepare_library() {
    APK_PATH=$(pm path --user "$USER_ID" "$PACKAGE" | sed -n 's/^package://p' | head -n 1)
    [ -n "$APK_PATH" ] || fail 'Не удалось определить каталог установленной игры.'
    APK_PATH=$(readlink -f "$APK_PATH") || fail 'Не удалось определить путь APK.'
    INSTALL_DIR=$(dirname "$APK_PATH")
    case "$INSTALL_DIR" in
        /data/app/*|/mnt/expand/*/app/*) ;;
        *) fail 'Игра должна быть установлена как обычное приложение в /data/app или /mnt/expand.' ;;
    esac
    NATIVE_REF=$(awk '$NF ~ /\/libil2cpp\.so$/ { print $NF; exit }' "/proc/$GAME_PID/maps")
    [ -n "$NATIVE_REF" ] || fail 'Не найден путь установленной libil2cpp.so.'
    NATIVE_REF=$(readlink -f "$NATIVE_REF") || fail 'Не удалось определить путь libil2cpp.so.'
    case "$NATIVE_REF" in
        "$INSTALL_DIR"/*) ;;
        *) fail 'libil2cpp.so находится вне каталога выбранной игры; загрузка отменена.' ;;
    esac
    [ -f "$NATIVE_REF" ] || fail 'Установленная libil2cpp.so недоступна.'
    NATIVE_DIR=$(dirname "$NATIVE_REF")
    LIBRARY_DIR=$(mktemp -d "$NATIVE_DIR/cfesp-launch.XXXXXXXX") \
        || fail 'Не создать временный каталог рядом с библиотеками игры.'
    LIBRARY="$LIBRARY_DIR/libcfesp.so"
    cp "$RUNTIME/libcfesp.so" "$LIBRARY" || fail 'Не удалось скопировать библиотеку.'
    chmod 755 "$LIBRARY_DIR"
    chmod 644 "$LIBRARY"
    SELINUX_MODE=$(getenforce 2>/dev/null || printf 'Unknown')
    if [ "$SELINUX_MODE" != Disabled ]; then
        command -v chcon >/dev/null 2>&1 || fail 'Нет chcon для контекста библиотеки.'
        APP_CONTEXT=$(read_context "$NATIVE_REF")
        [ -n "$APP_CONTEXT" ] || fail 'Не удалось прочитать SELinux-контекст установленной библиотеки.'
        chcon "$APP_CONTEXT" "$LIBRARY_DIR" "$LIBRARY" \
            || fail 'Не удалось применить SELinux-контекст к новым файлам.'
        [ "$(read_context "$LIBRARY_DIR")" = "$APP_CONTEXT" ] \
            || fail 'SELinux-контекст временного каталога не совпал.'
        [ "$(read_context "$LIBRARY")" = "$APP_CONTEXT" ] \
            || fail 'SELinux-контекст новой библиотеки не совпал.'
    fi
    ACTUAL=$(sha256sum "$LIBRARY")
    [ "${ACTUAL%% *}" = '@@LIBRARY_SHA@@' ] || fail 'Копия библиотеки повреждена.'
}

read_context() {
    ls -Zd "$1" | awk '{ for (i=1; i<=NF; i++) if ($i ~ /^u:object_r:/) { print $i; exit } }'
}

prepare_library
printf 'Загружаю библиотеку в PID %s...\n' "$GAME_PID"
if "$RUNTIME/cfinject" "$PACKAGE" "$LIBRARY"; then
    sleep 2
    if [ -r "/proc/$GAME_PID/maps" ] && grep -Fq "$LIBRARY" "/proc/$GAME_PID/maps"; then
        printf '%s\n' 'dlopen выполнен, библиотека присутствует в процессе игры.' \
            'Появление меню проверяй в игре.'
        printf 'Логи только нового процесса: logcat --pid=%s -s Unity MANGOcheat OxideBypass\n' "$GAME_PID"
        LOG_FILE="/data/user/$USER_ID/$PACKAGE/files/mango_log.txt"
        printf 'Лог-файл: %s\n' "$LOG_FILE"
        printf 'Скопировать в Download: su -c "cp %s /sdcard/Download/mango_log.txt"\n' "$LOG_FILE"
        sleep 6
        if [ -r "$LOG_FILE" ]; then
            cp "$LOG_FILE" /sdcard/Download/mango_log.txt 2>/dev/null \
                && printf '%s\n' 'Лог скопирован: /sdcard/Download/mango_log.txt' || :
        else
            printf '%s\n' 'Лог-файл пока не создан: библиотека ещё не стартовала или нет доступа к files/.'
        fi
    else
        fail 'После загрузки процесс завершился или библиотека отсутствует. Нужен logcat.'
    fi
else
    printf 'SELinux: %s\n' "$SELINUX_MODE" >&2
    ls -Zd "$LIBRARY_DIR" "$LIBRARY" >&2 || :
    fail 'Загрузчик вернул ошибку (см. выше). Root сам по себе не гарантирует разрешение ptrace/dlopen.'
fi
