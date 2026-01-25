# 🚀 Jak uruchomić program - Przewodnik

## 📋 Szybki start

### 1. **Kompilacja (pierwszy raz lub po zmianach)**
```powershell
cmake -S . -B build
cmake --build build --config Release
```

### 2. **Uruchomienie z domyślnym plikiem**
```powershell
.\build\Release\bin_packing.exe
```

### 3. **Uruchomienie z konkretnym plikiem danych**
```powershell
.\build\Release\bin_packing.exe "BinPackingData\M1a.txt"
.\build\Release\bin_packing.exe "BinPackingData\M1b.txt"
.\build\Release\bin_packing.exe "BinPackingData\M1c.txt"
```

### 4. **Otwórz wyniki (SVG)**
```powershell
Start .\out_svg
```

---

## 📊 Dostępne pliki testowe

W folderze `BinPackingData/`:
- `M1a.txt` - mała instancja
- `M1b.txt` - średnia instancja
- `M1c.txt` - większa instancja
- `M1d.txt` - duża instancja
- `M1e.txt` - bardzo duża instancja

---

## 🔍 Jak program działa - Architektura

### **1. Model wyspowy (Island Model)**

Program używa **modelu wyspowego** z równoległymi wątkami:

```
┌─────────────────────────────────────────┐
│  MAIN THREAD (Master)                  │
│  - Generuje rozwiązania początkowe     │
│  - Koordynuje wątki                     │
│  - Zbiera najlepsze rozwiązania        │
└─────────────────────────────────────────┘
           │
           ├─── WĄTEK 1 (Island 1)
           │    └─── Tabu Search (iteracje)
           │
           ├─── WĄTEK 2 (Island 2)
           │    └─── Tabu Search (iteracje)
           │
           ├─── WĄTEK 3 (Island 3)
           │    └─── Tabu Search (iteracje)
           │
           ├─── WĄTEK 4 (Island 4)
           │    └─── Tabu Search (iteracje)
           │
           ├─── WĄTEK 5 (Island 5)
           │    └─── Tabu Search (iteracje)
           │
           └─── WĄTEK 6 (Island 6)
                └─── Tabu Search (iteracje)
```

**Parametry:**
- `n_threads = 6` - liczba równoległych wątków (wysp)
- `iterations = 50` - liczba iteracji Tabu Search w każdym wątku
- `loops = 5` - liczba iteracji głównej pętli (ulepszanie wspólnego rozwiązania)

---

### **2. Przepływ działania**

#### **FAZA 1: Inicjalizacja**
```
1. Wczytaj dane z pliku (biny i paczki)
2. Wygeneruj 6 różnych rozwiązań początkowych (różne seedy)
3. Włącz logowanie wydajności GPU/CPU
```

#### **FAZA 2: Pierwsza iteracja (równoległa)**
```
Dla każdego wątku (1-6):
  └─── Uruchom Tabu Search z własnym rozwiązaniem początkowym
       └─── Dla każdej iteracji Tabu Search:
            ├─── Generuj ruchy (getMoves)
            ├─── Filtruj przez tabu listę
            ├─── Oceń ruchy (GPU lub CPU)
            ├─── Wybierz najlepszy ruch
            ├─── Zastosuj ruch do rozwiązania
            └─── Aktualizuj tabu listę

Zbierz najlepsze rozwiązanie ze wszystkich wątków
```

#### **FAZA 3: Iteracyjne ulepszanie (loops)**
```
Dla każdej iteracji głównej pętli (1-5):
  └─── Wszystkie wątki startują z najlepszego rozwiązania
       └─── Uruchom Tabu Search równolegle
            └─── Zbierz najlepsze rozwiązanie
```

#### **FAZA 4: Wyniki**
```
1. Wyświetl statystyki wydajności
2. Wyświetl rozmieszczenie paczek
3. Eksportuj SVG do folderu out_svg/
```

---

### **3. Tabu Search - szczegóły**

Każdy wątek wykonuje **Tabu Search** z następującymi krokami:

