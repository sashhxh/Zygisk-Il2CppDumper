#!/bin/sh
# MANGOcheat: project-relative Android NDK build.
set -eu
PROJECT_DIR=$(CDPATH= cd -- "$(dirname -- "$0")" && pwd)
NDK_BIN=""
for candidate in "${ANDROID_NDK_HOME:-}" "${ANDROID_NDK_ROOT:-}" "${NDK_HOME:-}"; do
    if [ -n "$candidate" ] && [ -x "$candidate/ndk-build" ]; then NDK_BIN="$candidate/ndk-build"; break; fi
done
if [ -z "$NDK_BIN" ] && command -v ndk-build >/dev/null 2>&1; then NDK_BIN=$(command -v ndk-build); fi
if [ -z "$NDK_BIN" ]; then
    for candidate in \
      /data/user/0/com.aide.ui.crustacean/no_backup/ndksupport-1710240003/android-ndk-aide \
      /data/user/0/com.aide.clone/no_backup/ndksupport-1710240003/android-ndk-aide; do
        if [ -x "$candidate/ndk-build" ]; then NDK_BIN="$candidate/ndk-build"; break; fi
    done
fi
[ -n "$NDK_BIN" ] || { echo "NDK bulunamadi. ANDROID_NDK_HOME ayarla."; exit 1; }
[ -s "$PROJECT_DIR/jni/dobby-source/Android.mk" ] || { echo "Dobby kaynaklari eksik."; exit 1; }
exec "$NDK_BIN" -C "$PROJECT_DIR" NDK_PROJECT_PATH="$PROJECT_DIR" "$@"
