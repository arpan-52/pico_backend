// pico_image/src/calib.cpp
// Off-source bandpass + baseline construction (svfits make_bpass).
// Strategy mirrors svfits:
//   - For records that do NOT overlap the DM track, accumulate per-(baseline,
//     channel) sum/mean of complex visibilities → off_src[b][c]
//   - Per-baseline mean amplitude over channels → normalize to 1 → abp[b][c]
//   - Median absolute deviation flagger for residual RFI (clip)

#include "pico/calib.hpp"
#include "pico/half.hpp"
#include "pico/dm.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <vector>

namespace pico {

namespace {

inline void decode_vis(const float* slot, Vis& v) {
    // svfits packs (re, im) as two uint16 halfs inside the float bit pattern.
    std::uint32_t bits;
    std::memcpy(&bits, slot, sizeof(bits));
    const std::uint16_t re_h = static_cast<std::uint16_t>(bits & 0xFFFFu);
    const std::uint16_t im_h = static_cast<std::uint16_t>((bits >> 16) & 0xFFFFu);
    v.r  = half_to_float(re_h);
    v.i  = half_to_float(im_h);
    v.wt = 1.0f;
}

} // namespace

int make_bandpass(const Config& cfg, const RawSet& rs, const AntSamp& as,
                  const void* rbuf, int idx, int slice, Bandpass& bp) {
    const int nch  = rs.channels;
    const int nbase = as.nbase;
    bp.n_base = nbase;
    bp.n_chan = nch;
    bp.abp.assign(nbase, std::vector<float>(nch, 1.0f));
    bp.off_src.assign(nbase, std::vector<Complex>(nch, Complex{0,0}));
    bp.file_idx = idx;
    bp.slice    = slice;

    // Iterate records in this slice. For each record, compute its time and
    // check whether the burst lies in this record's [cs,ce) range. If not,
    // accumulate into off_src.
    const auto* base = static_cast<const float*>(rbuf);
    const double dt_rec = rs.t_slice / rs.rec_per_slice;
    const double t_slice_start = idx * rs.t_slice + slice * rs.slice_interval;

    // Per-record burst channel range, so we know which records are off-source
    // for a given channel.
    std::vector<int> rec_cs(rs.rec_per_slice), rec_ce(rs.rec_per_slice);
    for (int r = 0; r < rs.rec_per_slice; ++r) {
        const double trec = t_slice_start + r * dt_rec;
        burst_chans_for_record(cfg, trec, dt_rec, nch, &rec_cs[r], &rec_ce[r]);
    }

    // off_src per (baseline, channel) = arithmetic MEAN of the off-burst
    // records (svfits make_bpass, svsubs.c:1494-1499). The MEAN — not the
    // median — is what removes the steady coherent component (continuum /
    // persistent correlated RFI) common to on- and off-burst samples; svfits
    // subtracts exactly this and the reference image comes out blank when no
    // burst is present. abp[b][c] = MEAN amplitude <sqrt(re^2+im^2)> over the
    // same records (svsubs.c:1495,1500). NOTE: abp must be <|V|> (mean of the
    // amplitudes), NOT |<V>| (= |off_src|): for noise |<V>| -> 0 and dividing
    // by it explodes the visibilities. n==0 channels are flagged (-1) and
    // filled by neighbour-carry below (svsubs.c:1502-1503,1529-1530).
    #pragma omp parallel for num_threads(cfg.num_threads)
    for (int b = 0; b < nbase; ++b) {
        // Skip autocorrelations — never imaged, only pollute off_src.
        if (as.baseline[b].s0.ant_id == as.baseline[b].s1.ant_id) continue;
        for (int c = 0; c < nch; ++c) {
            double sre = 0, sim = 0, samp = 0; long n = 0;
            for (int r = 0; r < rs.rec_per_slice; ++r) {
                if (c >= rec_cs[r] && c < rec_ce[r]) continue;  // burst region
                const float* slot = base +
                    (static_cast<std::ptrdiff_t>(r) * (nch * nbase) +
                     static_cast<std::ptrdiff_t>(b) * nch + c);
                Vis v; decode_vis(slot, v);
                // Guard non-finite decoded halfs (svfits svsubs.c:1493).
                if (!std::isfinite(v.r) || !std::isfinite(v.i)) continue;
                sre += v.r; sim += v.i; samp += std::hypot(v.r, v.i); ++n;
            }
            if (n > 0) {
                bp.off_src[b][c] = Complex(float(sre / n), float(sim / n));
                bp.abp[b][c]     = float(samp / n);
            } else {
                bp.off_src[b][c] = Complex{0, 0};
                bp.abp[b][c]     = -1.0f;  // flag for neighbour-carry below
            }
        }
    }
    // off_src bias diagnostic: this complex constant is subtracted from EVERY
    // burst sample, so a large coherent +ve real here drives a central source.
    {
        double sre = 0, sim = 0, samp = 0, amax = 0; std::size_t n = 0;
        for (int b = 0; b < nbase; ++b)
            for (int c = 0; c < nch; ++c) {
                const Complex o = bp.off_src[b][c];
                const double a = std::abs(o);
                sre += o.real(); sim += o.imag(); samp += a;
                if (a > amax) amax = a; ++n;
            }
        if (n > 0)
            std::fprintf(stderr,
                "make_bandpass[file=%d slice=%d]: off_src mean re=%.6g im=%.6g "
                "|.|=%.6g max|.|=%.6g over %zu (base,chan)\n",
                idx, slice, sre / n, sim / n, samp / n, amax, n);
    }
    // Amplitude bandpass normalisation (svfits svsubs.c:1521-1536): carry the
    // first valid value backward, forward-fill flagged channels, then divide
    // by the per-baseline median across channels so the bandpass sits near 1.
    for (int b = 0; b < nbase; ++b) {
        if (as.baseline[b].s0.ant_id == as.baseline[b].s1.ant_id) continue;
        auto& abp = bp.abp[b];
        if (abp[0] < 0.0f)
            for (int c = 1; c < nch; ++c) if (abp[c] > 0.0f) { abp[0] = abp[c]; break; }
        if (abp[0] < 0.0f) {                       // all channels flagged
            for (int c = 0; c < nch; ++c) abp[c] = 1.0f;
            continue;
        }
        for (int c = 1; c < nch; ++c) if (abp[c] < 0.0f) abp[c] = abp[c - 1];
        std::vector<float> tmp(abp.begin(), abp.end());
        std::nth_element(tmp.begin(), tmp.begin() + nch / 2, tmp.end());
        const float medc = tmp[nch / 2];
        if (medc > 0.0f) for (int c = 0; c < nch; ++c) abp[c] /= medc;
    }
    return 0;
}

int clip_record(std::vector<Vis>& ch, float mad_thresh) {
    if (ch.empty()) return 0;
    // Compute amplitude vector
    std::vector<float> amp; amp.reserve(ch.size());
    for (const auto& v : ch) if (v.wt > 0) amp.push_back(std::hypot(v.r, v.i));
    if (amp.size() < 4) return 0;
    std::nth_element(amp.begin(), amp.begin() + amp.size()/2, amp.end());
    const float med = amp[amp.size() / 2];
    // MAD
    std::vector<float> dev; dev.reserve(amp.size());
    for (float a : amp) dev.push_back(std::fabs(a - med));
    std::nth_element(dev.begin(), dev.begin() + dev.size()/2, dev.end());
    const float mad = dev[dev.size() / 2];
    if (mad <= 0) return 0;
    const float thr = med + mad_thresh * 1.4826f * mad;
    int flagged = 0;
    for (auto& v : ch) {
        if (std::hypot(v.r, v.i) > thr) { v.wt = -1.0f; ++flagged; }
    }
    return flagged;
}

} // namespace pico
