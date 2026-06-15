# Turret — wieżyczka pan/tilt na Raspberry Pi Pico (AtomVM + Elixir)

Sterowana joystickiem wieżyczka na dwóch serwach **SG90** (pan/tilt) z czujnikiem
odległości **HC-SR04**, działająca na **Raspberry Pi Pico (RP2040)** pod
**AtomVM**. Logika napisana w **Elixirze**.

Dwa tryby pracy:

- **Manualny** — joystick steruje obydwoma serwami.
- **Automatyczny** — wieżyczka wykonuje skan rastrowy przestrzeni; po wykryciu
  obiektu bliżej niż próg (`@detect_cm`, domyślnie 50 cm) zatrzymuje się na celu.

Przycisk joysticka przełącza tryby.

---

## Koncepcja i plan realizacji (one-pager)

**Pomysł na implementację.** Całość to jeden proces AtomVM z rekurencyjną pętlą
`loop/1`, przez którą przewijany jest **niemutowalny stan** (mapa: tryb, kąty
serw, kierunki skanu, poprzedni stan przycisku). Każdy obrót pętli:
odczyt przycisku -> akcja zależna od trybu -> krótki `sleep` -> ponowne wywołanie
pętli. Analog (ADC) i sprzętowy PWM obsługują własne NIF-y w C dodane do AtomVM;
przycisk i HC-SR04 korzystają ze standardowego `:gpio`.

**Dane wejściowe**
- Joystick: osie X/Y z ADC (surowo 0–4095, znormalizowane do −100…100); przycisk z GPIO (pull-up).
- HC-SR04: czas trwania echa (µs) -> przeliczany na centymetry.

**Dane wyjściowe**
- 2× serwo SG90: sygnał PWM (kąt 0–180°).
- Konsola UART (115200): logi trybu i wykrycia celu.

**Plan funkcji (kluczowe):**

| Funkcja | Rola |
|---------|------|
| `Turret.start/0` | inicjalizacja sprzętu, centrowanie serw, start pętli |
| `Turret.loop/1` | pętla główna: przycisk -> tick -> sleep |
| `read_button/1` | wykrycie zbocza, przełączenie trybu |
| `tick/1` | rozdział na tryb manualny / auto (pattern matching) |
| `manual_tick/1` | joystick -> strefa martwa -> delta -> kąty serw |
| `sweep/1` + `step_axis/3` | skan rastrowy „odbijający" (czysta logika ruchu) |
| `detect/1` | pomiar odległości z odpornością na pojedyncze timeouty |
| `ADC.read_normalized/1`, `PWM.set_angle/2`, `HCSR04.measure_cm/0` | warstwa sprzętowa |

**Wysokopoziomowy przebieg:** start -> centrowanie -> **pętla** { odczyt przycisku ->
(manual: joystick -> serwa  ·  auto: detekcja -> cel/skan) -> sleep } ↺

---

## Dlaczego ten projekt jest nietypowy

AtomVM dla portu RP2 **nie miał wbudowanej obsługi ADC ani sprzętowego PWM**.
Żeby odczytać joystick (analogowo) i sterować serwami (PWM), do źródeł AtomVM
zostały dodane **własne sterowniki NIF w C**:

| Plik | Rola |
|------|------|
| `adcdriver.c` / `adcdriver.h` | NIF-y ADC (joystick) |
| `pwmdriver.c` / `pwmdriver.h` | NIF-y sprzętowego PWM (serwa) |

GPIO (przycisk + HC-SR04) korzysta ze **standardowego** modułu `:gpio` AtomVM —
tu nic nie trzeba było dodawać.

