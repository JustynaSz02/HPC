#include "TabuSearch.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <random>

int evaluateSolution(const std::vector<Bin>& bins) {
	int used = 0;
	for (const auto& b : bins) {
		if (!b.packages.empty()) {
			++used;
		}
	}
	return used;
}

double evaluateSolutionTie(const std::vector<Bin>& bins) {
	double score = 0.0;
	for (const auto& b : bins) {
		score = score + b.evaluateBin();
	}
	return score;
}

std::vector<Bin> generateInitialSolution(const std::vector<Bin>& inputBins, const std::vector<Package>& packages, unsigned int seed) {
	std::vector<Bin> bins = inputBins;
	std::vector<Package> pack = packages;
	auto rng = std::default_random_engine{seed};
	std::shuffle(std::begin(pack), std::end(pack), rng);
	for (const auto& p : pack) {
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

std::vector<Bin> tabuSearch(const std::vector<Bin>& bins, const std::vector<Package>& packages, const std::vector<Bin>& initial, int iterations, int tabuSize) {
	std::vector<Bin> current = initial;
	std::vector<Bin> best = current;
	std::vector<std::string> tabu;
	tabu.reserve(static_cast<size_t>(tabuSize) + 1);

	for (int it = 0; it < iterations; ++it) {
		auto neighbors = getNeighbors(current);
		neighbors.erase(std::remove_if(
			neighbors.begin(), neighbors.end(), [&](const std::vector<Bin>& s){
				std::string r = reprSolution(s);
				return std::find(tabu.begin(), tabu.end(), r) != tabu.end();
			}),
			neighbors.end()
		);
		if (neighbors.empty()) break;
		std::stable_sort(neighbors.begin(), neighbors.end(), [](const auto& a, const auto& b){
			if (evaluateSolution(a) == evaluateSolution(b)) {
				return evaluateSolutionTie(a) > evaluateSolutionTie(b);
			}else{
				return evaluateSolution(a) < evaluateSolution(b);
			}
			
		});
		current = neighbors.front();
		if (evaluateSolution(current) < evaluateSolution(best)) {
			best = current;
		}
		else if (evaluateSolution(current) == evaluateSolution(best)) {
			if (evaluateSolution(current) > evaluateSolution(best)) {
				best = current;
			}
		}
		tabu.push_back(reprSolution(current));
		if (static_cast<int>(tabu.size()) > tabuSize) {
			tabu.erase(tabu.begin());
		}
	}
	
	return best;
}


