#!/usr/bin/env python3
"""
Generator większych instancji problemu Bin Packing.
"""

import random
import sys

def generate_instance(num_bin_types, num_packages, output_file, seed=None):
    """
    Generuj instancję problemu Bin Packing.
    
    Args:
        num_bin_types: Liczba różnych typów binów
        num_packages: Liczba paczek
        output_file: Nazwa pliku wyjściowego
        seed: Seed dla generatora losowego (opcjonalne)
    """
    if seed is not None:
        random.seed(seed)
    
    # Generuj typy binów (szerokość, wysokość, liczba dostępnych)
    bin_types = []
    for i in range(num_bin_types):
        w = random.randint(10, 30)
        h = random.randint(10, 30)
        count = random.randint(5, 15)
        bin_types.append((w, h, count))
    
    # Generuj paczki (id, szerokość, wysokość)
    packages = []
    for i in range(1, num_packages + 1):
        w = random.randint(2, 15)
        h = random.randint(2, 15)
        packages.append((i, w, h))
    
    # Zapisz do pliku
    with open(output_file, 'w') as f:
        # Liczba typów binów
        f.write(f"{num_bin_types}\n")
        
        # Typy binów (w h count)
        for w, h, count in bin_types:
            f.write(f"{w} {h} {count}\n")
        
        # Pusta linia
        f.write("\n")
        
        # Liczba paczek
        f.write(f"{num_packages}\n")
        
        # Paczki (id w h)
        for pkg_id, w, h in packages:
            f.write(f"{pkg_id} {w} {h}\n")
    
    print(f"Wygenerowano instancję: {output_file}")
    print(f"  - Typy binów: {num_bin_types}")
    print(f"  - Paczki: {num_packages}")

def main():
    if len(sys.argv) < 2:
        print("Uzycie: python generate_instance.py [opcje]")
        print("\nOpcje:")
        print("  --small      Mała instancja (5 typów binów, 50 paczek)")
        print("  --medium     Średnia instancja (8 typów binów, 100 paczek)")
        print("  --large      Duża instancja (10 typów binów, 200 paczek)")
        print("  --xlarge     Bardzo duża instancja (15 typów binów, 500 paczek)")
        print("  --custom N M Instancja z N typami binów i M paczkami")
        print("  --output X   Nazwa pliku wyjściowego (domyślnie: M2a.txt, M2b.txt, etc.)")
        print("  --seed X     Seed dla generatora losowego")
        print("\nPrzykład:")
        print("  python generate_instance.py --large --output M2a.txt --seed 42")
        sys.exit(1)
    
    # Domyślne rozmiary
    sizes = {
        'small': (5, 50),
        'medium': (8, 100),
        'large': (10, 200),
        'xlarge': (15, 500)
    }
    
    # Parsuj argumenty
    size = None
    custom = None
    output_file = None
    seed = None
    
    i = 1
    while i < len(sys.argv):
        arg = sys.argv[i]
        if arg == '--small':
            size = 'small'
        elif arg == '--medium':
            size = 'medium'
        elif arg == '--large':
            size = 'large'
        elif arg == '--xlarge':
            size = 'xlarge'
        elif arg == '--custom' and i + 2 < len(sys.argv):
            custom = (int(sys.argv[i+1]), int(sys.argv[i+2]))
            i += 2
        elif arg == '--output' and i + 1 < len(sys.argv):
            output_file = sys.argv[i+1]
            i += 1
        elif arg == '--seed' and i + 1 < len(sys.argv):
            seed = int(sys.argv[i+1])
            i += 1
        i += 1
    
    # Ustaw parametry
    if custom:
        num_bin_types, num_packages = custom
    elif size:
        num_bin_types, num_packages = sizes[size]
    else:
        # Domyślnie średnia
        num_bin_types, num_packages = sizes['medium']
    
    if output_file is None:
        if size == 'small':
            output_file = '../BinPackingData/M2a.txt'
        elif size == 'medium':
            output_file = '../BinPackingData/M2b.txt'
        elif size == 'large':
            output_file = '../BinPackingData/M2c.txt'
        elif size == 'xlarge':
            output_file = '../BinPackingData/M2d.txt'
        else:
            output_file = '../BinPackingData/M2_custom.txt'
    
    # Generuj
    generate_instance(num_bin_types, num_packages, output_file, seed)

if __name__ == '__main__':
    main()

