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

## r12c (по логу третьего лица)
- Лог: у рендереров тела `isVisible=0` ровно у тех, у кого включён dynamic occlusion (`m_DynamicOccludee=1`), а у двух без него (`sh0`, occludee=0) периодически `visible=1`.
  Значит тело отсекает запечённый occlusion culling при сдвинутой камере (поэтому и Solid Body не помогал: материал не при чём).
  ТП теперь выключает `allowOcclusionWhenDynamic` у рендереров тела и возвращает при выключении (как чамсы).

## r12d
- Если после выключения occlusion у рендереров тела движок всё равно их не рисует (`isVisible=0` 1.5 с подряд),
  третье лицо выключает `Camera.useOcclusionCulling` на время работы и возвращает обратно. В логе: `cam_occlusion_off=1 drawn=N`.

## r12e
- Лог r12d: рендереры тела `enabled=1 forceOff=0 occlusion=0`, камера с выключенным occlusion, mesh есть, bounds на месте — и всё равно `isVisible=0`, `drawn=0`.
  Occlusion не причина (запасной выключатель камеры отключён константой `kCameraOcclusionFallback`).
- Наиболее вероятно, игра перепрятывает тело между нашим вызовом и культингом камеры (pre-cull / beginCameraRendering).
  Добавлены хуки `Renderer.enabled/forceRenderingOff/shadowCastingMode/SetMaterialArray`, `GameObject.SetActive/layer`, `Camera.cullingMask`:
  пока включено ТП, они пишут в лог `Guard: <вызов>(<значение>) BLOCKED/allowed callers=+0x…` (смещения в libil2cpp = игровой метод) и
  блокируют скрытие (enabled=false, forceRenderingOff=true, SetActive(false), смена слоя) для рендереров/GameObject тела. Свои вызовы проходят без изменений.

## r12f
- Краш при включении ТП (лог r12e: хуки встали, один TP-отчёт, дальше тишина). Причина: icall-входы в `libunity` — крошечные заглушки
  (`tst w1,#0xff; cset w1,ne; b impl` или одно `b impl`, 4–12 байт), а Dobby пишет 16 байт и затирал соседние icall'ы.
  Теперь хук ставится на реальную реализацию (за заглушкой) и только если там есть 4 обычные инструкции; иначе `Guard: skipped …`.
- Обработчик краша: `CRASH signal=… fault=… pc=… (il2cpp+0x…)` и бэктрейс по frame pointer пишутся в лог до передачи управления игре.
- Предыдущий лог сохраняется как `mango_log.txt.prev` (раньше новый запуск затирал лог с крашем).

## r12g
- Лог r12f: хуки встали без краша, ни одного `Guard:`-события от игры (enabled/forceOff/shadow не трогаются), тело всё равно `visible=0`.
  Значит игра прячет его не флагами рендерера. Новая гипотеза: игра задаёт `Camera.cullingMatrix` (floating origin / first-person), и движок режет по
  фрустуму, который не следует за сдвинутой камерой (тело вокруг головы оказывается вне фрустума).
- ТП каждый кадр после сдвига вызывает `Camera.ResetCullingMatrix`. В лог TP-строки добавлено `cull_matrix_diff` — расхождение культ-матрицы с P*V
  сдвинутой камеры до сброса (≈0 — гипотеза неверна, >0.01 — подтверждена).

## r12h (по дампу https://github.com/xvcen/1)
- В `dump.cs`: `Oxide.Features.Rendering.FirstPersonRenderController` + `FirstPersonRenderFeature` (URP ScriptableRendererFeature) + `FirstPersonRenderSettings`
  с тремя `RenderingLayerMask` (`maskedRenderingLayerMask` @0x1C, `shiftedRenderingLayerMask` @0x20, `worldSpaceRenderingLayerMask` @0x24), своим FOV, near/far и depth clear.
  Контроллер держит списки `Renderer`/`SkinnedMeshRenderer` и статическими `kqI/ueZ/crA/ZDZ/cHH(Renderer, uint, uint)` переставляет им rendering layer:
  такие рендереры рисует только first-person проход, обычные проходы их пропускают. Это объясняет тело, которое «включено, но не рисуется».
- ТП теперь читает `Renderer.renderingLayerMask` рендереров тела и ставит default-слой (бит 0), возвращая исходное при выключении.
  В лог: `rlm=<сейчас>(was <исходный>)` и `Guard: Renderer.renderingLayerMask(...)` при вызовах игры.

## r12i (диагностика)
- Лог r12h: `rlm=1(was 1)` (слой уже дефолтный), `cull_matrix_diff=0` — ни rendering layer, ни culling matrix не объясняют `visible=0`. Тело enabled, на месте, не occluded, слой 0 в маске.
- Теперь пишем: `TP camera near/far/fov`, `TP clip x y z w` — куда центр bounds тела попадает в frustum по culling-матрице (внутри: |x|,|y|<=w, 0<=z<=w),
  `Chams: ... drawn_by_engine=N` — сколько рендереров чужих игроков движок реально рисует (контроль для `isVisible`),
  и хуки `Camera.cullingMatrix=` / `Camera.ResetCullingMatrix` (вызовы игры при включённом ТП).

## r12j
- Лог r12i: контроль `drawn_by_engine=8` (чужие рендереры движок рисует — `isVisible` рабочий), центр тела внутри frustum (`TP clip x=-0.17 y=-1.56 w=3.54`),
  near=0.15 far=1000 fov=62, игра `Camera.cullingMatrix` не трогает. Движок режет именно тело.
- Хуки icall для `GameObject.SetActive` / `set_layer` / `Camera.set_cullingMask` пропускались (заглушки по 4 байта). Теперь хукаются IL2CPP-обёртки по RVA из дампа
  (`GameObject.SetActive` 0xD4BB8CC, `set_layer` 0xD4BB690, `Camera.set_cullingMask` 0xD4351C0): `Guard: GameObject.SetActive(0) BLOCKED callers=+0x…`,
  где `callers` уже игровой метод, который скрывает тело (его RVA ищется в `dump.cs`).
