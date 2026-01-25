#ifdef HAVE_CUDA

#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <algorithm>
#include <cstring> // dla memcpy
#include <mutex> // dla sharedDimsMutex
#include <stdexcept> // dla std::runtime_error
#include <iostream> // dla std::cout
#include <thread> // dla std::this_thread::get_id
#include "GpuScoring.h"
#include "TabuSearch.h"

// Helper function do sprawdzania kolizji prostokątów
__device__ bool rectanglesOverlap(int x1, int y1, int w1, int h1, int x2, int y2, int w2, int h2) {
	return !(x1 + w1 <= x2 || x2 + w2 <= x1 || y1 + h1 <= y2 || y2 + h2 <= y1);
}

// Kernel do sprawdzania czy move jest valid
// Jeden wątek GPU = jeden move
__global__ void checkMoveValidityKernel(
	const int* move_pkgId,
	const int* move_fromBin,
	const int* move_toBin,
	const int* rect_x,
	const int* rect_y,
	const int* rect_w,
	const int* rect_h,
	const int* rect_binId,
	const int* binStart,
	const int* binCount,
	const int* binWidth,
	const int* binHeight,
	const int* packageWidths,
	const int* packageHeights,
	char* valid, // char zamiast bool dla CUDA compatibility
	int numMoves
) {
	int idx = blockIdx.x * blockDim.x + threadIdx.x;
	if (idx >= numMoves) return;
	
	int pkgId = move_pkgId[idx];
	int toBin = move_toBin[idx];
	
	// Pobierz wymiary paczki (przed rotacją)
	int pkgW = packageWidths[pkgId];
	int pkgH = packageHeights[pkgId];
	
	// Pobierz wymiary docelowego bina
	int binW = binWidth[toBin];
	int binH = binHeight[toBin];
	
	// Sprawdź czy paczka mieści się w binie (dla obu rotacji)
	bool fitsNormal = (pkgW <= binW && pkgH <= binH);
	bool fitsRotated = (pkgH <= binW && pkgW <= binH);
	
	if (!fitsNormal && !fitsRotated) {
		valid[idx] = 0; // char zamiast bool
		return;
	}
	
	// Sprawdź kolizje z istniejącymi paczkami w binie
	int startIdx = binStart[toBin];
	int count = binCount[toBin];
	
	// Sprawdź dla obu rotacji
	for (int rot = 0; rot < 2; ++rot) {
		int testW = (rot == 0) ? pkgW : pkgH;
		int testH = (rot == 0) ? pkgH : pkgW;
		
		if (testW > binW || testH > binH) continue;
		
		// Sprawdź wszystkie możliwe pozycje (uproszczone - tylko kilka pozycji)
		bool foundValid = false;
		for (int px = 0; px <= binW - testW && !foundValid; px += 2) {
			for (int py = 0; py <= binH - testH && !foundValid; py += 2) {
				bool hasCollision = false;
				
				// Sprawdź kolizje z istniejącymi paczkami
				for (int i = 0; i < count; ++i) {
					int rectIdx = startIdx + i;
					// rect_binId[rectIdx] powinien być == toBin (bo to są paczki z tego bina)
					// ale sprawdzamy dla bezpieczeństwa
					
					if (rectanglesOverlap(px, py, testW, testH,
					                      rect_x[rectIdx], rect_y[rectIdx],
					                      rect_w[rectIdx], rect_h[rectIdx])) {
						hasCollision = true;
						break;
					}
				}
				
				if (!hasCollision) {
					foundValid = true;
				}
			}
		}
		
		if (foundValid) {
			valid[idx] = 1; // char zamiast bool
			return;
		}
	}
	
	valid[idx] = 0; // char zamiast bool
}

