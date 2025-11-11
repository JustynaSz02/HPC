#pragma once
#include <vector>
#include <string>
#include "Bin.h"
#include "Package.h"

int evaluateSolution(const std::vector<Bin>& bins);
std::vector<Bin> generateInitialSolution(const std::vector<Bin>& bins, const std::vector<Package>& packages);
std::vector<std::vector<Bin>> getNeighbors(const std::vector<Bin>& solution);
std::vector<Bin> tabuSearch(const std::vector<Bin>& bins, const std::vector<Package>& packages, int iterations = 100, int tabuSize = 10);


