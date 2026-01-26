#pragma once
#include <vector>
#include "Bin.h"
#include "Package.h"

int evaluateSolution(const std::vector<Bin>& bins);
double evaluateSolutionTie(const std::vector<Bin>& bins);
std::vector<Bin> generateInitialSolution(const std::vector<Bin>& bins, const std::vector<Package>& packages, unsigned int seed);
std::vector<Bin> tabuSearch(const std::vector<Bin>& bins, const std::vector<Package>& packages, const std::vector<Bin>& initial, int iterations = 1000, int tabuSize = 10);