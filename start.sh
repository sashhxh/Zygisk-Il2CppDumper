#!/system/bin/sh
# Standalone launcher r42 xvcen UI / Vulkan for Oxide 1.16.11978 ARM64.
# Android ARM64, root; both native files are embedded below.
#
# USAGE AFTER BUILD:
#   Compile int_src with NDK (ndk-build in int_src/internal/jni/) to produce:
#     libcfesp.so   — the cheat library
#     cfinject      — the injector binary
#   Then run:
#     python3 int_src/internal/package_launcher.py \
#       --injector path/to/cfinject \
#       --library  path/to/libcfesp.so \
#       --template int_src/internal/launcher-template.sh \
#       --output   start.sh
#   This replaces @@PAYLOAD_FUNCTION@@, @@INJECTOR_SHA@@, @@LIBRARY_SHA@@
#   with base64-embedded binaries and SHA256 checksums.
#
# The three @@...@@ tokens below are template placeholders filled by package_launcher.py.
set -eu

fail() { printf 'Ошибка: %s\n' "$*" >&2; exit 1; }

LOAD_LIBRARY=1
UNITY_GRAPHICS='-force-vulkan'
[ "$#" -le 1 ] || fail 'Лишние аргументы.'
case "${1:-}" in
    -h|--help)
        printf '%s\n' \
            'Запуск: su -c "sh /sdcard/Download/start.sh"' \
            'Требуются Android ARM64, root и установленная Oxide.' \
            'Библиотека и cfinject уже внутри скрипта; сборка и интернет не нужны.' \
            'Скрипт перезапускает игру с Vulkan и подключает библиотеку.' \
            'После загрузки автоматически проверяет кадры меню и сохраняет отчёт.' \
            '--without-library: тот же режим графики, без загрузки библиотеки.' \
            '--stock: штатный запуск игры без библиотеки и без выбора графического API.' \
            'Политика устройства должна разрешать ptrace и загрузку библиотеки.'
        exit 0 ;;
    --without-library) LOAD_LIBRARY=0 ;;
    --stock) LOAD_LIBRARY=0; UNITY_GRAPHICS='' ;;
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
APP_UID=$(cmd package list packages -U --user "$USER_ID" "$PACKAGE" 2>/dev/null |
    awk -v package="package:$PACKAGE" '$1 == package { for (i=2; i<=NF; i++) if ($i ~ /^uid:[0-9]+$/) { sub(/^uid:/,"",$i); print $i; exit } }')
case "$APP_UID" in ''|*[!0-9]*) fail 'Не удалось определить UID установленной игры.' ;; esac
[ "$((APP_UID / 100000))" = "$USER_ID" ] || fail 'UID игры не соответствует выбранному пользователю Android.'
PROC_ROOT=/proc

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
if [ "$LOAD_LIBRARY" = 1 ]; then
    RUNTIME=$(mktemp -d /data/local/tmp/cfesp-launch.XXXXXXXX) || fail 'Не создать рабочую папку.'
fi

@@PAYLOAD_FUNCTION@@

if [ "$LOAD_LIBRARY" = 1 ]; then
    unpack_payload
    ACTUAL=$(sha256sum "$RUNTIME/cfinject")
    [ "${ACTUAL%% *}" = '@@INJECTOR_SHA@@' ] || fail 'Повреждён встроенный cfinject.'
    ACTUAL=$(sha256sum "$RUNTIME/libcfesp.so")
    [ "${ACTUAL%% *}" = '@@LIBRARY_SHA@@' ] || fail 'Повреждена встроенная библиотека.'
    chmod 755 "$RUNTIME" "$RUNTIME/cfinject"
    chmod 644 "$RUNTIME/libcfesp.so"
fi

candidate_pids() {
    pidof "$PACKAGE" 2>/dev/null || :
    # pidof may miss a process during Android startup. Read the UID's leaders
    # directly as well; find_game_pid still checks exact argv, UID and state.
    awk -v uid="$APP_UID" '/^Uid:/ && $2 == uid { n=split(FILENAME,a,"/"); print a[n-1] }' \
        "$PROC_ROOT"/[0-9]*/status 2>/dev/null || :
}

has_mapped_library() {
    [ -r "$1" ] || return 1
    awk -v library="$2" 'NF >= 6 { path=$6; sub(/^.*\//,"",path); if(path == library) found=1 } END { exit !found }' "$1" 2>/dev/null
}

