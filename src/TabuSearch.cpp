#include "TabuSearch.h"
#include "Move.h"
#include "GpuData.h"
#include "GpuScoring.h"
#include <algorithm>
#include <set>
#include <sstream>
#include <random>
#include <map>

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

// Nowa funkcja - generuje listę ruchów zamiast pełnych kopii rozwiązań
std::vector<Move> getMoves(const std::vector<Bin>& solution) {
	std::vector<Move> moves;
	for (size_t i = 0; i < solution.size(); ++i) {
		const auto& b = solution[i];
		for (const auto& p : b.packages) {
			for (size_t j = 0; j < solution.size(); ++j) {
				if (i == j) continue;
				// Dodajemy move - nie kopiujemy rozwiązania!
				moves.emplace_back(p.id, static_cast<int>(i), static_cast<int>(j));
			}
		}
	}
	return moves;
}

std::vector<Bin> tabuSearch(const std::vector<Bin>& bins, const std::vector<Package>& packages, const std::vector<Bin>& initial, int iterations, int tabuSize) {
	std::vector<Bin> current = initial;
	std::vector<Bin> best = current;
	std::set<Move> tabu; // Zmienione na std::set<Move> zamiast std::vector<std::string>

	for (int it = 0; it < iterations; ++it) {
		// Generuj ruchy zamiast pełnych sąsiadów
		auto moves = getMoves(current);
		
		// Filtruj ruchy przez tabu listę
		moves.erase(std::remove_if(
			moves.begin(), moves.end(), [&](const Move& m){
				return tabu.find(m) != tabu.end();
			}),
			moves.end()
		);
		
		if (moves.empty()) break;
		
		// Stwórz mapę pierwotnych wymiarów paczek (przed rotacją)
		// Używamy największego wymiaru jako "szerokości", mniejszego jako "wysokości"
		// (to jest przybliżenie - w rzeczywistości nie wiemy które było pierwotne)
		std::map<int, std::pair<int, int>> packageDimensions;
		for (const auto& bin : current) {
			for (const auto& pkg : bin.packages) {
				if (packageDimensions.find(pkg.id) == packageDimensions.end()) {
					// Zapisz wymiary (normalizujemy: większy = szerokość, mniejszy = wysokość)
					int w = std::max(pkg.w, pkg.h);
					int h = std::min(pkg.w, pkg.h);
					packageDimensions[pkg.id] = {w, h};
				}
			}
		}
		
		// Stwórz tablice wymiarów (indeksowane po package ID)
		int maxPkgId = 0;
		for (const auto& [id, dims] : packageDimensions) {
			maxPkgId = std::max(maxPkgId, id);
		}
		std::vector<int> packageWidths(maxPkgId + 1, 0);
		std::vector<int> packageHeights(maxPkgId + 1, 0);
		for (const auto& [id, dims] : packageDimensions) {
			packageWidths[id] = dims.first;
			packageHeights[id] = dims.second;
		}
		
		// Konwertuj rozwiązanie i ruchy do formatu GPU
		GpuSolutionView solutionView = convertToGpuView(current);
		GpuMovesView movesView = convertMovesToGpu(moves);
		
		// Oceń ruchy (GPU lub CPU fallback)
		MoveEvaluationResult evalResult = evaluateMoves(
			solutionView, movesView, packageWidths, packageHeights, current);
		
		// Znajdź najlepszy move z wyników
		int bestMoveIdx = -1;
		double bestScore = 1e9;
		for (int i = 0; i < movesView.totalMoves; ++i) {
			if (evalResult.valid[i] != 0 && evalResult.scores[i] < bestScore) { // char zamiast bool
				bestScore = evalResult.scores[i];
				bestMoveIdx = i;
			}
		}
		
		if (bestMoveIdx == -1) break; // Brak valid moves
		
		Move bestMove = moves[bestMoveIdx];
		
		// Aplikuj najlepszy move do current (1 raz!)
		// Najpierw znajdź paczkę przed usunięciem
		Package pkgToMove = Package(); // Inicjalizacja domyślna
		bool foundPkg = false;
		for (const auto& p : current[bestMove.fromBin].packages) {
			if (p.id == bestMove.pkgId) {
				pkgToMove = p;
				foundPkg = true;
				break;
			}
		}
		
		if (foundPkg && current[bestMove.fromBin].removePackageById(bestMove.pkgId)) {
			Package pkgCopy = pkgToMove;
			pkgCopy.x = 0;
			pkgCopy.y = 0;
			current[bestMove.toBin].placePackage(pkgCopy);
		}
		
		// Aktualizuj best solution
		if (evaluateSolution(current) < evaluateSolution(best)) {
			best = current;
		}
		else if (evaluateSolution(current) == evaluateSolution(best)) {
			if (evaluateSolutionTie(current) > evaluateSolutionTie(best)) {
				best = current;
			}
		}
		
		// Dodaj move do tabu listy
		tabu.insert(bestMove);
		if (static_cast<int>(tabu.size()) > tabuSize) {
			tabu.erase(tabu.begin());
		}
	}
	
	return best;
}


