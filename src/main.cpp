#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <future>
#include <string>
#include <filesystem>
#include <thread>
#include "Package.h"
#include "Bin.h"
#include "TabuSearch.h"
#include "Visualize.h"
#include <random>
#include <chrono>

static bool readInstance(const std::string& path, std::vector<Bin>& bins, std::vector<Package>& packages) {
	std::ifstream f(path);
	if (!f) {
		std::cerr << "[ERROR] Cannot open file: " << path << "\n";
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
		std::getline(f, dummy);
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
	std::string instancePath = "BinPackingData/M1b.txt";
	if (argc >= 2) {
		instancePath = argv[1];
	}

	std::cout << "--- SYSTEM INFO ---" << std::endl;
	std::cout << "Hardware Concurrency: " << std::thread::hardware_concurrency() << " cores available." << std::endl;

	std::vector<Bin> bins;
	std::vector<Package> packages;
	int n_threads = 6;
	int iterations = 100;
	int loops = 5;
	auto rng = std::default_random_engine{ 10 };
	std::uniform_int_distribution<> tabu_rand(5, 20);

	if (!readInstance(instancePath, bins, packages)) {
		std::cerr << "[CRITICAL] Failed to read instance file." << std::endl;
		return 1;
	}
	std::cout << "[SUCCESS] Data loaded. Packages: " << packages.size() << std::endl;

	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();
	std::vector<std::future<std::vector<Bin>>> threads;

	std::cout << "\n--- PHASE 1: INITIAL RANDOMIZED SEARCH ---" << std::endl;
	for (int i = 0; i < n_threads; i++) {
		auto init = generateInitialSolution(bins, packages, i);
		threads.push_back(std::async(std::launch::async, tabuSearch, bins, packages, init, iterations, tabu_rand(rng)));
	}

	std::vector<Bin> best;
	bool first_collected = true;

	for (int i = 0; i < n_threads; i++) {
		auto ret = threads[i].get();
		int current_bins = evaluateSolution(ret);
		double current_tie = evaluateSolutionTie(ret);
		std::cout << "[MAIN] Thread " << i << " returned. Bins: " << current_bins << ", Score: " << current_tie << std::endl;

		if (first_collected || current_bins < evaluateSolution(best) ||
			(current_bins == evaluateSolution(best) && current_tie > evaluateSolutionTie(best))) {
			best = ret;
			first_collected = false;
			std::cout << ">>> [UPDATE] New Global Best from Thread " << i << "!" << std::endl;
		}
	}

	// REFINEMENT LOOPS
	for (int l = 0; l < loops; l++) {
		std::cout << "\n--- PHASE 2: REFINEMENT LOOP " << l + 1 << "/" << loops << " ---" << std::endl;
		std::chrono::steady_clock::time_point loop_begin = std::chrono::steady_clock::now();
		threads.clear();

		for (int j = 0; j < n_threads; j++) {
			threads.push_back(std::async(std::launch::async, tabuSearch, bins, packages, best, iterations, tabu_rand(rng)));
		}

		for (int j = 0; j < n_threads; j++) {
			auto ret = threads[j].get();
			int ret_bins = evaluateSolution(ret);
			double ret_tie = evaluateSolutionTie(ret);

			if (ret_bins < evaluateSolution(best) || (ret_bins == evaluateSolution(best) && ret_tie > evaluateSolutionTie(best))) {
				best = ret;
				std::cout << ">>> [UPDATE] Loop " << l + 1 << ": Improved Best via Thread " << j << " (Bins: " << ret_bins << ")" << std::endl;
			}
		}
		auto loop_end = std::chrono::steady_clock::now();
		std::cout << "[INFO] Loop completed in " << std::chrono::duration_cast<std::chrono::milliseconds>(loop_end - loop_begin).count() << "ms" << std::endl;
	}

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	std::cout << "\n--- FINAL RESULTS ---" << std::endl;
	std::cout << "Total Time: " << std::chrono::duration_cast<std::chrono::seconds>(end - begin).count() << "s" << std::endl;
	std::cout << "Final Bins Used: " << evaluateSolution(best) << std::endl;

	exportSolutionToSvg(best, "out_svg", 20);
	std::cout << "[SUCCESS] Visualization saved." << std::endl;

	return 0;
}