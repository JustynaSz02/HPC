#pragma once
#include <vector>
#include "Bin.h"
#include "Move.h"

// Struktura reprezentująca rozwiązanie w formacie GPU (płaskie tablice)
struct GpuSolutionView {
	std::vector<int> rect_x;      // Pozycja X każdej paczki
	std::vector<int> rect_y;      // Pozycja Y każdej paczki
	std::vector<int> rect_w;      // Szerokość każdej paczki
	std::vector<int> rect_h;      // Wysokość każdej paczki
	std::vector<int> rect_binId; // Do którego bina należy paczka
	std::vector<int> rect_pkgId; // ID paczki
	std::vector<int> binStart;    // Indeks pierwszej paczki w binie [b]
	std::vector<int> binCount;   // Liczba paczek w binie [b]
	std::vector<int> binWidth;   // Szerokość bina [b]
	std::vector<int> binHeight;  // Wysokość bina [b]
	
	int totalPackages;            // Całkowita liczba paczek
	int totalBins;                // Całkowita liczba binów
};

// Konwertuje rozwiązanie (std::vector<Bin>) na format GPU
GpuSolutionView convertToGpuView(const std::vector<Bin>& solution);

// Struktura reprezentująca ruchy w formacie GPU
struct GpuMovesView {
	std::vector<int> move_pkgId;  // ID paczki do przeniesienia
	std::vector<int> move_fromBin; // Indeks bina źródłowego
	std::vector<int> move_toBin;   // Indeks bina docelowego
	
	int totalMoves;               // Całkowita liczba ruchów
};

// Konwertuje listę Move na format GPU
GpuMovesView convertMovesToGpu(const std::vector<Move>& moves);