// Kernel do obliczania score dla każdego move
// Uproszczona wersja - oblicza tylko liczbę użytych binów
__global__ void computeMoveScoreKernel(
	const int* move_pkgId,
	const int* move_fromBin,
	const int* move_toBin,
	const int* rect_binId,
	const int* binStart,
	const int* binCount,
	double* scores,
	int numMoves,
	int totalBins
) {
	int idx = blockIdx.x * blockDim.x + threadIdx.x;
	if (idx >= numMoves) return;
	
	int fromBin = move_fromBin[idx];
	int toBin = move_toBin[idx];
	
	// Symuluj przeniesienie - policz ile binów będzie użytych
	// Uproszczone: jeśli fromBin będzie pusty, zmniejsz liczbę binów
	// Jeśli toBin był pusty, zwiększ liczbę binów
	
	int fromBinCount = binCount[fromBin];
	int toBinCount = binCount[toBin];
	
	// Symulacja: po przeniesieniu
	int newFromBinCount = fromBinCount - 1;
	int newToBinCount = toBinCount + 1;
	
	// Oblicz liczbę użytych binów
	int usedBins = totalBins;
	if (newFromBinCount == 0 && fromBinCount > 0) {
		usedBins--; // Bin został opróżniony
	}
	if (newToBinCount == 1 && toBinCount == 0) {
		usedBins++; // Bin został użyty
	}
	
	// Score: mniej binów = lepsze
	// Dodaj małą wartość zależną od toBin dla tie-break
	double tieBreak = static_cast<double>(toBin) * 0.001;
	scores[idx] = static_cast<double>(usedBins) * 1000.0 - tieBreak;
}

// Struktura do buforowania pamięci GPU (optymalizacja - nie alokujemy za każdym razem)
struct GpuMemoryCache {
	// Pamięć GPU dla moves (zmienia się za każdym razem)
	int* d_move_pkgId;
	int* d_move_fromBin;
	int* d_move_toBin;
	char* d_valid; // char zamiast bool dla CUDA compatibility
	double* d_scores;
	int maxMoves;
	
	// Pamięć GPU dla solution (zmienia się za każdym razem)
	int* d_rect_x;
	int* d_rect_y;
	int* d_rect_w;
	int* d_rect_h;
	int* d_rect_binId;
	int* d_binStart;
	int* d_binCount;
	int* d_binWidth;
	int* d_binHeight;
	int maxPackages;
	int maxBins;
	
	// Pamięć GPU dla package dimensions (stała - buforowana)
	int* d_packageWidths;
	int* d_packageHeights;
	int maxPkgId;
	
	// Pinned memory dla szybszego transferu (package dimensions są stałe)
	int* h_packageWidths_pinned;
	int* h_packageHeights_pinned;
	
	// CUDA streams dla asynchronicznych transferów i obliczeń
	cudaStream_t stream1; // Stream dla moves
	cudaStream_t stream2; // Stream dla solution
	cudaStream_t stream3; // Stream dla wyników
	
	GpuMemoryCache() : d_move_pkgId(nullptr), d_move_fromBin(nullptr), d_move_toBin(nullptr),
	                   d_valid(nullptr), d_scores(nullptr), maxMoves(0),
	                   d_rect_x(nullptr), d_rect_y(nullptr), d_rect_w(nullptr), d_rect_h(nullptr),
	                   d_rect_binId(nullptr), d_binStart(nullptr), d_binCount(nullptr),
	                   d_binWidth(nullptr), d_binHeight(nullptr), maxPackages(0), maxBins(0) {
		// Utwórz CUDA streams (z sprawdzaniem błędów)
		// NIE inicjalizuj streams w konstruktorze - zrób to lazy przy pierwszym użyciu
		// To zapobiega zawieszeniu jeśli CUDA nie jest dostępne
		stream1 = nullptr;
		stream2 = nullptr;
		stream3 = nullptr;
	}
	
	~GpuMemoryCache() {
		// Zwolnij pamięć GPU
		if (d_move_pkgId) cudaFree(d_move_pkgId);
		if (d_move_fromBin) cudaFree(d_move_fromBin);
		if (d_move_toBin) cudaFree(d_move_toBin);
		if (d_valid) cudaFree(d_valid);
		if (d_scores) cudaFree(d_scores);
		if (d_rect_x) cudaFree(d_rect_x);
		if (d_rect_y) cudaFree(d_rect_y);
		if (d_rect_w) cudaFree(d_rect_w);
		if (d_rect_h) cudaFree(d_rect_h);
		if (d_rect_binId) cudaFree(d_rect_binId);
		if (d_binStart) cudaFree(d_binStart);
		if (d_binCount) cudaFree(d_binCount);
		if (d_binWidth) cudaFree(d_binWidth);
		if (d_binHeight) cudaFree(d_binHeight);
		// Package dimensions są współdzielone - nie zwalniamy tutaj
		// Zwolnij CUDA streams (tylko jeśli były utworzone)
		if (stream1 != nullptr) cudaStreamDestroy(stream1);
		if (stream2 != nullptr) cudaStreamDestroy(stream2);
		if (stream3 != nullptr) cudaStreamDestroy(stream3);
	}
};

