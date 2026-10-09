# HTTP server設定切替の作業記録

2026-10-09（日本時間）。ユーザー依頼でPoC用の小さい設定経路を追加。追加RP評価、prefill/both、専用UI、複数slot対応は実施していません。

## 実装

保存commit: `390c5b3d2c201a2ada62276e3f343fe8a49436d4`。

- `gemma4_recur` オブジェクトのn/m/alpha/beta/mode/scopeをrequestからtaskへ保存。未指定キーは起動時設定を使い、前requestの設定を継承しない。
- workerが応答開始前に設定を適用。単一text-only slot、speculativeなし。contextの起動時検証を共用し、同期後にgraph scheduler/reserveを更新。
- 変更時にslot prompt/checkpointとRAM prompt cacheを破棄。送られた会話履歴を再prefill。ブラウザの履歴は変更しない。
- role選択は起動時Nではなく実適用Nを使う。native generation_settingsとdebug logへ適用値を保存。OpenAI互換のverbose応答でも確認可能。
- 設定値の不正/未対応組合せはHTTP400。予約失敗時は前設定へ戻し、次の処理でreserveを再構築する。

## ビルドと技術検証

repository rootで以下を実行、各終了0。新buildはCPU版で、既存実験buildは変更していません。

```bash
cmake -S . -B build-poc-server -DGGML_CUDA=OFF -DCMAKE_BUILD_TYPE=Release -DLLAMA_BUILD_SERVER=ON -DLLAMA_USE_PREBUILT_UI=OFF -DLLAMA_BUILD_TESTS=ON
cmake --build build-poc-server --parallel 80 --target llama-server
python docs/gemma4-recurrence/checks/server-smoke.py --server build-poc-server/bin/llama-server --model /path/to/gemma-4-E4B-it-Q4_K_M.gguf --output build-poc-server/checks-cpu
```

LLAMA_BUILD_TESTS=ONを指定しましたが、build対象はllama-serverだけです。全CTest合格とは記録しません。チェック内容は[script/保存結果](../checks/README.md)を参照。N0->候補->N0のtoken一致、beta0回帰、alpha/M/beta変更、N32 graph容量、streaming、queue、OpenAI互換API、不正値/組合せ後の復帰を16項目確認しました。

ログはローカルのbuild-poc-server/configure.log、build.log、rebuild.log、checks-cpu/server.log、個別応答JSON。build treeはGit除外、集計JSONだけ同梱しています。compilerの既存警告はあり、build失敗はありません。

## 環境制約と失敗記録

通常sandboxのGPU照会はexit9、昇格照会はexit0でRTX2060 SUPER/8192MiB/idle1MiBを確認。新CPU版serverから既存CUDA backendを読み込む試行はGPUが使用可能にならずCPUへフォールバック。初回scriptはdebug logのverbosity不足でも停止しexit1。この試行をGPU成功と扱いません。条件をCPU明示/verbosity5へ変更した最終smokeはexit0、全16項目PASSです。通常sandbox内のlocalhost socket作成も拒否されたため、最終HTTPチェックは昇格実行しました。ドライバ/システム設定は変更していません。

GPU版の再build・設定切替、実ブラウザのCustom JSON操作、取消/長期運用、全組合せ、slot cacheの外部保存/復元は未検証です。検証CPU版buildはprebuilt UI OFFであり、配布UIの検証とは区別します。

## 文書整理

fork内のdocs/gemma4-recurrenceを公開用文書の入口とし、usage/implementation/STATUS/WORKLOGへ使い方・現在地・枝刈りした経緯を整理。元の研究root文書や巨大な一次アーカイブは保全し、forkへそのまま持ち込んでいません。古い記録の「次の作業」を新しい実施指示としません。
