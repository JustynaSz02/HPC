# 🔬 Eksperymenty i badania

Ten folder zawiera narzędzia do:
- **Automatycznego uruchamiania eksperymentów** z różnymi konfiguracjami
- **Generowania instancji** problemu Bin Packing
- **Tworzenia wykresów** z wyników eksperymentów
- **Analizy wydajności** programu

---

## 🚀 Szybki start - Automatyczny eksperyment

### **Główny skrypt: `run_speedup_experiment.py`**

Automatycznie uruchamia eksperymenty i tworzy wykres **Speedup vs Rozmiar problemu** dla różnych liczb wątków.

**Użycie:**
```powershell
cd experiments
python run_speedup_experiment.py
```

**Co robi:**
1. ✅ Generuje różne rozmiary instancji (small, medium, large, xlarge, xxlarge, xxxlarge, huge)
2. ✅ Uruchamia program z różnymi liczbami wątków (1, 2, 4, 6, 8)
3. ✅ Zbiera wyniki z każdego uruchomienia
4. ✅ Tworzy wykres speedup vs rozmiar problemu
5. ✅ Zapisuje wyniki do JSON

**Wynik:**
- 📊 Wykres: `../experiment_results/speedup_vs_problem_size.png`
- 📄 Wyniki: `../experiment_results/results.json`
- 📁 Pliki output: `../experiment_results/output_*.txt`

---

## ⚙️ Konfiguracja eksperymentu

Możesz zmodyfikować konfigurację w pliku `run_speedup_experiment.py`:

```python
CONFIG = {
    'threads': [1, 2, 4, 6, 8],  # Liczby wątków do przetestowania
    'problem_sizes': [  # Rozmiary problemów
        ('small', 5, 50),        # 5 typów binów, 50 paczek
        ('medium', 8, 100),      # 8 typów binów, 100 paczek
        ('large', 10, 200),      # 10 typów binów, 200 paczek
        ('xlarge', 15, 500),     # 15 typów binów, 500 paczek
        ('xxlarge', 20, 1000),   # 20 typów binów, 1000 paczek
        ('xxxlarge', 25, 2000),  # 25 typów binów, 2000 paczek
        ('huge', 30, 5000),      # 30 typów binów, 5000 paczek
    ],
    'iterations': 50,  # Liczba iteracji Tabu Search
    'runs_per_config': 1,  # Liczba uruchomień dla każdej konfiguracji
    'program_path': '../build/Release/bin_packing.exe',
}
```

---

## 📋 Inne skrypty

### **1. generate_instance.py** - Generator instancji

Generuje nowe instancje problemu Bin Packing.

**Użycie:**
```powershell
cd experiments
python generate_instance.py --custom 10 200 --output ../BinPackingData/custom.txt --seed 42
```

---

### **2. plot_time_vs_threads.py** - Wykres czasu vs wątki

Tworzy wykres pokazujący czas wykonania w zależności od liczby wątków.

**Użycie:**
```powershell
cd experiments
python plot_time_vs_threads.py ../output_1.txt:1 ../output_2.txt:2 ../output_4.txt:4
```

---

### **3. plot_speedup_and_eval.py** - Wykresy speedup i efektywności

Tworzy dwa wykresy: speedup i efektywność vs liczba wątków.

**Użycie:**
```powershell
cd experiments
python plot_speedup_and_eval.py ../output_1.txt:1 ../output_2.txt:2 ../output_4.txt:4
```

---

## 📊 Format wyników JSON

Wyniki są zapisywane w formacie JSON:

```json
{
  "config": {
    "threads": [1, 2, 4, 6, 8],
    "problem_sizes": [...],
    "iterations": 50
  },
  "experiments": [
    {
      "problem_name": "small",
      "problem_size": 50,
      "n_threads": 1,
      "total_time_s": 2.5,
      "eval_time_ms": 2000,
      "total_evaluations": 1500,
      "best_solution": 8
    },
    ...
  ]
}
```

---

## 🔧 Wymagania

- Python 3.6+
- matplotlib
- Skompilowany program: `build/Release/bin_packing.exe`

**Instalacja zależności:**
```powershell
pip install matplotlib
```

---

## 📁 Struktura folderów

```
HPC-threads/
├── experiments/              ← Ten folder
│   ├── run_speedup_experiment.py  ← Główny skrypt
│   ├── generate_instance.py
│   ├── plot_*.py
│   └── README.md
├── experiment_results/       ← Wyniki eksperymentów (tworzone automatycznie)
│   ├── results.json
│   ├── speedup_vs_problem_size.png
│   └── output_*.txt
├── BinPackingData/           ← Instancje problemu
└── build/Release/
    └── bin_packing.exe       ← Program
```

---

*Folder utworzony: 2024*
*Automatyzacja eksperymentów z różnymi konfiguracjami*

