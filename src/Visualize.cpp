#include "Visualize.h"
#include <filesystem>
#include <fstream>
#include <iomanip>

namespace fs = std::filesystem;

static void writeSvgBin(const Bin& b, const fs::path& filepath, int scale) {
	const int svgW = b.w * scale;
	const int svgH = b.h * scale;
	const int offsetX = std::max(40, scale + 16);   // left margin for Y labels (more space)
	const int offsetY = std::max(36, scale + 12);  // top margin for title
	const int canvasW = svgW + offsetX + 12;
	const int canvasH = svgH + offsetY + 32;       // bottom margin for X labels
	std::ofstream out(filepath, std::ios::out | std::ios::trunc);
	if (!out) return;
	out << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << canvasW
	    << "\" height=\"" << canvasH << "\" viewBox=\"0 0 " << canvasW << " " << canvasH << "\">\n";
	// draw bin border
	out << "  <rect x=\"" << offsetX << "\" y=\"" << offsetY << "\" width=\"" << svgW << "\" height=\"" << svgH
	    << "\" fill=\"white\" stroke=\"black\" stroke-width=\"2\" />\n";
	// axes (left and bottom, stronger)
	out << "  <g stroke=\"#666666\" stroke-width=\"2\">\n";
	out << "    <line x1=\"" << offsetX << "\" y1=\"" << (offsetY + svgH) << "\" x2=\"" << (offsetX + svgW) << "\" y2=\"" << (offsetY + svgH) << "\" />\n"; // X axis
	out << "    <line x1=\"" << offsetX << "\" y1=\"" << offsetY << "\" x2=\"" << offsetX << "\" y2=\"" << (offsetY + svgH) << "\" />\n"; // Y axis
	out << "  </g>\n";
	// draw grid (every 1 unit, lighter)
	out << "  <g stroke=\"#eeeeee\" stroke-width=\"1\">\n";
	for (int gx = 1; gx < b.w; ++gx) {
		int x = offsetX + gx * scale;
		out << "    <line x1=\"" << x << "\" y1=\"" << offsetY << "\" x2=\"" << x << "\" y2=\"" << (offsetY + svgH) << "\" />\n";
	}
	for (int gy = 1; gy < b.h; ++gy) {
		int y = offsetY + gy * scale;
		out << "    <line x1=\"" << offsetX << "\" y1=\"" << y << "\" x2=\"" << (offsetX + svgW) << "\" y2=\"" << y << "\" />\n";
	}
	out << "  </g>\n";
	// ticks and labels every 2 units on bottom and left borders
	const int labelFont = std::max(10, scale - 2);
	out << "  <g stroke=\"#999999\" stroke-width=\"2\">\n";
	for (int gx = 0; gx <= b.w; gx += 2) {
		int x = offsetX + gx * scale;
		// bottom ticks
		out << "    <line x1=\"" << x << "\" y1=\"" << (offsetY + svgH) << "\" x2=\"" << x << "\" y2=\"" << (offsetY + svgH + 10) << "\" />\n";
		// label
		out << "    <text x=\"" << x << "\" y=\"" << (offsetY + svgH + 26) << "\" text-anchor=\"middle\" font-family=\"sans-serif\" font-size=\"" << labelFont << "\" fill=\"black\" stroke=\"none\">" << gx << "</text>\n";
	}
	for (int gy = 0; gy <= b.h; gy += 2) {
		int y = offsetY + (b.h - gy) * scale;
		// left ticks
		out << "    <line x1=\"" << (offsetX - 12) << "\" y1=\"" << y << "\" x2=\"" << offsetX << "\" y2=\"" << y << "\" />\n";
		// label
		out << "    <text x=\"" << (offsetX - 16) << "\" y=\"" << (y + 5) << "\" text-anchor=\"end\" font-family=\"sans-serif\" font-size=\"" << labelFont << "\" fill=\"black\" stroke=\"none\">" << gy << "</text>\n";
	}
	out << "  </g>\n";
	// title (draw AFTER background/grid so it's not covered)
	out << "  <text x=\"" << (offsetX + svgW / 2) << "\" y=\"" << (offsetY - 12) << "\" text-anchor=\"middle\" "
	    << "font-size=\"" << std::max(12, scale) << "\" font-family=\"sans-serif\">Bin " << b.id << "</text>\n";
	// draw packages
	for (const auto& p : b.packages) {
		const int x = offsetX + p.x * scale;
		const int y = offsetY + (b.h - (p.y + p.h)) * scale; // invert Y to make origin bottom-left visually
		const int w = p.w * scale;
		const int h = p.h * scale;
		out << "  <rect x=\"" << x << "\" y=\"" << y << "\" width=\"" << w << "\" height=\"" << h
		    << "\" fill=\"#87CEFA\" stroke=\"black\" stroke-width=\"2\" />\n";
		// label
		out << "  <text x=\"" << (x + w / 2) << "\" y=\"" << (y + h / 2)
		    << "\" dominant-baseline=\"middle\" text-anchor=\"middle\" font-size=\""
		    << std::max(10, scale) << "\" font-family=\"sans-serif\" fill=\"#000000\">" << p.id << "</text>\n";
	}
	out << "</svg>\n";
}

void exportSolutionToSvg(const std::vector<Bin>& bins, const std::string& outputDir, int scale) {
	fs::path outDir(outputDir);
	std::error_code ec;
	fs::create_directories(outDir, ec);
	for (const auto& b : bins) {
		if (b.packages.empty()) continue;
		fs::path file = outDir / ("bin_" + std::to_string(b.id) + ".svg");
		writeSvgBin(b, file, scale);
	}
}


