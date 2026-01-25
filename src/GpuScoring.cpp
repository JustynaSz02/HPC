#include "GpuScoring.h"
#include "Bin.h"
#include "TabuSearch.h"
#include <algorithm>
#include <mutex>
#include <chrono>
#include <iostream>
#include <thread>

#ifdef HAVE_CUDA
#include <cuda_runtime.h>
// Deklaracja funkcji z GpuScoring.cu (bez extern "C" - używa C++ linkage)
MoveEvaluationResult gpuEvaluateMovesCuda(
	const GpuSolutionView& solutionView,
	const GpuMovesView& movesView,
	const std::vector<int>& packageWidths,
	const std::vector<int>& packageHeights
);
#endif

// Mutex usunięty - każdy wątek ma swój własny CUDA stream i może używać GPU równolegle
// CUDA obsługuje równoległe wywołania z różnych wątków CPU

// Statystyki wydajności (opcjonalne logowanie)
// Używamy mutex do synchronizacji w multi-threading
static long long totalEvaluationTimeMs = 0;
static long long totalEvaluations = 0;
static bool enablePerformanceLogging = false;
static long long gpuEvaluations = 0;
static long long cpuEvaluations = 0;
static std::mutex statsMutex;  // Mutex do synchronizacji statystyk

void setPerformanceLogging(bool enable) {
	enablePerformanceLogging = enable;
}

void resetPerformanceStats() {
	std::lock_guard<std::mutex> lock(statsMutex);
	totalEvaluationTimeMs = 0;
	totalEvaluations = 0;
	gpuEvaluations = 0;
	cpuEvaluations = 0;
}

void printPerformanceStats() {
	std::lock_guard<std::mutex> lock(statsMutex);
	if (totalEvaluations > 0) {
		double avgTime = static_cast<double>(totalEvaluationTimeMs) / totalEvaluations;
		std::string mode = (gpuEvaluations > 0) ? "GPU" : "CPU";
		std::cout << "Performance stats - evaluateMoves(): "
		          << "total calls=" << totalEvaluations
		          << ", total time=" << totalEvaluationTimeMs << "ms"
		          << ", avg time=" << avgTime << "ms/call"
		          << ", mode=" << mode;
		if (gpuEvaluations > 0 && cpuEvaluations > 0) {
			std::cout << " (GPU=" << gpuEvaluations << ", CPU=" << cpuEvaluations << ")";
		}
		std::cout << std::endl;
	}
}

bool isCudaAvailable() {
#ifdef HAVE_CUDA
	try {
		int deviceCount = 0;
		cudaError_t error = cudaGetDeviceCount(&deviceCount);
		if (error != cudaSuccess || deviceCount == 0) {
			return false;
		}
		// Sprawdź czy pierwsze urządzenie jest dostępne
		cudaDeviceProp prop;
		error = cudaGetDeviceProperties(&prop, 0);
		return (error == cudaSuccess);
	} catch (...) {
		// W razie jakiegokolwiek błędu, zwróć false
		return false;
	}
#else
	return false;
#endif
}

MoveEvaluationResult evaluateMovesCpu(
	const GpuSolutionView& solutionView,
	const GpuMovesView& movesView,
	const std::vector<int>& packageWidths,
	const std::vector<int>& packageHeights,
	const std::vector<Bin>& originalSolution) {
	(void)solutionView; // Nieużywany parametr - CPU fallback używa originalSolution zamiast GPU view
	
	MoveEvaluationResult result;
	result.valid.resize(movesView.totalMoves, 0); // char zamiast bool
	result.scores.resize(movesView.totalMoves, 1e9); // Duża wartość = zły move
	
	// Dla każdego move sprawdź czy jest valid i oblicz score
	for (int i = 0; i < movesView.totalMoves; ++i) {
		int pkgId = movesView.move_pkgId[i];
		int fromBin = movesView.move_fromBin[i];
		int toBin = movesView.move_toBin[i];
		
		// Symuluj move
		std::vector<Bin> testSol = originalSolution;
		
		// Usuń paczkę z bina źródłowego
		if (!testSol[fromBin].removePackageById(pkgId)) {
			result.valid[i] = 0; // char zamiast bool
			continue;
		}
		
		// Znajdź wymiary paczki (przed rotacją)
		int pkgW = packageWidths[pkgId];
		int pkgH = packageHeights[pkgId];
		
		// Spróbuj umieścić paczkę w docelowym binie (z rotacją)
		Package testPkg(pkgId, pkgW, pkgH);
		testPkg.x = 0;
		testPkg.y = 0;
		
		if (testSol[toBin].placePackage(testPkg)) {
			result.valid[i] = 1; // char zamiast bool
			// Oblicz score
			int usedBins = evaluateSolution(testSol);
			double tieScore = evaluateSolutionTie(testSol);
			// Score: mniej binów = lepsze, więcej tie = lepsze
			result.scores[i] = static_cast<double>(usedBins) * 1000.0 - tieScore;
		} else {
			result.valid[i] = 0; // char zamiast bool
		}
	}
	
	return result;
}

MoveEvaluationResult evaluateMoves(
	const GpuSolutionView& solutionView,
	const GpuMovesView& movesView,
	const std::vector<int>& packageWidths,
	const std::vector<int>& packageHeights,
	const std::vector<Bin>& originalSolution) {
	
	// Każdy wątek ma swój własny CUDA stream - równoległe wywołania GPU są możliwe
	// Mutex usunięty - CUDA obsługuje równoległe wywołania z różnych wątków CPU
	
	static thread_local int callCount = 0;
	callCount++;
	if (callCount == 1) {
		std::cout << "[Thread " << std::this_thread::get_id() << "] First evaluateMoves call\n";
	}
	
	auto startTime = std::chrono::steady_clock::now();
	
	// Sprawdź czy CUDA jest dostępne
	bool usingGpu = false;
	MoveEvaluationResult result;
	
#ifdef HAVE_CUDA
	if (isCudaAvailable()) {
		try {
			// Użyj GPU evaluation
			result = gpuEvaluateMovesCuda(solutionView, movesView, packageWidths, packageHeights);
			usingGpu = true;
		} catch (...) {
			// W razie błędu fallback na CPU
			usingGpu = false;
			result = evaluateMovesCpu(solutionView, movesView, packageWidths, packageHeights, originalSolution);
		}
	} else {
		// CUDA niedostępne w runtime - użyj CPU
		usingGpu = false;
		result = evaluateMovesCpu(solutionView, movesView, packageWidths, packageHeights, originalSolution);
	}
#else
	// CUDA nie jest dostępne w kompilacji - użyj CPU
	usingGpu = false;
	result = evaluateMovesCpu(solutionView, movesView, packageWidths, packageHeights, originalSolution);
#endif
	
	auto endTime = std::chrono::steady_clock::now();
	auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(endTime - startTime);
	
	// Zbierz statystyki (tylko jeśli włączone) - z synchronizacją
	if (enablePerformanceLogging) {
		std::lock_guard<std::mutex> lock(statsMutex);
		totalEvaluationTimeMs += duration.count();
		totalEvaluations++;
		if (usingGpu) {
			gpuEvaluations++;
		} else {
			cpuEvaluations++;
		}
	}
	
	return result;
}

