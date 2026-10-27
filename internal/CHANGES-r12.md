# r12-colored-xray-world-camera

Проверено: хост-тесты (ASAN/UBSAN), сборка `libcfesp.so` под arm64 через NDK r26d.
**Не проверено:** запуск на устройстве, рендер, замер FPS (как и в r11, `android_device_tested=false`).

## Третье лицо: нет тела
Причины (по ассетам APK и коду):
- Коллизия камеры била лучом по `~(1<<2)`, т.е. по слоям Hitbox(10), Entity(11), Ragdoll(22) своей же модели.
  4 попытки «пропустить своё» заканчивались, возвращалось «стена за спиной», камера оставалась у головы, тело скрывалось.
  Теперь маска только мир: Default, Construction, Deployed, Terrain, Vehicle, LightingStatic, VehicleBox (слои из TagManager).
  Нет physics scene -> камера не блокируется (раньше 0 = тело скрыто навсегда).
- Слой 8 больше не означает «руки». Руки = под FPManager/оружием, либо слой 8 вне модели персонажа.
  Тело на слое 8 остаётся видимым, слой 8 убирается из маски камеры, только если на нём нет тела.
- Слой рендерера читается каждый кадр, а не при обнаружении.
- Порог показа тела .65 -> .5 м.

## Чамсы: не просвечивают
- xray-шейдер теперь `Hidden/Internal-Colored` (есть в `unity default resources`, `_ZTest/_ZWrite/_Cull/_SrcBlend/_DstBlend` объявлены как обычные свойства).
  `UI/Default` читает `unity_GUIZTestMode` — это не свойство материала, per-material ZTest не гарантирован. Он остался запасным.
- Прозрачность по умолчанию .28 -> .6, слайдер до 1.0 (старое сохранённое .28 мигрирует в .6).

## FPS
- Скан иерархии ограничен по времени (чамсы 1.2 мс/тик, ТП 0.6 мс/кадр), а не по числу узлов; повторный скан раз в 3–4 с, если рендереры уже найдены.
- Запоминается рабочее написание `GetComponentByName` (раньше 4 поиска типа на каждую кость).
- `visited` — hash set вместо линейного поиска.
- Чтение полей свежих объектов Unity напрямую вместо syscall на каждое поле (`process_vm_readv` / `/proc/self/mem`).
- Кэш GameObject рендерера в ТП, `gettid` кэшируется в thread_local (вызывался на каждого игрока каждый кадр).
- Проверка материалов 0.35 -> 0.75 с (`GetSharedMaterialArray` аллоцирует массив каждый раз).

## Логи (`logcat -s MANGOcheat`)
- `TP travel=... body_view=... renderers=... [id Lслой fp|body en off sh]` раз в 5 с: сразу видно, почему тела нет.
- `Cost: third_person avg/max us, chams avg/max us` раз в ~15 с: сколько мс на главном потоке съедает каждая фича.

## r12.1: лог в файл
- Все строки лога пишутся ещё и в `/data/user/0/com.catsbit.oxidesurvivalisland/files/mango_log.txt`
  (и в `/sdcard/Android/data/<pkg>/files/mango_log.txt`). `start.sh` через 6 с копирует его в `/sdcard/Download/mango_log.txt`.
- Строки диагностики каждые 2 с: `Diag:` (local/camera/players), `Chams:` (кандидаты, найденные рендереры, причины отказов, шейдер), `TP ...`, `Cost:`.
- `missing icall: ...` — если не нашёлся какой-то Unity-биндинг.
- Вероятная причина «чамсы не работают совсем»: `Shader.FindBuiltin` ищет только в таблице builtin-ресурсов
  (`unity default resources`), а `UI/Default` лежит в основном data-файле -> шейдер не находился, ни один материал не менялся.
  `Hidden/Internal-Colored` лежит именно в `unity default resources`.

## r12b (по логу с устройства)
- Лог показал: `FindBuiltin` не находит ни `Hidden/Internal-Colored`, ни `UI/Default` (`kind=0 id=0`, `status=-2`, `tint=...` отказов), поэтому чамсы не красили ничего.
  Теперь шейдер ищется через `ResourcesAPIInternal.FindShaderByName` (это то, что вызывает `Shader.Find`), `FindBuiltin` — запасной.
  Без шейдера иерархии больше не сканируются, повтор раз в 5 с, строка лога один раз.
- Третье лицо по логу: камера уходит на 5 м, 6 body-рендереров enabled, forceOff=0, слой 0 в маске — но тело не видно.
  Добавлена построчная диагностика рендереров: `visible` (engine culling), bounds, scale, mesh.
  Исправлена проверка `ShadowCastingMode` (ShadowsOnly = 3, не 2).
  Новый переключатель `Solid Body (TP)` (вкладка player): красит локальное тело материалом чамсов — если с ним тело видно, значит игра скрывает его своим шейдером/материалом.
