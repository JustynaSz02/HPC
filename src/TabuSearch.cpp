#include "TabuSearch.h"
#include <algorithm>
#include <set>
#include <random>
#include <iostream>
#include <thread>

size_t hashSolution(const std::vector<Bin>& bins) {
	size_t h = 0;
	for (const auto& b : bins) {
		if (b.packages.empty()) continue;
		for (const auto& p : b.packages) {
			// Combining hashes of package IDs and bin IDs
			h ^= std::hash<int>{}(p.id) + 0x9e3779b9 + (h << 6) + (h >> 2);
			h ^= std::hash<int>{}(b.id) + 0x9e3779b9 + (h << 6) + (h >> 2);
		}
	}
	return h;
}

int evaluateSolution(const std::vector<Bin>& bins) {
	int used = 0;
	for (const auto& b : bins) {
		if (!b.packages.empty()) ++used;
	}
	return used;
}

double evaluateSolutionTie(const std::vector<Bin>& bins) {
	double score = 0.0;
	for (const auto& b : bins) {
		score += b.evaluateBin();
	}
	return score;
}

std::vector<Bin> generateInitialSolution(const std::vector<Bin>& inputBins, const std::vector<Package>& packages, unsigned int seed) {
	std::vector<Bin> bins = inputBins;
	std::vector<Package> pack = packages;
	auto rng = std::default_random_engine{ seed };
	std::shuffle(pack.begin(), pack.end(), rng);
	for (const auto& p : pack) {
		for (auto& b : bins) {
			if (b.placePackage(p)) break;
		}
	}
	return bins;
}

std::vector<Bin> tabuSearch(const std::vector<Bin>& bins, const std::vector<Package>& packages, const std::vector<Bin>& initial, int iterations, int tabuSize) {
	std::vector<Bin> current = initial;
	std::vector<Bin> best_local = current;
	std::vector<size_t> tabu_list;
	tabu_list.reserve(tabuSize + 1);

	for (int it = 0; it < iterations; ++it) {
		std::vector<Bin> best_neighbor;
		bool found_neighbor = false;
		int best_nb_bins = 1e9;
		double best_nb_score = -1.0;

		for (size_t i = 0; i < current.size(); ++i) {
			if (current[i].packages.empty()) continue;

			for (size_t p_idx = 0; p_idx < current[i].packages.size(); ++p_idx) {
				Package p = current[i].packages[p_idx];

				for (size_t j = 0; j < current.size(); ++j) {
					if (i == j) continue;

					int current_bin_fill = 0;
					for (const auto& pkg : current[j].packages) current_bin_fill += pkg.area;
					if (current_bin_fill + p.area > current[j].area) continue;

					std::vector<Bin> next_step = current;
					next_step[i].removePackageById(p.id);

					if (next_step[j].placePackage(p)) {
						size_t h = hashSolution(next_step);
						if (std::find(tabu_list.begin(), tabu_list.end(), h) == tabu_list.end()) {
							int n_bins = evaluateSolution(next_step);
							double n_score = evaluateSolutionTie(next_step);

							if (n_bins < best_nb_bins || (n_bins == best_nb_bins && n_score > best_nb_score)) {
								best_neighbor = std::move(next_step);
								best_nb_bins = n_bins;
								best_nb_score = n_score;
								found_neighbor = true;
							}
						}
					}
				}
			}
		}

		if (!found_neighbor) break;

		current = best_neighbor;
		if (evaluateSolution(current) < evaluateSolution(best_local) ||
			(evaluateSolution(current) == evaluateSolution(best_local) && evaluateSolutionTie(current) > evaluateSolutionTie(best_local))) {
			best_local = current;
		}

		tabu_list.push_back(hashSolution(current));
		if (tabu_list.size() > (size_t)tabuSize) tabu_list.erase(tabu_list.begin());
	}

	return best_local;
}