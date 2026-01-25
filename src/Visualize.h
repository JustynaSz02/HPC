#pragma once
#include <vector>
#include <string>
#include "Bin.h"

// Exports each used bin to an SVG file in outputDir (creates it if missing).
// Files: bin_<id>.svg
void exportSolutionToSvg(const std::vector<Bin>& bins, const std::string& outputDir, int scale = 20);


