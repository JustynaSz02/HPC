#pragma once
#include <vector>
#include <optional>
#include "Package.h"

class Bin {
public:
	static int nextId;
	int id;
	int w;
	int h;
	std::vector<Package> packages;

	explicit Bin(int width, int height)
	    : id(nextId++), w(width), h(height) {}

	bool canFit(const Package& pkg) const;
	bool placePackage(Package pkg);
	bool removePackageById(int packageId);
};


