#include "GpuData.h"

GpuSolutionView convertToGpuView(const std::vector<Bin>& solution) {
	GpuSolutionView view;
	
	view.totalBins = static_cast<int>(solution.size());
	view.binStart.resize(view.totalBins);
	view.binCount.resize(view.totalBins);
	view.binWidth.resize(view.totalBins);
	view.binHeight.resize(view.totalBins);
	
	int packageIndex = 0;
	
	// Przejdź przez wszystkie biny i zbierz dane paczek
	for (int binIdx = 0; binIdx < view.totalBins; ++binIdx) {
		const auto& bin = solution[binIdx];
		
		view.binStart[binIdx] = packageIndex;
		view.binCount[binIdx] = static_cast<int>(bin.packages.size());
		view.binWidth[binIdx] = bin.w;
		view.binHeight[binIdx] = bin.h;
		
		// Dodaj wszystkie paczki z tego bina
		for (const auto& pkg : bin.packages) {
			view.rect_x.push_back(pkg.x);
			view.rect_y.push_back(pkg.y);
			view.rect_w.push_back(pkg.w);
			view.rect_h.push_back(pkg.h);
			view.rect_binId.push_back(binIdx);
			view.rect_pkgId.push_back(pkg.id);
			packageIndex++;
		}
	}
	
	view.totalPackages = packageIndex;
	
	return view;
}

GpuMovesView convertMovesToGpu(const std::vector<Move>& moves) {
	GpuMovesView view;
	
	view.totalMoves = static_cast<int>(moves.size());
	view.move_pkgId.reserve(view.totalMoves);
	view.move_fromBin.reserve(view.totalMoves);
	view.move_toBin.reserve(view.totalMoves);
	
	for (const auto& move : moves) {
		view.move_pkgId.push_back(move.pkgId);
		view.move_fromBin.push_back(move.fromBin);
		view.move_toBin.push_back(move.toBin);
	}
	
	return view;
}

