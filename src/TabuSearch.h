#pragma once
#include <vector>
#include <string>
#include "Bin.h"
#include "Package.h"

int evaluateSolution(const std::vector<Bin>& bins);
std::vector<Bin> generateInitialSolution(const std::vector<Bin>& bins, const std::vector<Package>& packages, unsigned int seed);
std::vector<std::vector<Bin>> getNeighbors(const std::vector<Bin>& solution);
std::pair<std::vector<Bin>, std::vector<std::string>> tabuSearch(const std::vector<Bin>& bins, const std::vector<Package>& packages, const std::vector<Bin>& initial,const std::vector<std::string>& t_list, int iterations = 100, int tabuSize = 10);


