#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <future>
#include <string>
#include <filesystem>
#include "Package.h"
#include "Bin.h"
#include "TabuSearch.h"
#include "Visualize.h"
#include "GpuScoring.h"
#include <random>
#include <chrono>
static bool readInstance(const std::string& path, std::vector<Bin>& bins, std::vector<Package>& packages) {
	std::ifstream f(path);
	if (!f) {
		std::cerr << "Cannot open file: " << path << "\n";
		return false;
	}
	int n = 0;
	{
		std::string line;
		if (!std::getline(f, line)) return false;
		n = std::stoi(line);
	}
	for (int i = 0; i < n; ++i) {
		std::string line;
		if (!std::getline(f, line)) return false;
		std::istringstream iss(line);
		int w, h, count;
		iss >> w >> h >> count;
		for (int j = 0; j < count; ++j) {
			bins.emplace_back(w, h);
		}
	}
	{
		std::string dummy;
		std::getline(f, dummy); // blank line
	}
	int m = 0;
	{
		std::string line;
		if (!std::getline(f, line)) return false;
		m = std::stoi(line);
	}
	for (int i = 0; i < m; ++i) {
		std::string line;
		if (!std::getline(f, line)) return false;
		std::istringstream iss(line);
		int id, w, h;
		iss >> id >> w >> h;
		packages.emplace_back(id, w, h);
	}
	return true;
}

int main(int argc, char** argv) {
	std::cout << "=== PROGRAM STARTING ===\n";
	std::cout.flush();
	
	std::string instancePath = "../../../BinPackingData/data.txt";
	int n_threads = 8;
	int iterations = 50;
	
	// Parsuj argumenty: [plik_danych] [liczba_wątków] [iteracje]
	if (argc >= 2) {
		instancePath = argv[1];
	}
	if (argc >= 3) {
		n_threads = std::stoi(argv[2]);
		if (n_threads < 1) n_threads = 1;
	}
	if (argc >= 4) {
		iterations = std::stoi(argv[3]);
		if (iterations < 1) iterations = 50;
	}
	
	std::cout << "Hardware threads: " << std::thread::hardware_concurrency() << '\n';
	std::cout.flush();
	std::vector<Bin> bins;
	std::vector<Package> packages;
	//int tabu_size = 10;
	int loops = 5;
	auto rng = std::default_random_engine{10};
	std::uniform_int_distribution<> tabu_rand(5, 20); //rozmiar listy tabu bedzie losowy od 5 do 20
	if (!readInstance(instancePath, bins, packages)) {
		std::cerr << "Failed to read instance file\n";
		return 1;
	}

	// Włącz logowanie wydajności (opcjonalne)
	setPerformanceLogging(true);
	resetPerformanceStats();
	
	std::cout << "=== START PROGRAM ===\n";
	std::cout << "CUDA available: " << (isCudaAvailable() ? "YES" : "NO") << "\n";
	std::cout << "Threads: " << n_threads << "\n";
	std::cout << "Iterations: " << iterations << "\n";
	std::cout << "Starting Tabu Search...\n";
	
	// Rozpocznij pomiar całkowitego czasu wykonania (wszystkie fazy)
	std::chrono::steady_clock::time_point programBegin = std::chrono::steady_clock::now();
	std::chrono::steady_clock::time_point begin = programBegin;
	std::vector<std::future<std::vector<Bin>>> threads;
	std::vector<std::string> best_tabu;
	for (int i = 0; i < n_threads; i++) {
		auto init = generateInitialSolution(bins, packages, i);
		threads.push_back(std::async(std::launch::async, tabuSearch, bins, packages, init, iterations, tabu_rand(rng)));
	}
	auto best = generateInitialSolution(bins, packages, 10);
	std::cout << "Best: " << evaluateSolution(best) << ", "<< evaluateSolutionTie(best) << '\n';
	for (int i = 0; i < n_threads; i++) {
		//threads[i].wait();
		auto ret = threads[i].get();
		if (evaluateSolution(best) > evaluateSolution(ret)  or (evaluateSolution(best) == evaluateSolution(ret) and evaluateSolutionTie(best) < evaluateSolutionTie(ret))) {
			best = ret;
			std::cout << "Best at " << i << ": " << evaluateSolution(best) << ", " << evaluateSolutionTie(best) << '\n';
		}
	}
	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	auto totalMs = std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count();
	std::cout << "Time elapsed = " << (totalMs / 1000.0) << "[s] (" << totalMs << "ms)" << std::endl;
	
	// Wyświetl statystyki wydajności po pierwszej iteracji (bez resetowania - zbieramy wszystkie statystyki)
	printPerformanceStats();
	// NIE resetujemy statystyk - zbieramy je przez cały program
	
	for (int i = 0; i < loops; i++) {
		std::chrono::steady_clock::time_point loopBegin = std::chrono::steady_clock::now();
		threads.clear();
		for (int j = 0; j < n_threads; j++) {
			threads.push_back(std::async(std::launch::async, tabuSearch, bins, packages, best,iterations, tabu_rand(rng)));
		}
		for (int j = 0; j < n_threads; j++) {
			//threads[i].wait();
			auto ret = threads[j].get();
			if (evaluateSolution(best) > evaluateSolution(ret) or (evaluateSolution(best) == evaluateSolution(ret) and evaluateSolutionTie(best) < evaluateSolutionTie(ret))) {
				best = ret;
				std::cout << "Best at " << j << ": " << evaluateSolution(best) << ", " << evaluateSolutionTie(best) << '\n';
			}
		}
		std::chrono::steady_clock::time_point loopEnd = std::chrono::steady_clock::now();
		auto loopMs = std::chrono::duration_cast<std::chrono::milliseconds>(loopEnd - loopBegin).count();
		std::cout << "Time elapsed = " << (loopMs / 1000.0) << "[s] (" << loopMs << "ms)" << std::endl;
	}
	
	// Wyświetl finalne statystyki wydajności
	std::cout << "\n=== Performance Statistics ===" << std::endl;
	printPerformanceStats();
	
	// Wyświetl całkowity czas wykonania (wszystkie fazy)
	std::chrono::steady_clock::time_point programEnd = std::chrono::steady_clock::now();
	auto totalTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(programEnd - programBegin).count();
	std::cout << "Total execution time: " << (totalTimeMs / 1000.0) << "s (" << totalTimeMs << "ms)" << std::endl;
	
	//auto best = tabuSearch(bins, packages, init, 100, 10);
	std::cout << "\nRozmieszczenie paczek:\n";
	for (const auto& b : best) {
		if (b.packages.empty()) continue;
		std::cout << "Bin ID " << b.id << ":\n";
		for (const auto& p : b.packages) {
			std::cout << "  Package ID " << p.id
			          << ": X=" << p.x << ", Y=" << p.y
			          << ", W=" << p.w << ", H=" << p.h
			          << ", Rotated=" << (p.rotated ? "True" : "False") << "\n";
		}
	}
	int used = evaluateSolution(best);
	std::cout << "Best solution found: " << used << "\n";
	// export SVGs
	exportSolutionToSvg(best, "out_svg", 20);
	std::cout << "SVGs zapisane w folderze: out_svg (styl v2: tytul/siatka/osi)\n";
	return 0;
}


