#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <chrono>
#include <cuda_runtime.h>
#include "Package.h"
#include "Bin.h"
#include "TabuSearch.h"
#include "Visualize.h"

static bool readInstance(const std::string& path, std::vector<Bin>& bins, std::vector<Package>& packages) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    if (!std::getline(f, line)) return false;
    int n = std::stoi(line);
    for (int i = 0; i < n; ++i) {
        if (!std::getline(f, line)) return false;
        std::istringstream iss(line);
        int w, h, count;
        iss >> w >> h >> count;
        for (int j = 0; j < count; ++j) bins.emplace_back(w, h);
    }
    std::string dummy;
    std::getline(f, dummy);
    if (!std::getline(f, line)) return false;
    int m = std::stoi(line);
    for (int i = 0; i < m; ++i) {
        if (!std::getline(f, line)) return false;
        std::istringstream iss(line);
        int id, w, h;
        iss >> id >> w >> h;
        packages.emplace_back(id, w, h);
    }
    return true;
}

int main(int argc, char** argv) {
    (void)argc; (void)argv;
    int devCount = 0;
    cudaGetDeviceCount(&devCount);
    if (devCount == 0) {
        std::cerr << "No CUDA Device!" << std::endl;
        return 1;
    }

    // UPDATE PATH HERE
    std::string path = "C:\\Users\\Jayden\\Downloads\\HPC\\BinPackingData\\M1b.txt";
    std::vector<Bin> bins;
    std::vector<Package> packages;

    if (!readInstance(path, bins, packages)) {
        std::cerr << "File Read Error" << std::endl;
        return 1;
    }

    std::vector<Bin> global_best;
    int restarts = 100; // High restart count to force finding 9 bins
    int iterations = 300;
    int tabu_tenure = 25;

    std::cout << "[GPU] Starting Optimized Tabu Search (FFD + Fill-Aware Kernel)..." << std::endl;
    auto start = std::chrono::high_resolution_clock::now();

    for (int r = 0; r < restarts; ++r) {
        // Seed 0 = Strict FFD (Best chance). Seeds > 0 = Randomized variations.
        auto init = generateInitialSolution(bins, packages, r);

        std::vector<Bin> result = tabuSearch(bins, packages, init, iterations, tabu_tenure);

        int count = evaluateSolution(result);
        double score = evaluateSolutionTie(result);

        if (r == 0 || count < evaluateSolution(global_best) ||
            (count == evaluateSolution(global_best) && score > evaluateSolutionTie(global_best))) {
            global_best = result;
            std::cout << ">>> BEST: " << count << " bins (Restart " << r << ")" << std::endl;
        }
    }

    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "\n[FINAL] Bins Used: " << evaluateSolution(global_best) << std::endl;
    std::cout << "Time: " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count() << "ms" << std::endl;

    exportSolutionToSvg(global_best, "out_svg", 20);
    return 0;
}