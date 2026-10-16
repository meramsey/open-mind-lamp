# Random walk and statistics

## Measurement and random walk

For each unconditioned unsigned 32-bit hardware word `w_i`, define

```
k_i = popcount(w_i)
d_i = 2*k_i - 32
S_j = sum(d_i over accumulation interval j)
H_(j+1) = wrap_360(H_j + sensitivity * S_j)
```

Under independent fair bits, `E[d_i]=0`, `Var(d_i)=32`. For `m` words in an
interval the unwrapped hue increment has expectation zero and variance
`32*m*sensitivity²`. There is no modulo mapping of random integers to a bounded
range, and thus no modulo bias. Wrapping hue is a deterministic geometric
operation, not biased selection of random events.

Defaults (20 words/s, 1 s, 0.2°/net bit) give increment standard deviation
`sqrt(640)*0.2 ≈ 5.06°` per second-long interval. Changing sample rate or sensitivity
intentionally changes diffusion. Accumulation changes when moves occur. A frame
rate change does not acquire extra random words or multiply increments.

Target hue and rendered hue are separate. Each animation update follows the
shortest angular difference `delta ∈ [-180,180)` with

`hue += delta * (1 - exp(-dt / smoothing_time))`.

At zero smoothing the target is used directly. Animation is independent of
bit-count statistics; hue-only observations lose information through wrapping.
Saving settings explicitly restarts the walk. For an exact raw-sum replay,
`sum(d_i)=2*ones-32*words`; grouping nominal intervals by timestamps approximates
display targets. Actual accumulation/render deadlines have scheduling jitter and
are not recorded as frame events, so the export is not a pixel-perfect animation
replay. Scientific bit-count measurements are exactly reconstructible.

HSV chromatic components receive a single 2.2 gamma curve; brightness is then applied
as a physical PWM multiplier, followed by optional per-channel gains and one final
clamp/rounding step. Saturation/brightness/gains/visual modes never feed back
into RNG conditioning. At most 32/255 reaches any LED channel.

## Independent analysis

For `n=32*word_count` bits and `K` one-bits:

- expected `K = n/2`, standard deviation `sqrt(n/4)`;
- `z = (K-n/2)/sqrt(n/4)`;
- exact-distribution two-sided fair-binomial p-value:
  `min(1, 2*P[Bin(n,.5) <= min(K,n-K)])`;
- predeclared upper (`high`) or lower (`low`) one-sided tail;
- 95% Wilson interval for the one-bit fraction.

Binomial tails are evaluated with a log-PMF and recurrence summation in double
precision, not a Gaussian replacement. Extremely small tails can underflow to
zero; that means below floating-point resolution, not mathematical impossibility.
Tests compare the implementation with exact combinatorial sums for small n.
Intervals and p-values require their model assumptions; correlated bits reduce
effective sample size and can invalidate nominal significance.

The analyzer checks sequence, monotonic timestamps, word bounds, boolean health,
header/algorithm/condition, planned count and footer accounting. Missing footers
are labeled interrupted. A malformed final line is rejected unless
`--recover-truncated` is explicitly used; salvaged data is always marked ineligible.
Duplicate files/session IDs are rejected in a comparison. SHA-256 covers the
original byte stream, including any incomplete tail.

```
python3 analysis/src/analyze.py trial.ndjson
python3 analysis/src/analyze.py trial.ndjson --csv words.csv --raw words.bin
python3 analysis/src/analyze.py control.ndjson intention.ndjson --compare
```

CSV includes word/timing/health; raw binary is four-byte little-endian words.
Analyze bits LSB-first when order matters. These formats support independent
tools such as PractRand or dieharder, but a single short session has limited
test power. Passing any test suite never proves unpredictability or physical entropy.

`--compare` pools eligible control/intention bits and reports the two-proportion
normal z/p approximation. It excludes baseline and ineligible sessions explicitly
in group counts. It does not correct multiplicity, pairing, protocol heterogeneity
or session clustering; do not treat it as confirmatory evidence.
