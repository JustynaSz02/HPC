#pragma once

// Struktura reprezentująca ruch w Tabu Search
// Move = "przenieś paczkę pkgId z bina fromBin do bina toBin"
struct Move {
	int pkgId;    // ID paczki do przeniesienia
	int fromBin;  // Indeks bina źródłowego
	int toBin;    // Indeks bina docelowego

	Move() : pkgId(-1), fromBin(-1), toBin(-1) {}
	Move(int pkg, int from, int to) : pkgId(pkg), fromBin(from), toBin(to) {}

	// Operator porównania dla tabu listy (std::set lub std::find)
	bool operator==(const Move& other) const {
		return pkgId == other.pkgId && fromBin == other.fromBin && toBin == other.toBin;
	}

	bool operator<(const Move& other) const {
		if (pkgId != other.pkgId) return pkgId < other.pkgId;
		if (fromBin != other.fromBin) return fromBin < other.fromBin;
		return toBin < other.toBin;
	}
};

