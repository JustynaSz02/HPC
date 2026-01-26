#include "TabuSearch.h"
#include <cuda_runtime.h>
#include <device_launch_parameters.h>
#include <algorithm>
#include <random>
#include <iostream>
#include <vector>
#include <cmath>

// GPU Kernel: Fill-Aware Scoring
// Rewards placing items in bins that become "more full" (compacting).
// Penalizes placing items in empty bins (to prevent using 10 bins).
__global__ void evaluateAllMovesKernel(int* d_bw, int* d_bh, int* d_pw, int* d_ph,
    int* d_current_fill, float* d_scores,
    int num_bins, int num_pkgs) {
    int pkg_idx = blockIdx.x * blockDim.x + threadIdx.x;
    int bin_idx = blockIdx.y * blockDim.y + threadIdx.y;

    if (pkg_idx < num_pkgs && bin_idx < num_bins) {
        // Dimension Fit Check
        if (d_pw[pkg_idx] <= d_bw[bin_idx] && d_ph[pkg_idx] <= d_bh[bin_idx]) {

            float bin_cap = (float)(d_bw[bin_idx] * d_bh[bin_idx]);
            float pkg_area = (float)(d_pw[pkg_idx] * d_ph[pkg_idx]);
            float current_area = (float)d_current_fill[bin_idx];

            float new_fill_ratio = (current_area + pkg_area) / bin_cap;

            // SCORING LOGIC:
            // 1. If bin is empty, huge penalty (don't open new bins if possible).
            // 2. Power function (cubed) rewards "almost full" bins exponentially more than "half full" bins.

            if (current_area == 0.0f) {
                d_scores[pkg_idx * num_bins + bin_idx] = 0.001f; // Penalty for opening new bin
            }
            else {
                d_scores[pkg_idx * num_bins + bin_idx] = powf(new_fill_ratio, 3.0f);
            }

        }
        else {
            d_scores[pkg_idx * num_bins + bin_idx] = -1.0f; // Invalid move
        }
    }
}

int evaluateSolution(const std::vector<Bin>& bins) {
    int used = 0;
    for (const auto& b : bins) { if (!b.packages.empty()) ++used; }
    return used;
}

double evaluateSolutionTie(const std::vector<Bin>& bins) {
    double score = 0.0;
    for (const auto& b : bins) { score += b.evaluateBin(); }
    return score;
}

std::vector<Bin> generateInitialSolution(const std::vector<Bin>& inputBins, const std::vector<Package>& packages, unsigned int seed) {
    std::vector<Bin> bins = inputBins;
    std::vector<Package> pack = packages;

    // FFD Strategy: Strict Sort Descending
    std::sort(pack.begin(), pack.end(), [](const Package& a, const Package& b) {
        return a.area > b.area;
        });

    // Randomize only if seed > 0 (for restarts)
    if (seed > 0) {
        auto rng = std::default_random_engine{ seed };
        // Only shuffle the smaller 50% of items to keep big anchors in place
        int start_idx = pack.size() / 2;
        std::shuffle(pack.begin() + start_idx, pack.end(), rng);
    }

    for (const auto& p : pack) {
        for (auto& b : bins) {
            if (b.placePackage(p)) break;
        }
    }
    return bins;
}

