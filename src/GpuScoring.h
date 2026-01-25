#pragma once
#include "GpuData.h"
#include <vector>

// Struktura wyników oceny ruchów
struct MoveEvaluationResult {
	std::vector<char> valid;    // Czy move jest możliwy (char zamiast bool dla CUDA compatibility)
	std::vector<double> scores; // Ocena każdego move (niższe = lepsze)
};

// Funkcja sprawdzająca czy CUDA jest dostępne w runtime
bool isCudaAvailable();

// Funkcja oceniająca ruchy na GPU (lub CPU fallback)
// Parametry:
//   solutionView - rozwiązanie w formacie GPU
//   movesView - ruchy w formacie GPU
//   packageWidths - szerokość każdej paczki (przed rotacją), indeksowane po package ID
//   packageHeights - wysokość każdej paczki (przed rotacją), indeksowane po package ID
//   originalSolution - oryginalne rozwiązanie (potrzebne do symulacji w CPU fallback)
MoveEvaluationResult evaluateMoves(
	const GpuSolutionView& solutionView,
	const GpuMovesView& movesView,
	const std::vector<int>& packageWidths,
	const std::vector<int>& packageHeights,
	const std::vector<Bin>& originalSolution
);

// Wersja CPU fallback (zawsze dostępna)
MoveEvaluationResult evaluateMovesCpu(
	const GpuSolutionView& solutionView,
	const GpuMovesView& movesView,
	const std::vector<int>& packageWidths,
	const std::vector<int>& packageHeights,
	const std::vector<Bin>& originalSolution  // Potrzebne do symulacji
);

// Wersja GPU (tylko jeśli CUDA dostępne)
#ifdef HAVE_CUDA
MoveEvaluationResult gpuEvaluateMovesCuda(
	const GpuSolutionView& solutionView,
	const GpuMovesView& movesView,
	const std::vector<int>& packageWidths,
	const std::vector<int>& packageHeights
);
#endif

// Funkcje do logowania wydajności (opcjonalne)
void setPerformanceLogging(bool enable);
void resetPerformanceStats();
void printPerformanceStats();