// Thread-local cache - każdy wątek ma swój własny cache i stream
// Pozwala na równoległe wywołania GPU z różnych wątków CPU
thread_local GpuMemoryCache gpuCache;

// Współdzielony cache dla package dimensions (są stałe dla wszystkich wątków)
struct SharedPackageDimensions {
	int* d_packageWidths;
	int* d_packageHeights;
	int* h_packageWidths_pinned;
	int* h_packageHeights_pinned;
	int maxPkgId;
	
	SharedPackageDimensions() : d_packageWidths(nullptr), d_packageHeights(nullptr),
	                            h_packageWidths_pinned(nullptr), h_packageHeights_pinned(nullptr),
	                            maxPkgId(-1) {}
	
	~SharedPackageDimensions() {
		if (d_packageWidths) cudaFree(d_packageWidths);
		if (d_packageHeights) cudaFree(d_packageHeights);
		if (h_packageWidths_pinned) cudaFreeHost(h_packageWidths_pinned);
		if (h_packageHeights_pinned) cudaFreeHost(h_packageHeights_pinned);
	}
};

// Współdzielony cache z mutex tylko dla inicjalizacji (package dimensions są stałe)
static SharedPackageDimensions sharedPackageDims;
static std::mutex sharedDimsMutex; // Tylko do inicjalizacji współdzielonych danych

