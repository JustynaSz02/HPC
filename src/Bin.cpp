#include "Bin.h"

int Bin::nextId = 0;

bool Bin::canFit(const Package& package) const {
	if (package.w > w || package.h > h) {
		return false;
	}
	for (const auto& p : packages) {
		const bool separated =
		    (package.x + package.w <= p.x) ||
		    (package.x >= p.x + p.w) ||
		    (package.y + package.h <= p.y) ||
		    (package.y >= p.y + p.h);
		if (!separated) {
			return false;
		}
	}
	return true;
}

bool Bin::placePackage(Package package) {
	for (int rotate = 0; rotate < 2; ++rotate) {
		if (rotate == 1) {
			package.rotate90();
		}
		for (int px = 0; px <= w - package.w; ++px) {
			for (int py = 0; py <= h - package.h; ++py) {
				Package trial = package;
				trial.moveTo(px, py);
				if (canFit(trial)) {
					packages.push_back(trial);
					return true;
				}
			}
		}
		if (rotate == 1) {
			package.rotate90(); // revert rotation if needed
		}
	}
	return false;
}

bool Bin::removePackageById(int packageId) {
	for (size_t i = 0; i < packages.size(); ++i) {
		if (packages[i].id == packageId) {
			packages.erase(packages.begin() + static_cast<long>(i));
			return true;
		}
	}
	return false;
}