find_game_pid() {
    GAME_PID=''
    GAME_PIDS=''
    UNITY_PIDS=''
    MAIN_PIDS=''
    for candidate in $(candidate_pids); do
        case "$candidate" in ''|*[!0-9]*) continue ;; esac
        case " $GAME_PIDS " in *" $candidate "*) continue ;; esac
        [ -r "$PROC_ROOT/$candidate/status" ] && [ -r "$PROC_ROOT/$candidate/cmdline" ] || continue
        identity=$(awk '/^Uid:/ { u=$2 } /^Tgid:/ { t=$2 } /^State:/ { s=$2 } /^PPid:/ { p=$2 } END { print u,t,s,p }' "$PROC_ROOT/$candidate/status" 2>/dev/null) || continue
        set -- $identity
        [ "$#" = 4 ] || continue
        [ "$1" = "$APP_UID" ] && [ "$2" = "$candidate" ] || continue
        case "$3" in Z|X|x) continue ;; esac
        parent=$4
        case "$parent" in ''|*[!0-9]*) continue ;; esac
        name=$(tr '\000' '\n' < "$PROC_ROOT/$candidate/cmdline" 2>/dev/null | head -n 1) || continue
        [ "$name" = "$PACKAGE" ] || continue
        GAME_PIDS="${GAME_PIDS:+$GAME_PIDS }$candidate"
        if has_mapped_library "$PROC_ROOT/$candidate/maps" libunity.so; then
            UNITY_PIDS="${UNITY_PIDS:+$UNITY_PIDS }$candidate"
        fi
        # A forked child can temporarily inherit the exact argv and mappings.
        # Android's main app process comes directly from a zygote.
        if [ -r "$PROC_ROOT/$parent/cmdline" ]; then
            parent_name=$(tr '\000' '\n' < "$PROC_ROOT/$parent/cmdline" 2>/dev/null | head -n 1) || continue
            case "${parent_name##*/}" in zygote|zygote64)
                MAIN_PIDS="${MAIN_PIDS:+$MAIN_PIDS }$candidate" ;;
            esac
        fi
    done
    candidates=${MAIN_PIDS:-$GAME_PIDS}
    loaded_matches=''
    for candidate in $candidates; do
        case " $UNITY_PIDS " in *" $candidate "*) loaded_matches="${loaded_matches:+$loaded_matches }$candidate" ;; esac
    done
    candidates=${loaded_matches:-$candidates}
    set -- $candidates
    if [ "$#" = 1 ]; then GAME_PID=$1; return; fi
    # Multiple live main processes are allowed to settle during startup.
    # Never select a random PID or stop another Android user's game.
    return 0
}

