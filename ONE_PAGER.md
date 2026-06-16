# Wieżyczka pan/tilt z trybem automatycznym

**Wstępny plan realizacji projektu**

## Pomysł na implementację

- Sterowanie wieżyczką pan/tilt (dwa serwa SG90) na Raspberry Pi Pico pod AtomVM; logika zaimplementowana w języku Elixir
- Dwa tryby pracy: manualny (sterowanie joystickiem) oraz automatyczny (skan przestrzeni i wykrycie celu czujnikiem odległości HC-SR04)
- Stan systemu reprezentowany funkcyjnie - niemutowalna mapa przewijana przez rekurencyjną pętlę procesu; tryby przełączane przyciskiem
- Dostęp do sprzętu (ADC, PWM) przez własne sterowniki NIF w języku C, dodane do AtomVM dla RP2040

## Dane wejściowe

- Pozycje osi joysticka (ADC) i stan przycisku
- Czas trwania echa czujnika HC-SR04 (odległość)
- Aktualny tryb i stan systemu

## Dane wyjściowe

- Sygnały PWM sterujące kątem dwóch serw (0-180 stopni)
- Zaktualizowany stan (pozycja serw, tryb, kierunek skanu)
- Informacja o wykryciu celu (odległość, pozycja) na konsoli

## Plan implementacji

- Sterowniki ADC i PWM (NIF w C) w AtomVM
- Wrappery w Elixir: PWM, ADC, HC-SR04
- Pętla główna i niemutowalny stan systemu
- Tryb manualny: joystick steruje kątem serw
- Wykrywanie zbocza przycisku i przełączanie trybów
- Tryb automatyczny: skan rastrowy
- Detekcja celu odporna na timeouty czujnika
- Zatrzymanie na celu i płynne wznowienie skanu

## Zakres podstawowy

- Pełne sterowanie manualne joystickiem
- Automatyczny skan i wykrycie obiektu poniżej progu odległości
- Własne sterowniki ADC/PWM dla RP2040 w AtomVM
- Płynny, deterministyczny start trybu automatycznego

## Ryzyka implementacyjne

- Odczyt HC-SR04 (timeouty, szum)
- Zasilanie i kalibracja serw
- Synchronizacja stanu skanu (brak skoków serw przy wejściu w tryb auto)
- Złożoność implementacji sterowników w AtomVM
