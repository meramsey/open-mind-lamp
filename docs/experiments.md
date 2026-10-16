# Experiments and reproducibility

The lamp is an exploratory random-event-generator platform inspired by historical
mind-lamp experiments. It does not assume that intention can influence hardware
randomness. An unusual statistic is not an established mechanism or causal result.

## Session workflow

1. Before viewing results, write a protocol: hypothesis, device/firmware, condition
   allocation, fixed sample count, primary statistic/direction, number of sessions,
   timing, exclusions, multiplicity adjustment and stopping rule. Archive it externally.
2. Configure lamp settings before recording. Keep brightness, RF conditions,
   supply, enclosure, sample rate and environment matched between conditions.
3. Choose `baseline`, `control` or `intention`; predeclare more one-bits (`high`)
   or fewer (`low`). Enter a protocol/preregistration identifier (1–160 characters).
4. Set 32–4096 words. At default 20 words/s, 1200 words is approximately one minute,
   4096 words approximately 205 seconds. Every word is 32 measured bits.
5. Start. The browser may close; firmware stops on the planned word count.
   Settings are locked. A new session restarts the walk from configured starting hue.
6. Download the raw trial. Run the offline audit and save its SHA-256 alongside
   the unmodified file. Copy it to independent storage before clearing the lamp.
7. Collect all preregistered sessions including negative/null results. Do not
   discard “bad looking” runs. Record technical failures and early stops separately.

Condition is explicitly chosen and visible; this UI does not implement blinding
or randomized allocation. For a controlled study, generate and archive a balanced
randomized schedule independently, conceal allocation as appropriate, and keep an
external experiment log. Raw source selection is fixed to hardware in production.

## What is recorded

One header includes schema, unique ID, firmware/IDF versions, source and entropy
configuration, algorithm version, all lamp settings, initial hue, intended word
count, condition/direction, protocol string, device monotonic start time and
client-supplied UTC. Client UTC is unverified; no NTP/server is required.

Each raw-word record includes sequential index, relative microsecond time,
unsigned word, scheduler lateness and continuous health status. Footer records
count, one-bit count and reason (`completed`, `user_stopped`, `health_failure`,
`entropy_unavailable`, `storage_error`, `led_failure`, `network_recovery`). A missing
footer means interrupted; it is never silently inferred as completed.

Browser color is illustrative, not calibrated colorimetry. Raw words and bit
counts are the primary measurements, not screenshots, hue alone, or visual impressions.
Hue wraps and smoothing obscure large cumulative motion; analyze raw data.

## Important analysis choices

Use fixed-N, preregistered primary endpoints for confirmatory inference. Repeatedly
checking a live z-score and stopping at a favorable excursion invalidates an
ordinary fixed-N p-value. The dashboard deliberately reports diagnostic fractions
without a live significance claim. Early/failure-stopped trials receive descriptive
statistics but are marked ineligible for fixed-N interpretation by the analyzer.

Basic health monitors themselves impose a failure stopping rule. Keep every such
failure and specify handling before data collection; do not quietly resample until
the RNG passes. Very broad limits make fair-source failures unlikely but do not
remove the selection issue in principle.

Control comparisons must consider session/time clustering, temperature, radio
activity, device drift, optional stopping, carryover and multiple comparisons.
The included pooled-bit comparison is **exploratory**, not a substitute for a
randomized session-level analysis. Independent replication should use new runs,
published code/configuration and the same protocol. Public hashes detect later
changes only if anchored externally; the device journal is not a signed,
tamper-proof laboratory record.
