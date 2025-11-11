#pragma once
#include <string>

class Package {
public:
	int id;
	int w;
	int h;
	int x;
	int y;
	bool rotated;

	Package() = default;
	Package(int packageId, int width, int height, int px = 0, int py = 0)
	    : id(packageId), w(width), h(height), x(px), y(py), rotated(false) {}

	void moveTo(int nx, int ny) {
		x = nx;
		y = ny;
	}

	void rotate90() {
		std::swap(w, h);
		rotated = !rotated;
	}
};


