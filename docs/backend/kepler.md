# Kepler Vulkan decode tuning (v1 candidate)

This fork contains Vulkan matvec tuning for NVK on GK110, GK110B and GK210. Tesla K20c is the tested device. Other cards in these families are trial targets, not validated devices. NVIDIA proprietary Vulkan and other GPU generations keep the upstream paths.

The source starts at llama.cpp `4413a192410fad1f054e9216114087a3e01527a0`. Testing uses Mesa NVK `76b9372c4e5d186f57d22d3a4f95dd1c5e04ece2`, Linux nouveau, and SmolLM2-1.7B-Instruct. Stock Mesa is sufficient. An optional, separate Mesa NAK patch removes redundant low-byte PRMT before I2F on SM35; that patch is not included in this fork.

## Build and select a GPU

Use a fresh build directory. The usual Vulkan dependencies and a glslc compiler are required; see [Vulkan build instructions](../build.md#vulkan).

```sh
cmake -S . -B build-kepler --log-level=WARNING \
  -DCMAKE_BUILD_TYPE=Release -DCMAKE_RULE_MESSAGES=OFF \
  -DGGML_VULKAN=ON -DGGML_CUDA=OFF -DGGML_BLAS=OFF \
  -DLLAMA_BUILD_TESTS=ON -DLLAMA_BUILD_APP=OFF \
  -DLLAMA_BUILD_SERVER=OFF -DLLAMA_BUILD_MTMD=OFF
cmake --build build-kepler --parallel --target llama-bench test-backend-ops

# Use your NVK ICD. Do not replace the system Mesa installation.
export VK_DRIVER_FILES=/path/to/nouveau_icd.x86_64.json
build-kepler/bin/llama-bench --list-devices
```

Choose the Vulkan device reported for the intended GPU. Do not assume Vulkan0 or the first NVIDIA GPU, especially on a multi-GPU K80 or a host with other NVIDIA cards. Driver assignment and GPU clocks are managed outside this fork; the build and smoke script do not change them.

## What is selected

| Input | Tuning |
|---|---|
| Q8_0 | WG128; two rows; precise four short dot chains |
| Q8_0, K >= 4096 and M < K | Aligned two-u32 plus final-u16 weight reads |
| F16 | WG64; protected packed32 weight reads |
| F16, nonsquare and shaderInt64 available | Protected 64-bit weight and activation reads |
| F16, K >= 1024 and M > K | Four rows; otherwise two |
| Q4_0 / Q4_K, K >= 1024 | WG64 for K <= 4096, WG128 for longer K |
| Q4_0, 1024 <= K <= 4096 and M > K | Four rows; contracting shapes retain two |

K is the input width, M the output width. Dedicated shaders require a contiguous single vector and single batch, F32 activation, weight alignment of 8 bytes and activation alignment of 16 bytes. F16 requires K divisible by 4; Q8_0 and Q4_0 require valid complete quant blocks. Partial output rows are supported. Views must also satisfy the upstream Vulkan descriptor-offset alignment; arbitrary byte offsets are unsupported in this source snapshot. Descriptor sizes and element indices must fit the existing 32-bit path. Unsupported layouts, sizes, N > 1 and batches use the existing paths; a WG hint may still apply to an upstream matvec path.

These shape rules are initial performance heuristics, not a promise that every model or shape gets faster. The original exact SmolLM2 FFN guards have been replaced with these layout and size conditions. Small M, tails, other model layouts and different memory/clock configurations may change the result. MoE MUL_MAT_ID has no dedicated new tuning.

## Compare and diagnose

- Default: automatic tuning only on NVK device names identifying GK110, GK110B or GK210. A custom NVK device-name override may prevent identification.
- `GGML_VK_KEPLER=0`: disable the tuning, including WG overrides, for an upstream-style comparison in the same binary.
- `GGML_VK_KEPLER_WIDE=0`: retain other tuning but disable Q8 wide reads.
- `GGML_VK_KEPLER_TRACE=1`: print selected matvec pipelines. Use only for diagnosis; leave unset during timing.

An enable value cannot force tuning onto a different GPU family. The device log reports whether tuning is enabled; a model can still use fallback pipelines because of its tensor layouts or operations.

Run the small CPU reference comparison before benchmarking:

```sh
build-kepler/bin/test-backend-ops test -b VulkanN -o MUL_MAT \
  --test-file docs/backend/kepler-smoke.ops
# More checks for offset views, partial rows and fused bias:
build-kepler/bin/test-backend-ops test -b VulkanN -o KEPLER_MATVEC_CHECK
```

For a short reproducible comparison, use the same GGUF, GPU and Mesa on both sides:

```sh
scripts/kepler-smoke.sh build-kepler VulkanN /path/to/model.gguf /path/to/fresh-record-dir
```

The script checks small operators against CPU and records a logger-free tg128 ABBA comparison (OFF, ON, ON, OFF), command lines, stdout/stderr, exit status, commit, hashes and kernel version. It does not download or modify models. `-r 1 -p 0 -n 128 -t 8 -b 32 -ub 32 -ctk f16 -ctv f16 -fa off -ngl 99` is the comparison configuration. If VRAM cannot hold the model, record the resulting offload and treat that as a different workload.

Share the record directory plus GPU model/PCI ID, Mesa version and ICD, PCIe link, available clocks, temperature and any throttling/ECC information. Use `unavailable` for readings you cannot obtain. Report correctness failures before interpreting speed. A small or inconsistent change is a useful result; one fast run alone does not establish an improvement.

## K20c validation on 2026-10-14

Release build, stock Mesa commit above, SmolLM2-1.7B-Instruct, cache disabled, no perf/trace logger. Each format uses its own unchanged GGUF. These are tg128 ABBA medians with two independent processes per side, complete 25-layer offload, context256, one sequence, F16 KV and flash attention off. OFF and ON use the same fork binary; OFF restores upstream-style dispatch. This is not a comparison with a separately rebuilt upstream binary.

| Format | OFF tokens/s | ON tokens/s | Throughput change |
|---|---:|---:|---:|
| Q8_0 | 11.646 | 21.164 | +81.73% |
| F16 | 8.426 | 18.467 | +119.16% |
| Q4_0 | 16.947 | 24.649 | +45.45% |
| Q4_K_M | 14.716 | 20.127 | +36.77% |

The ON samples were above the OFF range for all four formats. The result is specific to this GPU, Mesa, models and decode configuration. Full model profiling was not repeated, so this table does not assign gains to individual operations or claim DRAM bandwidth. A separate comparison against the integrated exact-shape work branch found additional gains of Q8_0 +6.17%, F16 +3.05%, Q4_0 +12.51% and Q4_K_M +9.32% from wider dispatch; those percentages must not be added to the table above.

CPU reference checks passed: expanded operators39/39, original operators29/29, aligned offset/bias40/40, OFF39/39 and wide-disabled39/39. Three strided quant cases were unsupported by the existing backend and skipped. Sixteen representative native shapes (K1024/1536/4096/12288) reduced synchronized host time by about18-55% versus exact-shape dispatch. GPRs stayed at32 for F16,40 for Q8/Q4_0 and48 for Q4_K with allocator spill/fill0. These static resource counts do not establish the bottleneck.

An initial test-file fixture left the tail of a strided F16 backing uninitialized, yielding NaN on both CPU and GPU; its backing initialization was corrected. A deliberately descriptor-misaligned view aborted with tuning ON and OFF; that unsupported fixture was removed and the alignment restriction is stated above. The valid final tests and public smoke script passed. GK110B/GK210 hardware, long contexts and MoE remain unmeasured.

[Machine-readable validation summary](kepler-validation.json) includes samples, medians, representative-shape host timings and the known initial failures. Full raw logs, complete patches and binary/driver hashes are kept in the companion preparation record.

## Scope and provenance

The fork combines the measured Q8 dot4/rows2/down-wide, F16 packed/64-bit loads/up rows4, and Q4_0 up rows4 with Q4_K WG hints. Q4_0 dot4, Q4_K smin4, Q4_K rows4 and load hoist are not enabled: model gains were absent, weak or negative. F16 and Q8 speedups from separate experiments must not be summed into a total claim.

CPU reference, offset/bias checks, representative shapes and SmolLM2 decode are checked on K20c. GK110B/GK210 hardware, arbitrary models, long contexts and MoE need community validation. The companion preparation record contains exact source commits, build options, raw failures and comparisons. Public fork preparation does not imply an upstream contribution or release tag.

AI assistance: Codex helped implement, review and measure these local changes. Commits disclose this with `Assisted-by: Codex`.
