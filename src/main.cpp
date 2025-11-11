#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <string>
#include <filesystem>
#include "Package.h"
#include "Bin.h"
#include "TabuSearch.h"
#include "Visualize.h"

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
	std::string instancePath = "BinPackingData/M1a.txt";
	if (argc >= 2) {
		instancePath = argv[1];
	}
	std::vector<Bin> bins;
	std::vector<Package> packages;
	if (!readInstance(instancePath, bins, packages)) {
		std::cerr << "Failed to read instance file\n";
		return 1;
	}
	auto best = tabuSearch(bins, packages, 100, 10);
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


