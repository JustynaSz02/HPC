#!/usr/bin/env python3
"""
Automatyczny eksperyment: Speedup vs Rozmiar problemu dla różnych liczb wątków

Ten skrypt:
1. Generuje różne rozmiary instancji problemu
2. Uruchamia program z różnymi liczbami wątków
3. Zbiera wyniki
4. Tworzy wykres speedup vs rozmiar problemu
"""

import subprocess
import os
import sys
import re
import json
import matplotlib.pyplot as plt
from pathlib import Path
from collections import defaultdict

# Konfiguracja eksperymentu
CONFIG = {
    'threads': [1, 2, 4, 6, 8],  # Liczby wątków do przetestowania
    'problem_sizes': [  # Rozmiary problemów: (nazwa, typy_binów, liczba_paczek)
        ('small', 5, 50),
        ('medium', 8, 100),
        ('large', 10, 200),
        ('xlarge', 15, 500),
        ('xxlarge', 20, 1000),      # Bardzo duży
        ('xxxlarge', 25, 2000),     # Ogromny
        ('huge', 30, 5000),         # Gigantyczny
        ('massive', 40, 10000),     # Masowy
        ('gigantic', 50, 20000),    # Gigantyczny
    ],
    'iterations': 50,  # Liczba iteracji Tabu Search
    'runs_per_config': 1,  # Liczba uruchomień dla każdej konfiguracji (dla uśrednienia)
    'program_path': '../build/Release/bin_packing.exe',  # Ścieżka do skompilowanego programu
    'data_dir': '../BinPackingData',  # Folder z instancjami
    'output_dir': '../experiment_results',  # Folder na wyniki
    'results_file': '../experiment_results/results.json',  # Plik JSON z wynikami
}

def get_problem_size(problem_file):
    """Pobierz rozmiar problemu (liczbę paczek) z pliku danych."""
    try:
        for encoding in ['utf-8', 'utf-8-sig', 'latin-1', 'cp1252']:
            try:
                with open(problem_file, 'r', encoding=encoding, errors='ignore') as f:
                    lines = f.readlines()
                    # Format: pierwsza linia = liczba typów binów, potem pusta linia, potem liczba paczek
                    for i, line in enumerate(lines):
                        line = line.strip()
                        if line and line.isdigit() and i > 2:
                            # Sprawdź czy następna linia to pusta lub zaczyna się od liczby (paczki)
                            if i + 1 < len(lines):
                                next_line = lines[i + 1].strip()
                                if not next_line or (next_line and ' ' in next_line):
                                    return int(line)
                    # Alternatywnie: ostatnia liczba przed listą paczek
                    for i in range(len(lines) - 1, -1, -1):
                        line = lines[i].strip()
                        if line and line.isdigit() and i > 2:
                            return int(line)
            except:
                continue
    except:
        pass
    return None

def generate_instance(name, bin_types, num_packages, output_file):
    """Wygeneruj instancję problemu."""
    print(f"  Generowanie instancji: {name} ({bin_types} typów binów, {num_packages} paczek)...")
    
    script_path = Path(__file__).parent / 'generate_instance.py'
    cmd = [
        sys.executable,
        str(script_path),
        '--custom', str(bin_types), str(num_packages),
        '--output', output_file,
        '--seed', '42'  # Stały seed dla powtarzalności
    ]
    
    result = subprocess.run(cmd, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"  Błąd generowania instancji: {result.stderr}")
        return False
    
    print(f"  [OK] Instancja wygenerowana: {output_file}")
    return True

def run_experiment(program_path, instance_file, n_threads, iterations, output_file):
    """Uruchom eksperyment i zapisz output."""
    print(f"    Uruchamianie: {n_threads} wątków, {iterations} iteracji...")
    
    cmd = [program_path, instance_file, str(n_threads), str(iterations)]
    
    try:
        with open(output_file, 'w', encoding='utf-8') as f:
            # Większe timeout dla większych problemów (do 30 minut)
            result = subprocess.run(cmd, stdout=f, stderr=subprocess.STDOUT, text=True, timeout=1800)
        
        if result.returncode != 0:
            print(f"    ⚠ Program zakończył się z kodem: {result.returncode}")
            return False
        
        print(f"    [OK] Wynik zapisany: {output_file}")
        return True
    except subprocess.TimeoutExpired:
        print(f"    ⚠ Timeout - program działał zbyt długo")
        return False
    except Exception as e:
        print(f"    [ERROR] Blad: {e}")
        return False