std::vector<Bin> tabuSearch(const std::vector<Bin>& bins, const std::vector<Package>& packages,
    const std::vector<Bin>& initial, int iterations, int tabuSize) {
    std::vector<Bin> current = initial;
    std::vector<Bin> best_found = initial;

    int num_bins = (int)bins.size();
    int num_pkgs = (int)packages.size();

    // Tabu List: Stores which iteration a package is allowed to move back to a specific bin
    // flattened: tabu_list[package_id * num_bins + bin_id] = iteration_expiry
    std::vector<int> tabu_list(num_bins * num_pkgs, 0);

    // Device Pointers
    int* d_bw, * d_bh, * d_pw, * d_ph, * d_current_fill;
    float* d_scores;

    // Alloc
    cudaMalloc(&d_bw, num_bins * sizeof(int));
    cudaMalloc(&d_bh, num_bins * sizeof(int));
    cudaMalloc(&d_pw, num_pkgs * sizeof(int));
    cudaMalloc(&d_ph, num_pkgs * sizeof(int));
    cudaMalloc(&d_current_fill, num_bins * sizeof(int));
    cudaMalloc(&d_scores, num_bins * num_pkgs * sizeof(float));

    // Host Data Prep
    std::vector<int> h_bw(num_bins), h_bh(num_bins), h_pw(num_pkgs), h_ph(num_pkgs);
    for (int i = 0; i < num_bins; ++i) { h_bw[i] = bins[i].w; h_bh[i] = bins[i].h; }
    for (int i = 0; i < num_pkgs; ++i) { h_pw[i] = packages[i].w; h_ph[i] = packages[i].h; }

    cudaMemcpy(d_bw, h_bw.data(), num_bins * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_bh, h_bh.data(), num_bins * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_pw, h_pw.data(), num_pkgs * sizeof(int), cudaMemcpyHostToDevice);
    cudaMemcpy(d_ph, h_ph.data(), num_pkgs * sizeof(int), cudaMemcpyHostToDevice);

    std::vector<float> h_scores(num_bins * num_pkgs);
    std::vector<int> h_fill(num_bins);

    dim3 threads(16, 16);
    dim3 blocks((num_pkgs + 15) / 16, (num_bins + 15) / 16);

    for (int it = 0; it < iterations; it++) {

        // 1. Calculate current fill levels on host
        for (int b = 0; b < num_bins; ++b) h_fill[b] = current[b].currentArea();
        cudaMemcpy(d_current_fill, h_fill.data(), num_bins * sizeof(int), cudaMemcpyHostToDevice);

        // 2. Launch Kernel
        evaluateAllMovesKernel<<<blocks, threads >>>(d_bw, d_bh, d_pw, d_ph, d_current_fill, d_scores, num_bins, num_pkgs);
        cudaMemcpy(h_scores.data(), d_scores, num_bins * num_pkgs * sizeof(float), cudaMemcpyDeviceToHost);

        // 3. Find best VALID, NON-TABU move
        float best_move_score = -1.0f;
        int best_p = -1;
        int target_bin = -1;
        int source_bin = -1;

        // Optimization: Identify "Weakest" Bin (lowest fill > 0) to try and empty it
        int weakest_bin = -1;
        int min_fill = 9999999;
        for (int b = 0; b < num_bins; ++b) {
            if (h_fill[b] > 0 && h_fill[b] < min_fill) {
                min_fill = h_fill[b];
                weakest_bin = b;
            }
        }

        // Search for move
        for (int p_idx = 0; p_idx < num_pkgs; ++p_idx) {
            // Find where this package currently is
            int current_bin_idx = -1;
            for (int b = 0; b < num_bins; ++b) {
                // This is slow on CPU, but necessary without full GPU state tracking
                for (const auto& pkg : current[b].packages) {
                    if (pkg.id == packages[p_idx].id) { current_bin_idx = b; break; }
                }
                if (current_bin_idx != -1) break;
            }
            if (current_bin_idx == -1) continue; // Should not happen

            // If we are in "Empty Weakest" mode, boost moves FROM the weakest bin
            float priority_mult = (current_bin_idx == weakest_bin) ? 1.5f : 1.0f;

            for (int b_idx = 0; b_idx < num_bins; ++b_idx) {
                if (b_idx == current_bin_idx) continue; // Don't move to same bin

                float score = h_scores[p_idx * num_bins + b_idx];
                if (score < 0) continue; // Invalid fit dimensions

                score *= priority_mult;

                // Tabu Check
                if (it < tabu_list[p_idx * num_bins + b_idx]) {
                    // Aspiration Criteria: Allow tabu move if it improves global best
                    // (Simplified here: just skip)
                    continue;
                }

                if (score > best_move_score) {
                    best_move_score = score;
                    best_p = packages[p_idx].id;
                    target_bin = b_idx;
                    source_bin = current_bin_idx;
                }
            }
        }

        // 4. Apply Move
        if (best_p != -1) {
            std::vector<Bin> next_sol = current;
            bool removed = next_sol[source_bin].removePackageById(best_p);

            // Re-find package object because it was removed
            Package p_obj;
            for (const auto& p : packages) if (p.id == best_p) { p_obj = p; break; }

            // Validate geometric fit on CPU (Critical)
            if (removed && next_sol[target_bin].placePackage(p_obj)) {
                current = next_sol;

                // Set Tabu: Don't allow moving back to source_bin for tabuSize iters
                tabu_list[best_p * num_bins + source_bin] = it + tabuSize;

                // Update Best
                if (evaluateSolution(current) < evaluateSolution(best_found) ||
                    (evaluateSolution(current) == evaluateSolution(best_found) && evaluateSolutionTie(current) > evaluateSolutionTie(best_found))) {
                    best_found = current;
                }
            }
        }
    }

    cudaFree(d_bw); cudaFree(d_bh); cudaFree(d_pw); cudaFree(d_ph);
    cudaFree(d_current_fill); cudaFree(d_scores);

    return best_found;
}