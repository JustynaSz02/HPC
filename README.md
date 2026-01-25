# 🧠 2D Bin Packing with Tabu Search (C++)

Projekt został przepisany z Pythona na C++. Implementuje heurystyczne rozwiązanie problemu 2D Bin Packing z użyciem metody Tabu Search:

- Nieprzesłaniające się prostokątne paczki
- Rotacja 90°
- Minimalizacja liczby użytych binów

Wynik jest wypisywany w konsoli oraz eksportowany do plików SVG w folderze `out_svg/`.

---

## 📂 Struktura

```bash
project/
├── CMakeLists.txt
├── src/
│   ├── main.cpp
│   ├── Package.h
│   ├── Package.cpp
│   ├── Bin.h
│   ├── Bin.cpp
│   ├── TabuSearch.h
│   ├── TabuSearch.cpp
│   ├── Visualize.h
│   └── Visualize.cpp
├── BinPackingData/
│   ├── M1a.txt
│   ├── M1b.txt
│   ├── M1c.txt
│   ├── M1d.txt
│   └── M1e.txt
├── build/          # folder z skompilowanym programem
├── out_svg/        # wygenerowane obrazki SVG
└── README.md
```

---

## 🚀 Jak uruchomić program

### Wymagania:
- **CMake 3.16+** (zainstaluj z [cmake.org](https://cmake.org/download/) lub przez `winget install Kitware.CMake`)
- **Kompilator C++** (MSVC - wbudowany w Visual Studio, lub MinGW-w64)

### Krok 1: Otwórz PowerShell
Otwórz PowerShell w folderze projektu.

### Krok 2: Zbuduj projekt (pierwszy raz lub po zmianach w kodzie)
```powershell
cmake -S . -B build
cmake --build build --config Release
```

### Krok 3: Uruchom program
```powershell
.\build\Release\bin_packing.exe
```

### Krok 4: Otwórz folder z obrazkami
```powershell
Start .\out_svg
```

**Uruchomienie z innym plikiem danych:**
```powershell
.\build\Release\bin_packing.exe "BinPackingData/M1b.txt"
```

**Szybkie uruchomienie (jeśli już masz zbudowany program):**
```powershell
.\build\Release\bin_packing.exe
Start .\out_svg
```

---

## 📈 Wyjście

Program wypisuje w konsoli:
- Rozmieszczenie paczek w każdym binie (pozycje X, Y, wymiary, rotacja)
- Liczbę użytych binów: `Best solution found: <liczba>`
- Informację o zapisanych SVG: `SVGs zapisane w folderze: out_svg`

**Obrazki SVG:**
- Każdy użyty bin ma swój plik `bin_<ID>.svg` w folderze `out_svg/`
- Obrazki zawierają: siatkę, osie z etykietami, paczki z numerami ID

---

## 📜 Licencja

MIT / educational use.
