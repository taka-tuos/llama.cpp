> 保存時点の作業記録です。「次の作業」は当時の記載で、2026-10-09に実験を終了しました。未同梱ファイルへのリンクは元アーカイブ参照へ変更し、実験rootの絶対pathを置換しました。

# S8 実施完了記録

単一HTML：概要・方式・絞り込み・全文（元アーカイブ参照: `s8-report.html`）（外部依存なし）

2026-10-02（日本時間）。ユーザー指定のS8を実装し、固定44条件の技術検証・同負荷性能測定・132生成・本文保存を完了。D5完了。人間RP評価・品質改善の認定は保留。S9は今回未着手。

固定条件/環境（元アーカイブ参照: `metadata.md`） / [技術検証](s8-technical.md) / 比較と性能（元アーカイブ参照: `comparison.md`） / smoke（元アーカイブ参照: `smoke.md`） / 会話（元アーカイブ参照: `conversation.md`） / 場面（元アーカイブ参照: `scene.md`） / 測定JSON（元アーカイブ参照: `final-results.json`）

## 実装

Mは正整数/既定1、線形αはu=s+α(e−s)、各extra直後s=u+(z−u)/M、最後だけβ混合。prelude-inputはID2/linear-v1、旧S7はprelude-input-legacyのID1/ratio-v1へ保持。scope none/decode、既定decode、prefill/bothは未対応エラー。N0/β0/none/promptにextraなし。α0/α1/M1/β1は直結。通常pass・層内部・PLE・RoPE/mask・base KV・tail mappingを維持。α1はbase23/extra/z→sを明示graph登録し、途中の演算省略も防ぐ。

既存12変更ファイル・pacman.txt・過去run/build/GGUFを保持。HEAD 6a1a922d269908a29cbd4b49c27e6a8e7fd10fae、コミット/pushなし。現在source.patchとbefore.patch、source/model/build/script/dump hashを保存。実cwd <experiment-root>、指定workspaceの実体。

## コマンドと検証

| 作業 | コマンド / 終了コード / 一次記録 |
|---|---|
| 専用build | cmake -S llama.cpp -B runs/2026-10-02-s8/build -DCMAKE_BUILD_TYPE=Release -DGGML_CUDA=ON -DCMAKE_CUDA_ARCHITECTURES=75; cmake --build ... --parallel 80。configure/buildと修正後再buildすべて0。configure.log/build.log/rebuild-alpha1*.log |
| CLI/reuse/API | cli-validation.py 0・73ケース。graph-reuse 0。restrictions 0・不正値/sequence/非E4B拒否。speculativeは期待拒否4 |
| 技術 | technical.py 0、44条件callback有無・旧legacy19・disabled6・output5。verify-technical.py 0。19旧回帰と新α0/M1の旧S6全logits/state一致。N0/1/2/4の省略defaultはdefault-regression.py 0で旧S6 binary一致 |
| 保存値照合 | diagnostics.py 0、574pass地点の独立float32再計算、最大入力/更新誤差0。数学的対照10件。representative-top-logits.jsonに7地点＋prefillのtop/EOG/think終了順位 |
| 性能 | benchmark.py 0、44条件 × warm-up1＋3回。benchmark-commands.json/各bench.log/.exit/.vram.json |
| 自由生成 | generation.py 0、132/132のprocess exit0。generation-commands.json/各generation.log/.exit。形式エラーはprocess exit0とは別に区分 |
| 解析 | analyze.py 0、132 raw/IDs/think/回答/settings、prompt/sampler照合、旧S6共通9条件＋N0の30生成ID回帰。analysis-commands.json |

α1初回のbase23評価順と2回目の中間s省略を検出して修正。失敗/.exit/dumpと修正前成功runを保存し、許容差を緩めていない。最終α1だけ6条件を再実行し、他経路の保存値は再照合。失敗記録（元アーカイブ参照: `failure-alpha1.md`）。解析script初回のlabel重複によるPythonエラーはattempt1-partial-analysis.logへ保存して修正。推論結果への変更なし。

## 実測

RTX 2060 SUPER 8 GiB / driver615.71.09 / CUDA13.4.92 / GCC16.2.1。Release/native/architecture75、logical CPU80・parallel80、推論threads8。GGUF SHA-256 dff0ffba4c90b4082d70214d53ce9504a28d4d8d998276dcb3b8881a656c742a。ログは43/43 layers offload、CUDA model2883.51 MiBとCPU_Mapped2208 MiB、KV f16、FA on、context4096/batch512。CUDA推論をCPU成功と混同しない。

| N (α0/M1/β1) | decode tok/s 中央値 (範囲) |
|---:|---|
| 0 | 83.53 (83.29–83.54) |
| 1 | 73.82 (73.80–73.88) |
| 2 | 67.65 (67.60–67.66) |
| 3 | 62.25 (62.21–62.33) |

全44条件のexecute sampled peakは3796–3798 MiB、KV buffer容量は全条件224.00 MiB。同履歴でstate/KVはN0と一致。開始前idle1MiB、他GPU processなし（environment.log）。pollはquery後50ms待機、実間隔・GPU利用率をvram.jsonへ保存。短いpeakを見逃しうる。各process一覧の実行中連続取得はしていない。load/context/executeを分け、executeの実処理寿命内を計測。

自由生成wall-clock合計1888.8秒（31.48分）、性能44条件wall-clock合計469.8秒（7.83分）。model load/prefill/monitor待機を含む各process時間で、decode tok/sと区別する。build/技術検証/解析は別。

## 出力所見と制約

最終回答到達 117/132、EOS 115/132、形式エラー 0/132、実stop settings欠落 0/132。有効出力のthinking未終了 15件、100token以上のperiod1–10連続反復はthink 13件・回答 2件。形式エラーはthink/回答/反復unknown、raw周期反復を別保存。検出なしを全反復なしと認定しない。

3 prompt・単一greedy条件の固定探索。回答/EOS/速度は実測だが日本語RP改善の証明ではない。scene/conversation本文を保存して人間所見待ち。S7/D4未完をS8で遡って解消済みにはしない。S20正式人間RP評価・Phase Bは保留。

次の一作業: S9のprefill/bothを明示roleで実装しD6を検証する。今回のS8依頼ではS9へ着手していない。


## 追加β/M補完（2026-10-02）

追加33条件/99生成＋N0再確認3生成を実施。統合77条件/231生成、回答200件・EOS197件。初回44条件の記録は上記のとおり保持。追加内容・技術/性能/生成の終了0と一次証拠は追加実施記録（元アーカイブ参照: `supplement-beta-m/completion.md`）、最新は比較表（元アーカイブ参照: `comparison.md`） / 単一HTML（元アーカイブ参照: `s8-report.html`）。
