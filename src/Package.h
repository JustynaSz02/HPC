#pragma once

class Package {
public:
	int id;
	int w;
	int h;
	int x;
	int y;
	int area;
	bool rotated;

	Package() = default;
	Package(int packageId, int width, int height, int px = 0, int py = 0)
		: id(packageId), w(width), h(height), x(px), y(py), rotated(false), area(width* height) {
	}

	void moveTo(int nx, int ny) {
		x = nx;
		y = ny;
	}

	void rotate90() {
		int temp = w; w = h; h = temp;
		rotated = !rotated;
	}
};