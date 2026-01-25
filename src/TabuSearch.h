#pragma once
#include <vector>
#include <string>
#include "Bin.h"
#include "Package.h"
#include "Move.h"

int evaluateSolution(const std::vector<Bin>& bins);
double evaluateSolutionTie(const std::vector<Bin>& bins);
std::vector<Bin> generateInitialSolution(const std::vector<Bin>& bins, const std::vector<Package>& packages, unsigned int seed);
std::vector<std::vector<Bin>> getNeighbors(const std::vector<Bin>& solution); // DEPRECATED - użyj getMoves()
std::vector<Move> getMoves(const std::vector<Bin>& solution);
std::vector<Bin> tabuSearch(const std::vector<Bin>& bins, const std::vector<Package>& packages, const std::vector<Bin>& initial, int iterations = 100, int tabuSize = 10);


