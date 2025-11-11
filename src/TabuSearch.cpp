#include "TabuSearch.h"
#include <algorithm>
#include <set>
#include <sstream>

int evaluateSolution(const std::vector<Bin>& bins) {
	int used = 0;
	for (const auto& b : bins) {
		if (!b.packages.empty()) {
			++used;
		}
	}
	return used;
}

std::vector<Bin> generateInitialSolution(const std::vector<Bin>& inputBins, const std::vector<Package>& packages) {
	std::vector<Bin> bins = inputBins;
	for (const auto& p : packages) {
		for (auto& b : bins) {
			Package copy = p;
			if (b.placePackage(copy)) {
				break;
			}
		}
	}
	return bins;
}

static std::string reprSolution(const std::vector<Bin>& bins) {
	std::ostringstream oss;
	for (const auto& b : bins) {
		oss << "B" << b.id << ":";
		for (const auto& p : b.packages) {
			oss << p.id << ",";
		}
		oss << "|";
	}
	return oss.str();
}

std::vector<std::vector<Bin>> getNeighbors(const std::vector<Bin>& solution) {
	std::vector<std::vector<Bin>> neighbors;
	for (size_t i = 0; i < solution.size(); ++i) {
		const auto& b = solution[i];
		for (const auto& p : b.packages) {
			for (size_t j = 0; j < solution.size(); ++j) {
				if (i == j) continue;
				std::vector<Bin> newSol = solution;
				if (newSol[i].removePackageById(p.id)) {
					Package pcopy = p;
					if (newSol[j].placePackage(pcopy)) {
						neighbors.push_back(std::move(newSol));
					}
				}
			}
		}
	}
	return neighbors;
}

std::vector<Bin> tabuSearch(const std::vector<Bin>& bins, const std::vector<Package>& packages, int iterations, int tabuSize) {
	std::vector<Bin> current = generateInitialSolution(bins, packages);
	std::vector<Bin> best = current;
	std::vector<std::string> tabu;
	tabu.reserve(static_cast<size_t>(tabuSize) + 1);

	for (int it = 0; it < iterations; ++it) {
		auto neighbors = getNeighbors(current);
		neighbors.erase(
			std::remove_if(neighbors.begin(), neighbors.end(), [&](const std::vector<Bin>& s){
				std::string r = reprSolution(s);
				return std::find(tabu.begin(), tabu.end(), r) != tabu.end();
			}),
			neighbors.end()
		);
		if (neighbors.empty()) break;
		std::stable_sort(neighbors.begin(), neighbors.end(), [](const auto& a, const auto& b){
			return evaluateSolution(a) < evaluateSolution(b);
		});
		current = neighbors.front();
		if (evaluateSolution(current) < evaluateSolution(best)) {
			best = current;
		}
		tabu.push_back(reprSolution(current));
		if (static_cast<int>(tabu.size()) > tabuSize) {
			tabu.erase(tabu.begin());
		}
	}
	return best;
}