Szczegóły integracji ze źródłami AtomVM są w sekcji
[Własne sterowniki AtomVM](#własne-sterowniki-atomvm).

---

## Sprzęt

### Lista elementów

- Raspberry Pi Pico (RP2040)
- 2× serwo SG90
- czujnik ultradźwiękowy HC-SR04
- moduł joysticka analogowego (2 osie + przycisk)
- zasilanie 5 V dla serw (najlepiej osobne źródło)

### Podłączenie

| Komponent | Pin Pico | Funkcja | Uwagi |
|-----------|----------|---------|-------|
| Serwo X (pan) — sygnał | GP2 | PWM | |
| Serwo Y (tilt) — sygnał | GP1 | PWM | |
| Joystick VRx | GP26 | ADC0 | zasilanie joysticka z **3V3** |
| Joystick VRy | GP27 | ADC1 | zasilanie joysticka z **3V3** |
| Joystick SW (przycisk) | GP28 | wejście, pull-up | aktywny stanem niskim |
| HC-SR04 Trig | GP4 | wyjście | |
| HC-SR04 Echo | GP5 | wejście | **5 V -> patrz niżej** |

---

## Struktura projektu

```
turret/
├── mix.exs                 # konfiguracja projektu mix
├── deploy.sh               # kompilacja + pakowanie + wgranie na Pico
├── lib/
│   ├── turret.ex           # logika główna (pętla, tryby, skan, detekcja)
│   ├── pwm.ex              # wrapper PWM -> sterowanie serwami (kąt)
│   ├── adc.ex              # wrapper ADC -> odczyt joysticka (znormalizowany)
│   └── hcsr04.ex           # pomiar odległości (na standardowym :gpio)
└── atomvm-drivers/         # kopie sterowników wgranych do źródeł AtomVM
    ├── adcdriver.c
    ├── adcdriver.h
    ├── pwmdriver.c
    └── pwmdriver.h
```

---

## Moduły Elixir

| Moduł | Opis |
|-------|------|
| `Turret` | Pętla główna, obsługa przycisku, tryb manualny i automatyczny (skan rastrowy + detekcja). |
| `PWM` | Zamienia kąt 0–180° na wypełnienie PWM i ustawia je na pinie. `set_angle/2`, `center/1`, `init/2`, `deinit/1`. |
| `ADC` | Odczyt surowy oraz znormalizowany joysticka do zakresu −100…100 względem środka. |
| `HCSR04` | Wyzwala Trig, mierzy czas echa i przelicza na centymetry. Zwraca `{:ok, cm}` lub `{:error, :timeout}`. |

### Kalibracja serw (PWM)

W `pwm.ex` wypełnienie jest liczone na 16-bit (`PWM_WRAP = 65535`, 50 Hz):

```
@duty_min 1650   # ~0,5 ms  -> 0°
@duty_mid 4900   # ~1,5 ms  -> 90°
@duty_max 8150   # ~2,5 ms  -> 180°
```

Jeśli serwo nie dochodzi do krańców albo „buczy" na końcach, dostrój `@duty_min`
/ `@duty_max` pod swój egzemplarz SG90.

---

## Własne sterowniki AtomVM

To jest część, która wymaga przebudowania samego AtomVM dla RP2.

### Co udostępniają sterowniki

**ADC** (`adcdriver.c`) — działa tylko na pinach **26–28** (kanały ADC0–2):

| NIF | Sygnatura | Zwraca |
|-----|-----------|--------|
| init | `adc:init/1` | `:ok` |
| read | `adc:read/1` | `{:ok, raw}` (0–4095) |
| read_temp | `adc:read_temp/0` | `{:ok, raw}` z wbudowanego czujnika temperatury (kanał 4) |

**PWM** (`pwmdriver.c`) — dowolny GPIO, rozdzielczość 16-bit, zegar 125 MHz:

| NIF | Sygnatura | Zwraca |
|-----|-----------|--------|
| init | `pwm:init/2` (pin, freq_hz) | `:ok` |
| set_duty | `pwm:set_duty/2` (pin, 0–65535) | `:ok` |
| deinit | `pwm:deinit/1` (pin) | `:ok` |

Każdy NIF jest rozwiązywany zarówno pod nazwą erlangową (`adc:init/1`), jak i
elixirową (`Elixir.ADC:init/1`) — dlatego z Elixira wołasz je przez moduły
`ADC` / `PWM`, a one trafiają wprost do sterownika w C.

### Jak wgrać sterowniki do AtomVM

1. Skopiuj cztery pliki do katalogu sterowników portu RP2, np.:
   `src/platforms/rp2/src/lib/`.
2. Dopisz źródła `adcdriver.c` i `pwmdriver.c` do listy plików kompilowanych
   w `CMakeLists.txt` tego katalogu (tam, gdzie listowane są pozostałe `*.c`
   portu).
3. Rejestracja NIF-ów jest **automatyczna** — makro
   `REGISTER_NIF_COLLECTION(...)` na końcu każdego sterownika dokłada kolekcję
   do loadera, więc nie trzeba ręcznie edytować głównego rejestru NIF-ów.
   (W zależności od konfiguracji linkera upewnij się tylko, że sekcja z
   rejestracją nie zostaje wycięta przy optymalizacji.)
4. Przebuduj AtomVM dla rp2 zgodnie z dokumentacją AtomVM i odbierz nowy
   `AtomVM.uf2` — to **ten** plik (z wkompilowanymi sterownikami) wgrywasz na
   Pico, nie oficjalny build.

> Bez przebudowanego `AtomVM.uf2` projekt rzuci błędem o nierozwiązanym NIF-ie
> (`adc:read/1` / `pwm:set_duty/2`) zaraz po starcie.

---

## Budowanie i wgrywanie

### Wymagania

- Elixir / `mix`
- narzędzia AtomVM: `packbeam`, `uf2tool`
- `picotool` (wgrywanie i restart Pico)
- `screen` (podgląd konsoli)
- przebudowany **`AtomVM.uf2`** (z własnymi sterownikami) oraz biblioteka
  standardowa **`atomvmlib-rp2-pico.uf2`**

### Deploy

Skrypt `deploy.sh` robi całość:

```bash
./deploy.sh
```

Co po kolei się dzieje:

1. `mix compile` — kompilacja modułów Elixir do `.beam`.
2. `packbeam` — spakowanie `.beam` w `turret.avm`.
3. `uf2tool` — zapakowanie `turret.avm` w `turret.uf2` pod offsetem aplikacji
   `0x10180000`.
4. `picotool` — restart do trybu BOOTSEL i wgranie po kolei:
   `AtomVM.uf2` -> `atomvmlib-rp2-pico.uf2` -> `turret.uf2`, potem reset.
5. `screen /dev/ttyACM0 115200` — podgląd logów (`IO.puts` z wieżyczki).

> Pełny zestaw trzech `.uf2` wgrywasz raz; przy kolejnych zmianach samej logiki
> Elixira zwykle wystarcza ponowne wgranie tylko `turret.uf2`.

---

## Obsługa

- **Start** — po wgraniu serwa centrują się (90°/90°) i wieżyczka rusza w trybie
  manualnym.
- **Przycisk joysticka** — przełącza manualny ↔ automatyczny (wykrywane zbocze
  opadające, wejście z pull-up).
- **Tryb manualny** — wychylenie joysticka rusza serwami; mała strefa martwa
  (`@manual.dead_zone`) eliminuje dryf w spoczynku, `@manual.speed` ustawia
  szybkość.
- **Tryb automatyczny** — skan rastrowy „odbijający": tilt (Y) jedzie tam i z
  powrotem, a przy każdym odbiciu pan (X) robi krok i również odbija się od
  krańców. Po wykryciu obiektu `< @detect_cm` wieżyczka zatrzymuje się i trzyma
  pozycję; gdy obiekt zniknie, skan płynnie wznawia się z bieżącego położenia.

---

## Konfiguracja (stałe w `turret.ex`)

| Stała | Znaczenie |
|-------|-----------|
| `@pins` | przypisanie pinów (serwa, osie joysticka, przycisk) |
| `@sweep.step_x` / `@sweep.step_y` | krok skanu w stopniach (pan / tilt) |
| `@manual.dead_zone` | strefa martwa joysticka |
| `@manual.speed` | szybkość ruchu w trybie manualnym |
| `@detect_cm` | próg wykrycia celu (cm) |
| `@samples` | ile pomiarów na tick zanim uznamy „brak celu" (odporność na pojedyncze timeouty) |
| `@loop_ms` | okres pętli głównej |

Piny Trig/Echo HC-SR04 oraz timeout pomiaru są w `hcsr04.ex` (`@trig`, `@echo`,
`@timeout_us`).