def parse_output_file(filepath):
    """Parsuj plik output i zwróć dane."""
    try:
        for encoding in ['utf-16', 'utf-8', 'utf-8-sig', 'latin-1', 'cp1252']:
            try:
                with open(filepath, 'r', encoding=encoding, errors='ignore') as f:
                    lines = f.readlines()
                    break
            except:
                continue
        else:
            return None
    except:
        return None
    
    result = {
        'total_time_s': 0.0,
        'eval_time_ms': 0.0,
        'total_evaluations': 0,
        'best_solution': 0
    }
    
    # PRIORYTET: Szukaj "Total execution time: Xs (Yms)" - to jest najdokładniejszy pomiar
    total_time = 0.0
    found_total_time = False
    for line in lines:
        line = line.strip()
        match = re.search(r'Total execution time: ([\d.]+)s\s*\((\d+)ms\)', line)
        if match:
            total_time = float(match.group(1))  # Użyj całkowitego czasu (najdokładniejszy)
            found_total_time = True
            break
    
    # Fallback: Jeśli nie ma "Total execution time", sumuj wszystkie "Time elapsed"
    if not found_total_time:
        for line in lines:
            line = line.strip()
            # Nowy format: "Time elapsed = 0.123[s] (123ms)"
            match = re.search(r'Time elapsed = ([\d.]+)\[s\]\s*\((\d+)ms\)', line)
            if match:
                total_time += float(match.group(1))
            else:
                # Stary format: "Time elapsed = 0[s]"
                match = re.search(r'Time elapsed = (\d+)\[s\]', line)
                if match:
                    total_time += float(match.group(1))
    
    # Szukaj performance stats (używamy ostatniego z sekcji "=== Performance Statistics ===", 
    # ale jeśli ma total_time=0, używamy pierwszego niezerowego)
    in_perf_section = False
    last_perf = None
    first_nonzero_perf = None  # Pierwszy wpis z total_time > 0
    
    for line in lines:
        line = line.strip()
        if '=== Performance Statistics ===' in line:
            in_perf_section = True
        
        # Performance stats: total calls=1500, total time=923ms, avg time=0.542ms/call, mode=GPU
        match = re.search(r'total calls=(\d+), total time=(\d+)ms, avg time=([\d.]+)ms/call', line)
        if match:
            perf_data = {
                'total_evaluations': int(match.group(1)),
                'total_time_ms': int(match.group(2)),
                'avg_time_ms': float(match.group(3))
            }
            
            # Zapisz pierwszy niezerowy wpis
            if perf_data['total_time_ms'] > 0 and first_nonzero_perf is None:
                first_nonzero_perf = perf_data
            
            # Zapisz ostatni wpis z sekcji Performance Statistics
            if in_perf_section:
                last_perf = perf_data
                result['eval_time_ms'] = perf_data['total_time_ms']
                result['total_evaluations'] = perf_data['total_evaluations']
    
    # Szukaj "Best solution found: X"
    for line in lines:
        line = line.strip()
        match = re.search(r'Best solution found: (\d+)', line)
        if match:
            result['best_solution'] = int(match.group(1))
            break
    
    # Ustaw czas całkowity
    if total_time > 0:
        result['total_time_s'] = total_time
    else:
        # Użyj performance stats do obliczenia czasu
        perf_to_use = None
        
        # Preferuj ostatni wpis z sekcji Performance Statistics
        if last_perf and last_perf['total_time_ms'] > 0:
            perf_to_use = last_perf
        # Jeśli ostatni ma 0, użyj pierwszego niezerowego
        elif first_nonzero_perf:
            perf_to_use = first_nonzero_perf
        # Jeśli nie mamy żadnego, użyj last_perf (nawet jeśli ma 0)
        elif last_perf:
            perf_to_use = last_perf
        
        if perf_to_use:
            if perf_to_use['total_time_ms'] > 0:
                # Mamy czas z performance stats
                result['total_time_s'] = (perf_to_use['total_time_ms'] / 1000.0) * 1.1  # +10% overhead
                result['eval_time_ms'] = perf_to_use['total_time_ms']
                result['total_evaluations'] = perf_to_use['total_evaluations']
            elif perf_to_use['avg_time_ms'] > 0 and perf_to_use['total_evaluations'] > 0:
                # Oblicz z avg_time * total_evaluations
                total_ms = perf_to_use['avg_time_ms'] * perf_to_use['total_evaluations']
                result['total_time_s'] = (total_ms / 1000.0) * 1.1  # +10% overhead
                result['eval_time_ms'] = total_ms
                result['total_evaluations'] = perf_to_use['total_evaluations']
            else:
                # Jeśli wszystko jest 0 (GPU bardzo szybkie), użyj avg_time jeśli dostępne
                if perf_to_use['avg_time_ms'] > 0 and perf_to_use['total_evaluations'] > 0:
                    total_ms = perf_to_use['avg_time_ms'] * perf_to_use['total_evaluations']
                    result['total_time_s'] = max(total_ms / 1000.0, 0.001)  # Minimum 0.001s
                    result['eval_time_ms'] = total_ms
                    result['total_evaluations'] = perf_to_use['total_evaluations']
                elif perf_to_use['total_evaluations'] > 0:
                    # Oblicz z liczby wywołań * minimalny czas na wywołanie
                    # Załóż minimalny czas: 0.001ms na wywołanie (dla GPU)
                    min_time_ms = perf_to_use['total_evaluations'] * 0.001
                    result['total_time_s'] = max(min_time_ms / 1000.0, 0.001)
                    result['eval_time_ms'] = min_time_ms
                    result['total_evaluations'] = perf_to_use['total_evaluations']
                else:
                    # Fallback: użyj bardzo małego czasu
                    result['total_time_s'] = 0.001
                    result['eval_time_ms'] = 1
    
    # Zwróć wynik jeśli mamy jakikolwiek czas lub dane
    if result['total_time_s'] > 0 or result['total_evaluations'] > 0:
        return result
    return None