restart_game() {
    printf 'Лоадер r32: перезапускаю Oxide, графика: %s. Текущая сессия игры закроется.\n' "${UNITY_GRAPHICS:-штатная}"
    find_game_pid
    if [ -n "$GAME_PIDS" ]; then
        printf 'Останавливаю игру пользователя %s: PID %s\n' "$USER_ID" "$GAME_PIDS"
    fi
    am force-stop --user "$USER_ID" "$PACKAGE" || fail 'Android не остановил игру.'
    stop_wait=10
    while :; do
        find_game_pid
        [ -n "$GAME_PIDS" ] || break
        [ "$stop_wait" -gt 0 ] || fail 'Процесс игры остался запущен. Загрузка отменена, чтобы не использовать старый рендерер.'
        sleep 1
        stop_wait=$((stop_wait - 1))
    done
    # Unity reads this extra at process startup. Bringing an existing task to
    # the foreground does not switch its already initialized graphics backend.
    am start --user "$USER_ID" -n "$COMPONENT" \
        -a android.intent.action.MAIN -c android.intent.category.LAUNCHER \
        --es unity "$UNITY_GRAPHICS" || fail 'Android не запустил игру.'
}

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
    NATIVE_REF="$INSTALL_DIR/lib/arm64/libunity.so"
    [ -f "$NATIVE_REF" ] || fail 'Не найдена извлечённая ARM64 libunity.so в установленной игре.'
    NATIVE_REF=$(readlink -f "$NATIVE_REF") || fail 'Не удалось определить путь libunity.so.'
    case "$NATIVE_REF" in
        "$INSTALL_DIR"/*) ;;
        *) fail 'libunity.so находится вне каталога выбранной игры; загрузка отменена.' ;;
    esac
    [ -f "$NATIVE_REF" ] || fail 'Установленная libunity.so недоступна.'
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

read_launch_log() {
    if [ -x /system/bin/logcat ]; then
        /system/bin/logcat -d -v brief --pid="$GAME_PID" -s MANGOcheat 2>&1 | tail -n 120
    else
        printf '%s\n' 'Системный logcat недоступен.'
    fi
}

diagnose_launch() {
    REPORT_DIR=$(mktemp -d "$REPORT_ROOT/cfesp-r32-report.XXXXXXXX") || return 0
    REPORT="$REPORT_DIR/launch.txt"
    printf '%s\n' 'Проверяю Vulkan-меню (до 15 секунд)...'
    report_wait=15
    while :; do
        {
            printf 'Лоадер r32; PID %s; графика %s\n' "$GAME_PID" "$UNITY_GRAPHICS"
            read_launch_log
        } > "$REPORT"
        if grep -q 'Vulkan UI frame submitted' "$REPORT"; then
            printf '%s\n' 'Первый кадр Vulkan-меню отправлен на GPU.'
            break
        fi
        [ "$report_wait" -gt 0 ] || break
        sleep 1
        report_wait=$((report_wait - 1))
    done
    chmod 755 "$REPORT_DIR"
    chmod 644 "$REPORT"
    if ! grep -q 'Vulkan UI frame submitted' "$REPORT"; then
        if grep -q 'boot:.*MANGOCHEAT_REV=r[0-9]' "$REPORT"; then
            printf '%s\n' 'Библиотека запущена, но Vulkan-меню ещё не готово. Сохрани отчёт ниже.'
        else
            printf '%s\n' 'Нет стартового сообщения библиотеки. Подключение меню пока не подтверждено.'
        fi
        cat "$REPORT"
    fi
    printf 'Отчёт запуска: %s\n' "$REPORT"
}

clock_seconds() {
    clock_value=''
    IFS=' ' read -r clock_value clock_idle < "$PROC_ROOT/uptime" || fail 'Не удалось прочитать системный таймер.'
    WAIT_NOW=${clock_value%%.*}
    case "$WAIT_NOW" in ''|*[!0-9]*) fail 'Неверное значение системного таймера.' ;; esac
}

wait_for_unity() {
    READY=0
    ALREADY_LOADED=0
    clock_seconds
    wait_deadline=$((WAIT_NOW + 90))
    next_wait_report=$WAIT_NOW
    wait_last_state=''
    WAIT_REASON='Процесс игры ещё не найден.'
    while :; do
        find_game_pid
        if [ -z "$GAME_PID" ]; then
            if [ -n "$GAME_PIDS" ]; then
                WAIT_REASON="Выбираю главный процесс; UID $APP_UID, кандидаты: $GAME_PIDS."
            else
                WAIT_REASON="Процесс игры UID $APP_UID ещё не найден."
            fi
        elif [ ! -r "$PROC_ROOT/$GAME_PID/maps" ]; then
            WAIT_REASON="PID $GAME_PID найден, но список библиотек недоступен."
        elif has_mapped_library "$PROC_ROOT/$GAME_PID/maps" libcfesp.so; then
            ALREADY_LOADED=1
            return 0
        elif has_mapped_library "$PROC_ROOT/$GAME_PID/maps" libunity.so; then
            READY=1
            printf 'Unity найдена: PID %s, UID %s.\n' "$GAME_PID" "$APP_UID"
            return 0
        else
            WAIT_REASON="PID $GAME_PID найден, Unity ещё не загружена."
        fi
        # Use elapsed monotonic time, including time spent checking procfs.
        # Counting 1800 sleeps did not impose a 90-second timeout on a slow CPU.
        clock_seconds
        wait_left=$((wait_deadline - WAIT_NOW))
        [ "$wait_left" -gt 0 ] || wait_left=0
        if [ "$WAIT_REASON" != "$wait_last_state" ] || [ "$WAIT_NOW" -ge "$next_wait_report" ]; then
            printf 'Ожидание, осталось %s с: %s\n' "$wait_left" "$WAIT_REASON"
            wait_last_state=$WAIT_REASON
            next_wait_report=$((WAIT_NOW + 5))
        fi
        [ "$WAIT_NOW" -lt "$wait_deadline" ] || return 1
        sleep 0.05
    done
}

read_startup_log() {
    [ -x /system/bin/logcat ] || return 0
    if [ -n "$GAME_PID" ]; then
        /system/bin/logcat -d -v brief --pid="$GAME_PID" -s MANGOcheat Unity AndroidRuntime 2>&1 | tail -n 50
    else
        /system/bin/logcat -d -v brief -s ActivityManager ActivityTaskManager 2>&1 | grep -F "$PACKAGE" | tail -n 30 || :
    fi
}

diagnose_startup() {
    REPORT_DIR=$(mktemp -d "$REPORT_ROOT/cfesp-r32-report.XXXXXXXX") || return 0
    REPORT="$REPORT_DIR/startup.txt"
    {
        printf 'Лоадер r32; пользователь %s; UID %s; Activity %s\n' "$USER_ID" "$APP_UID" "$COMPONENT"
        printf 'Ожидание: %s\n' "$WAIT_REASON"
        printf 'PID: %s; кандидаты: %s\n' "${GAME_PID:-нет}" "${GAME_PIDS:-нет}"
        for report_pid in $(candidate_pids); do
            case "$report_pid" in ''|*[!0-9]*) continue ;; esac
            printf '\nПроцесс %s:\n' "$report_pid"
            if [ -r "$PROC_ROOT/$report_pid/cmdline" ]; then tr '\000' '\n' < "$PROC_ROOT/$report_pid/cmdline" 2>/dev/null | head -n 1 || :; fi
            if [ -r "$PROC_ROOT/$report_pid/status" ]; then awk '/^(State|Tgid|PPid|Uid):/' "$PROC_ROOT/$report_pid/status" 2>/dev/null || :; fi
            if [ -r "$PROC_ROOT/$report_pid/maps" ]; then awk '/libunity|libil2cpp|libvulkan|base.apk/ { print }' "$PROC_ROOT/$report_pid/maps" 2>/dev/null | tail -n 12 || :; fi
        done
        read_startup_log
    } > "$REPORT"
    chmod 755 "$REPORT_DIR"
    chmod 644 "$REPORT"
    cat "$REPORT"
    printf 'Отчёт ожидания Unity: %s\n' "$REPORT"
}

REPORT_ROOT=/data/local/tmp
if [ "$LOAD_LIBRARY" = 1 ]; then prepare_library; fi

restart_game
if [ "$LOAD_LIBRARY" = 0 ]; then
    printf '%s\n' 'Игра запущена без библиотеки. Проверь те же заборы, стены и двери.'
    exit 0
fi
printf '%s\n' 'Жду загрузку Unity для подключения Vulkan (до 90 секунд)...'
if ! wait_for_unity; then
    diagnose_startup
    fail 'Unity не дождался. Причина и отчёт указаны выше; библиотека не загружалась.'
fi
if [ "$ALREADY_LOADED" = 1 ]; then
    printf '%s\n' 'libcfesp.so уже загружена. Повторную загрузку пропускаю.'
    diagnose_launch
    exit 0
fi
ELF_CLASS=$(od -An -tu1 -j4 -N1 "/proc/$GAME_PID/exe" | tr -d ' \n')
[ "$ELF_CLASS" = 2 ] || fail 'Процесс игры должен быть 64-битным.'


printf 'Загружаю библиотеку в PID %s...\n' "$GAME_PID"
if "$RUNTIME/cfinject" "$PACKAGE" "$LIBRARY" "$GAME_PID" "$APP_UID"; then
    sleep 2
    if [ -r "/proc/$GAME_PID/maps" ] && grep -Fq "$LIBRARY" "/proc/$GAME_PID/maps"; then
        printf '%s\n' 'dlopen выполнен, библиотека присутствует в процессе игры.' \
            'Появление меню проверяй в игре.'
        diagnose_launch
        printf 'Логи только нового процесса: /system/bin/logcat --pid=%s -s MANGOcheat\n' "$GAME_PID"
    else
        diagnose_launch
        fail 'После загрузки процесс завершился или библиотека отсутствует. Нужен logcat.'
    fi
else
    printf 'SELinux: %s\n' "$SELINUX_MODE" >&2
    ls -Zd "$LIBRARY_DIR" "$LIBRARY" >&2 || :
    fail 'Загрузчик вернул ошибку (см. выше). Root сам по себе не гарантирует разрешение ptrace/dlopen.'
fi