```
┌─────────────────────────────────────────┐
│  Tabu Search (w każdym wątku)          │
├─────────────────────────────────────────┤
│                                         │
│  FOR iteracja = 1 TO 50:                │
│    │                                    │
│    ├─ 1. Generuj ruchy (getMoves)       │
│    │     └─ Dla każdej paczki:          │
│    │        └─ Dla każdego bina:        │
│    │           └─ Stwórz Move           │
│    │                                    │
│    ├─ 2. Filtruj przez tabu listę      │
│    │     └─ Usuń ruchy które są w tabu │
│    │                                    │
│    ├─ 3. Oceń ruchy (GPU/CPU)          │
│    │     ├─ Konwertuj do GPU view       │
│    │     ├─ evaluateMoves()             │
│    │     │   ├─ Sprawdź CUDA           │
│    │     │   ├─ Jeśli GPU:              │
│    │     │   │   └─ CUDA kernels        │
│    │     │   └─ Jeśli CPU:              │
│    │     │       └─ CPU fallback        │
│    │     └─ Zwróć valid[] i scores[]   │
│    │                                    │
│    ├─ 4. Wybierz najlepszy ruch        │
│    │     └─ Min score, max tie-break    │
│    │                                    │
│    ├─ 5. Zastosuj ruch                 │
│    │     ├─ Usuń paczkę z bina źródłowego
│    │     └─ Umieść paczkę w binie docelowym
│    │                                    │
│    └─ 6. Aktualizuj tabu listę          │
│         └─ Dodaj ruch do tabu           │
│                                         │
└─────────────────────────────────────────┘
```

---

### **4. GPU Acceleration - jak działa**

Gdy CUDA jest dostępne:

```
┌─────────────────────────────────────────┐
│  evaluateMoves() - GPU Path            │
├─────────────────────────────────────────┤
│                                         │
│  1. Lock mutex (synchronizacja wątków)  │
│                                         │
│  2. Konwertuj dane do GPU view:         │
│     ├─ GpuSolutionView (płaskie tablice)│
│     └─ GpuMovesView (tablice ruchów)   │
│                                         │
│  3. Asynchroniczne transfery (streams): │
│     ├─ Stream 1: moves (równoległy)    │
│     └─ Stream 2: solution (równoległy) │
│                                         │
│  4. CUDA Kernels:                       │
│     ├─ checkMoveValidityKernel()        │
│     │   └─ Sprawdź kolizje dla każdego move
│     └─ computeMoveScoreKernel()         │
│         └─ Oblicz score dla każdego move
│                                         │
│  5. Asynchroniczne kopiowanie wyników:  │
│     └─ Stream 3: valid[] i scores[]    │
│                                         │
│  6. Unlock mutex                        │
│                                         │
└─────────────────────────────────────────┘
```

**Optymalizacje:**
- ✅ **Pinned Memory** - szybsze transfery
- ✅ **Buforowanie pamięci GPU** - nie alokujemy za każdym razem
- ✅ **Asynchroniczne transfery** - równoległość

---

## 📈 Przykładowe wyjście

```
28                                    # Liczba rdzeni CPU
Best: 10, 18.0014                    # Początkowe rozwiązanie
Best at 0: 9, 16.8636                # Najlepsze z wątku 0
Best at 1: 8, 14.7602                # Najlepsze z wątku 1
Best at 2: 8, 14.7993                # Najlepsze z wątku 2
Time elapsed = 2[s]                  # Czas pierwszej iteracji

Performance stats - evaluateMoves(): 
  total calls=300, total time=2ms, avg time=0.0067ms/call, mode=GPU

=== Performance Statistics ===
Performance stats - evaluateMoves(): 
  total calls=1500, total time=0ms, avg time=0ms/call, mode=GPU

Rozmieszczenie paczek:
Bin ID 0:
  Package ID 1: X=0, Y=0, W=10, H=5, Rotated=False
  Package ID 3: X=10, Y=0, W=8, H=6, Rotated=True
  ...
Best solution found: 8              # Liczba użytych binów
SVGs zapisane w folderze: out_svg
```

---

## 🎯 Parametry programu

Możesz zmienić parametry w `src/main.cpp`:

```cpp
int n_threads = 6;      // Liczba równoległych wątków
int iterations = 50;    // Liczba iteracji Tabu Search
int loops = 5;          // Liczba iteracji głównej pętli
```

---

## 🔧 Troubleshooting

### Problem: "Cannot open file"
**Rozwiązanie:** Sprawdź czy plik istnieje i podaj pełną ścieżkę:
```powershell
.\build\Release\bin_packing.exe "BinPackingData\M1a.txt"
```

### Problem: Program używa CPU zamiast GPU
**Sprawdź:**
1. Czy CUDA Toolkit jest zainstalowany
2. Czy karta graficzna NVIDIA jest dostępna
3. Sprawdź logi - powinno być `mode=GPU`

### Problem: Brak plików SVG
**Sprawdź:** Czy folder `out_svg/` istnieje (program go utworzy automatycznie)

---

## 📚 Więcej informacji

- `JAK_DZIALA_PROGRAM.md` - **prosty przewodnik jak program działa** ⭐
- `README.md` - podstawowe informacje
- `DOKUMENTACJA_ZMIAN.md` - szczegółowa dokumentacja zmian
- `OPTYMALIZACJE_GPU.md` - optymalizacje GPU
- `CO_ZOSTALO_DO_ZROBIENIA.md` - status projektu

---

*Ostatnia aktualizacja: $(Get-Date -Format "yyyy-MM-dd")*