// Funkcja wrapper do wywołania GPU kernels
MoveEvaluationResult gpuEvaluateMovesCuda(
	const GpuSolutionView& solutionView,
	const GpuMovesView& movesView,
	const std::vector<int>& packageWidths,
	const std::vector<int>& packageHeights
) {
	static thread_local int gpuCallCount = 0;
	gpuCallCount++;
	if (gpuCallCount == 1) {
		std::cout << "[GPU Thread " << std::this_thread::get_id() << "] First GPU call\n";
	}
	
	MoveEvaluationResult result;
	result.valid.resize(movesView.totalMoves, 0); // char zamiast bool
	result.scores.resize(movesView.totalMoves, 1e9);
	
	if (movesView.totalMoves == 0) {
		return result;
	}
	
	// Sprawdź błędy CUDA na początku
	cudaError_t err = cudaGetLastError();
	if (err != cudaSuccess) {
		// Wyczyść błędy i kontynuuj
		cudaGetLastError();
	}
	
	// Ustaw device dla tego wątku (ważne dla multi-threading)
	err = cudaSetDevice(0);
	if (err != cudaSuccess) {
		throw std::runtime_error("cudaSetDevice failed");
	}
	
	// Inicjalizuj CUDA streams lazy (przy pierwszym użyciu)
	if (gpuCache.stream1 == nullptr) {
		err = cudaStreamCreate(&gpuCache.stream1);
		if (err != cudaSuccess) throw std::runtime_error("cudaStreamCreate failed for stream1");
		err = cudaStreamCreate(&gpuCache.stream2);
		if (err != cudaSuccess) throw std::runtime_error("cudaStreamCreate failed for stream2");
		err = cudaStreamCreate(&gpuCache.stream3);
		if (err != cudaSuccess) throw std::runtime_error("cudaStreamCreate failed for stream3");
	}
	
	int numMoves = movesView.totalMoves;
	int numPackages = solutionView.totalPackages;
	int numBins = solutionView.totalBins;
	int maxPkgId = static_cast<int>(packageWidths.size()) - 1;
	
	// Alokuj/buforuj pamięć GPU dla moves (jeśli potrzeba więcej miejsca)
	if (gpuCache.maxMoves < numMoves) {
		if (gpuCache.d_move_pkgId) cudaFree(gpuCache.d_move_pkgId);
		if (gpuCache.d_move_fromBin) cudaFree(gpuCache.d_move_fromBin);
		if (gpuCache.d_move_toBin) cudaFree(gpuCache.d_move_toBin);
		if (gpuCache.d_valid) cudaFree(gpuCache.d_valid);
		if (gpuCache.d_scores) cudaFree(gpuCache.d_scores);
		
		cudaMalloc(&gpuCache.d_move_pkgId, numMoves * sizeof(int));
		cudaMalloc(&gpuCache.d_move_fromBin, numMoves * sizeof(int));
		cudaMalloc(&gpuCache.d_move_toBin, numMoves * sizeof(int));
		cudaMalloc(&gpuCache.d_valid, numMoves * sizeof(char)); // char zamiast bool
		cudaMalloc(&gpuCache.d_scores, numMoves * sizeof(double));
		gpuCache.maxMoves = numMoves;
	}
	
	// Alokuj/buforuj pamięć GPU dla solution (jeśli potrzeba więcej miejsca)
	if (gpuCache.maxPackages < numPackages || gpuCache.maxBins < numBins) {
		if (gpuCache.d_rect_x) cudaFree(gpuCache.d_rect_x);
		if (gpuCache.d_rect_y) cudaFree(gpuCache.d_rect_y);
		if (gpuCache.d_rect_w) cudaFree(gpuCache.d_rect_w);
		if (gpuCache.d_rect_h) cudaFree(gpuCache.d_rect_h);
		if (gpuCache.d_rect_binId) cudaFree(gpuCache.d_rect_binId);
		if (gpuCache.d_binStart) cudaFree(gpuCache.d_binStart);
		if (gpuCache.d_binCount) cudaFree(gpuCache.d_binCount);
		if (gpuCache.d_binWidth) cudaFree(gpuCache.d_binWidth);
		if (gpuCache.d_binHeight) cudaFree(gpuCache.d_binHeight);
		
		cudaMalloc(&gpuCache.d_rect_x, numPackages * sizeof(int));
		cudaMalloc(&gpuCache.d_rect_y, numPackages * sizeof(int));
		cudaMalloc(&gpuCache.d_rect_w, numPackages * sizeof(int));
		cudaMalloc(&gpuCache.d_rect_h, numPackages * sizeof(int));
		cudaMalloc(&gpuCache.d_rect_binId, numPackages * sizeof(int));
		cudaMalloc(&gpuCache.d_binStart, numBins * sizeof(int));
		cudaMalloc(&gpuCache.d_binCount, numBins * sizeof(int));
		cudaMalloc(&gpuCache.d_binWidth, numBins * sizeof(int));
		cudaMalloc(&gpuCache.d_binHeight, numBins * sizeof(int));
		gpuCache.maxPackages = numPackages;
		gpuCache.maxBins = numBins;
	}
	
	// Buforuj package dimensions (są stałe - współdzielone między wszystkimi wątkami)
	// Używamy mutex tylko do inicjalizacji (raz)
	{
		std::lock_guard<std::mutex> lock(sharedDimsMutex);
		if (sharedPackageDims.maxPkgId != maxPkgId) {
			if (sharedPackageDims.d_packageWidths) cudaFree(sharedPackageDims.d_packageWidths);
			if (sharedPackageDims.d_packageHeights) cudaFree(sharedPackageDims.d_packageHeights);
			if (sharedPackageDims.h_packageWidths_pinned) cudaFreeHost(sharedPackageDims.h_packageWidths_pinned);
			if (sharedPackageDims.h_packageHeights_pinned) cudaFreeHost(sharedPackageDims.h_packageHeights_pinned);
			
			// Użyj pinned memory dla szybszego transferu
			cudaMallocHost(&sharedPackageDims.h_packageWidths_pinned, (maxPkgId + 1) * sizeof(int));
			cudaMallocHost(&sharedPackageDims.h_packageHeights_pinned, (maxPkgId + 1) * sizeof(int));
			cudaMalloc(&sharedPackageDims.d_packageWidths, (maxPkgId + 1) * sizeof(int));
			cudaMalloc(&sharedPackageDims.d_packageHeights, (maxPkgId + 1) * sizeof(int));
			
			// Skopiuj dane do pinned memory
			memcpy(sharedPackageDims.h_packageWidths_pinned, packageWidths.data(), (maxPkgId + 1) * sizeof(int));
			memcpy(sharedPackageDims.h_packageHeights_pinned, packageHeights.data(), (maxPkgId + 1) * sizeof(int));
			
			// Skopiuj z pinned memory do GPU (szybsze niż zwykły cudaMemcpy)
			cudaMemcpy(sharedPackageDims.d_packageWidths, sharedPackageDims.h_packageWidths_pinned, (maxPkgId + 1) * sizeof(int), cudaMemcpyHostToDevice);
			cudaMemcpy(sharedPackageDims.d_packageHeights, sharedPackageDims.h_packageHeights_pinned, (maxPkgId + 1) * sizeof(int), cudaMemcpyHostToDevice);
			
			sharedPackageDims.maxPkgId = maxPkgId;
		}
	}
	
	// Asynchroniczne kopiowanie danych CPU→GPU używając wielu streamów
	// Stream 1: moves (może być równoległy z stream 2)
	cudaMemcpyAsync(gpuCache.d_move_pkgId, movesView.move_pkgId.data(), numMoves * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream1);
	cudaMemcpyAsync(gpuCache.d_move_fromBin, movesView.move_fromBin.data(), numMoves * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream1);
	cudaMemcpyAsync(gpuCache.d_move_toBin, movesView.move_toBin.data(), numMoves * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream1);
	
	// Stream 2: solution (równoległy z stream 1)
	cudaMemcpyAsync(gpuCache.d_rect_x, solutionView.rect_x.data(), numPackages * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream2);
	cudaMemcpyAsync(gpuCache.d_rect_y, solutionView.rect_y.data(), numPackages * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream2);
	cudaMemcpyAsync(gpuCache.d_rect_w, solutionView.rect_w.data(), numPackages * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream2);
	cudaMemcpyAsync(gpuCache.d_rect_h, solutionView.rect_h.data(), numPackages * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream2);
	cudaMemcpyAsync(gpuCache.d_rect_binId, solutionView.rect_binId.data(), numPackages * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream2);
	cudaMemcpyAsync(gpuCache.d_binStart, solutionView.binStart.data(), numBins * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream2);
	cudaMemcpyAsync(gpuCache.d_binCount, solutionView.binCount.data(), numBins * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream2);
	cudaMemcpyAsync(gpuCache.d_binWidth, solutionView.binWidth.data(), numBins * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream2);
	cudaMemcpyAsync(gpuCache.d_binHeight, solutionView.binHeight.data(), numBins * sizeof(int), cudaMemcpyHostToDevice, gpuCache.stream2);
	
	// Czekaj aż transfery się zakończą przed uruchomieniem kernelów
	cudaStreamSynchronize(gpuCache.stream1);
	cudaStreamSynchronize(gpuCache.stream2);
	
	// Konfiguracja grid/block
	int threadsPerBlock = 256;
	int blocksPerGrid = (numMoves + threadsPerBlock - 1) / threadsPerBlock;
	
	// Wywołaj kernel sprawdzania validności (używamy stream1)
	// Używamy współdzielonych package dimensions
	checkMoveValidityKernel<<<blocksPerGrid, threadsPerBlock, 0, gpuCache.stream1>>>(
		gpuCache.d_move_pkgId, gpuCache.d_move_fromBin, gpuCache.d_move_toBin,
		gpuCache.d_rect_x, gpuCache.d_rect_y, gpuCache.d_rect_w, gpuCache.d_rect_h, gpuCache.d_rect_binId,
		gpuCache.d_binStart, gpuCache.d_binCount, gpuCache.d_binWidth, gpuCache.d_binHeight,
		sharedPackageDims.d_packageWidths, sharedPackageDims.d_packageHeights,
		gpuCache.d_valid, numMoves
	);
	
	// Czekaj aż pierwszy kernel się zakończy
	cudaStreamSynchronize(gpuCache.stream1);
	
	// Wywołaj kernel obliczania score (używamy stream1)
	computeMoveScoreKernel<<<blocksPerGrid, threadsPerBlock, 0, gpuCache.stream1>>>(
		gpuCache.d_move_pkgId, gpuCache.d_move_fromBin, gpuCache.d_move_toBin,
		gpuCache.d_rect_binId, gpuCache.d_binStart, gpuCache.d_binCount,
		gpuCache.d_scores, numMoves, numBins
	);
	
	// Czekaj aż drugi kernel się zakończy
	cudaStreamSynchronize(gpuCache.stream1);
	
	// Asynchroniczne kopiowanie wyników GPU→CPU (używamy stream3)
	std::vector<char> valid_chars(numMoves);
	cudaMemcpyAsync(valid_chars.data(), gpuCache.d_valid, numMoves * sizeof(char), cudaMemcpyDeviceToHost, gpuCache.stream3); // char zamiast bool
	cudaMemcpyAsync(result.scores.data(), gpuCache.d_scores, numMoves * sizeof(double), cudaMemcpyDeviceToHost, gpuCache.stream3);
	
	// Czekaj aż transfery wyników się zakończą
	cudaStreamSynchronize(gpuCache.stream3);
	
	// Konwertuj bool* na char* dla result.valid
	for (int i = 0; i < numMoves; ++i) {
		result.valid[i] = (valid_chars[i] != 0) ? 1 : 0;
	}
	
	// Pamięć GPU jest buforowana - nie zwalniamy jej tutaj
	// Zostanie zwolniona automatycznie przez destruktor GpuMemoryCache
	
	return result;
}

#endif // HAVE_CUDA