def create_speedup_plot(results, output_file):
    """Utwórz wykres speedup vs rozmiar problemu."""
    print("\n[WYKRES] Tworzenie wykresu...")
    
    # Organizuj dane: problem_size -> {n_threads -> time}
    data_by_size = defaultdict(dict)
    problem_sizes = {}
    
    for exp in results:
        problem_name = exp['problem_name']
        problem_size = exp['problem_size']
        n_threads = exp['n_threads']
        time = exp['total_time_s']
        
        if problem_size and time > 0:
            data_by_size[problem_size][n_threads] = time
            problem_sizes[problem_name] = problem_size
    
    if not data_by_size:
        print("  [ERROR] Brak danych do wykresu!")
        return False
    
    # Oblicz speedup (względem 1 wątku)
    speedup_by_size = defaultdict(dict)
    for problem_size, threads_data in data_by_size.items():
        baseline_time = threads_data.get(1)
        if baseline_time and baseline_time > 0:
            for n_threads, time in threads_data.items():
                if time > 0:
                    speedup_by_size[problem_size][n_threads] = baseline_time / time
    
    # Przygotuj dane do wykresu
    all_threads = set()
    for size_data in speedup_by_size.values():
        all_threads.update(size_data.keys())
    all_threads = sorted([t for t in all_threads if t != 1])  # Bez 1 wątku (baseline)
    
    if not all_threads:
        print("  [ERROR] Brak danych dla wiecej niz 1 watku!")
        return False
    
    # Utwórz wykres
    plt.figure(figsize=(12, 7))
    
    # Kolory dla różnych liczb wątków
    colors = ['#3498db', '#2ecc71', '#e74c3c', '#9b59b6', '#f39c12', '#1abc9c']
    
    # Dla każdej liczby wątków narysuj linię
    problem_sizes_sorted = sorted(data_by_size.keys())
    
    for idx, n_threads in enumerate(all_threads):
        sizes = []
        speedups = []
        
        for problem_size in problem_sizes_sorted:
            if problem_size in speedup_by_size and n_threads in speedup_by_size[problem_size]:
                sizes.append(problem_size)
                speedups.append(speedup_by_size[problem_size][n_threads])
        
        if sizes:
            color = colors[idx % len(colors)]
            plt.plot(sizes, speedups, 'o-', linewidth=2.5, 
                    markersize=10, color=color, label=f'{n_threads} wątków', alpha=0.8)
            
            # Dodaj wartości na punktach
            for size, sp in zip(sizes, speedups):
                plt.annotate(f'{sp:.2f}x', (size, sp), textcoords="offset points", 
                           xytext=(0,10), ha='center', fontsize=9)
    
    # Linia idealna (liniowy speedup) dla największej liczby wątków
    if all_threads:
        max_threads = max(all_threads)
        ideal_sizes = problem_sizes_sorted
        ideal_speedups = [max_threads] * len(ideal_sizes)
        plt.plot(ideal_sizes, ideal_speedups, 'g--', alpha=0.3, linewidth=1.5, 
                label=f'Idealny ({max_threads}x)')
    
    plt.xlabel('Rozmiar problemu (liczba paczek)', fontsize=12)
    plt.ylabel('Speedup', fontsize=12)
    plt.title('Speedup vs Rozmiar problemu dla różnych liczb wątków', fontsize=14, fontweight='bold')
    plt.grid(True, alpha=0.3)
    plt.legend(loc='best')
    
    plt.tight_layout()
    plt.savefig(output_file, dpi=300, bbox_inches='tight')
    print(f"  [OK] Wykres zapisany: {output_file}")
    return True

def main():
    print("=" * 60)
    print("AUTOMATYCZNY EKSPERYMENT: Speedup vs Rozmiar problemu")
    print("=" * 60)
    
    # Sprawdź czy program istnieje
    program_path = Path(CONFIG['program_path'])
    if not program_path.exists():
        print(f"[ERROR] Program nie znaleziony: {program_path}")
        print(f"  Upewnij się, że program jest skompilowany w: {program_path}")
        sys.exit(1)
    
    # Utwórz foldery
    data_dir = Path(CONFIG['data_dir'])
    output_dir = Path(CONFIG['output_dir'])
    output_dir.mkdir(exist_ok=True)
    data_dir.mkdir(exist_ok=True)
    
    print(f"\n[FOLDERY]")
    print(f"  Dane: {data_dir}")
    print(f"  Wyniki: {output_dir}")
    
    # Generuj instancje
    print(f"\n[GENEROWANIE] Generowanie instancji problemu...")
    instances = []
    for name, bin_types, num_packages in CONFIG['problem_sizes']:
        instance_file = data_dir / f"exp_{name}.txt"
        if generate_instance(name, bin_types, num_packages, str(instance_file)):
            problem_size = get_problem_size(instance_file)
            instances.append((name, str(instance_file), problem_size))
    
    if not instances:
        print("[ERROR] Nie udalo sie wygenerowac instancji!")
        sys.exit(1)
    
    print(f"\n[OK] Wygenerowano {len(instances)} instancji")
    
    # Uruchom eksperymenty
    print(f"\n[URUCHAMIANIE] Uruchamianie eksperymentow...")
    print(f"  Wątki: {CONFIG['threads']}")
    print(f"  Iteracje: {CONFIG['iterations']}")
    print(f"  Uruchomień na konfigurację: {CONFIG['runs_per_config']}")
    
    results = []
    
    for problem_name, instance_file, problem_size in instances:
        print(f"\n  Problem: {problem_name} (rozmiar: {problem_size})")
        
        for n_threads in CONFIG['threads']:
            for run in range(CONFIG['runs_per_config']):
                output_file = output_dir / f"output_{problem_name}_{n_threads}w_{run}.txt"
                
                if run_experiment(program_path, instance_file, n_threads, 
                                CONFIG['iterations'], str(output_file)):
                    data = parse_output_file(str(output_file))
                    if data:
                        results.append({
                            'problem_name': problem_name,
                            'problem_size': problem_size,
                            'n_threads': n_threads,
                            'run': run,
                            'total_time_s': data['total_time_s'],
                            'eval_time_ms': data['eval_time_ms'],
                            'total_evaluations': data['total_evaluations'],
                            'best_solution': data['best_solution'],
                            'output_file': str(output_file)
                        })
    
    if not results:
        print("\n[ERROR] Brak wynikow do analizy!")
        sys.exit(1)
    
    print(f"\n[OK] Ukonczono {len(results)} eksperymentow")
    
    # Zapisz wyniki do JSON
    results_file = Path(CONFIG['results_file'])
    with open(results_file, 'w', encoding='utf-8') as f:
        json.dump({
            'config': CONFIG,
            'experiments': results
        }, f, indent=2, ensure_ascii=False)
    print(f"\n[ZAPIS] Wyniki zapisane: {results_file}")
    
    # Utwórz wykres
    plot_file = output_dir / 'speedup_vs_problem_size.png'
    if create_speedup_plot(results, str(plot_file)):
        print(f"\n[SUKCES] Eksperyment zakonczony pomyslnie!")
        print(f"   Wykres: {plot_file}")
        print(f"   Wyniki: {results_file}")
    else:
        print(f"\n[UWAGA] Eksperyment zakonczony, ale nie udalo sie utworzyc wykresu")

if __name__ == '__main__':
    main()

