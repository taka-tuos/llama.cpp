> 保存時点の作業記録です。「次の作業」は当時の記載で、2026-10-09に実験を終了しました。未同梱ファイルへのリンクは元アーカイブ参照へ変更し、実験rootの絶対pathを置換しました。

# S8 large N完了確認・評価・共有HTML

2026-10-03（日本時間）。前回起動したGPU測定の完了を確認し、保存結果をCPU解析。今回の依頼では新推論/build/source変更なし。

## 測定完了

- `runner.exit=0`、runner.logの`COMPLETE 102 generations`を確認。参照→N4→8→16→32の全15 technical/benchmark/generation stage終了コード0。
- 34条件×3ケース×seed42＝102一意label、protocolと欠落/重複なし。全102 CLI/decoder exit0、実settings102件をsampler/seed/予算/argvと照合。context shift0、format_error0。
- 回答98、EOS90、上限12（回答なし4・あり8）。β.5は51/51 EOS、β1・N/M≤2/3は36/36 EOS、β1・N=Mは12/12上限。字数内64/102。N32/M48/β1の図書館はEOSだがparser本文6字。
- 初回N32のcapacity失敗exit134と修正後preflight exit0を保持。全34条件100-token技術検証が完了、同履歴N0のKV/state・callback有無一致、Q6N・PLE/KV追加なし・role/有限性確認。独立float32更新2947地点最大誤差0。旧output N0–4/legacy19/linear N1–3回帰はstage0で合格。証拠は`stage-*.technical-pass.json`/`stage-*.formula-diagnostics.json`/technicalログ。
- 全34条件でwarm-up1＋測定3回の同履歴benchmark exit0。条件別中央値をNでまとめたdecode中央値はN0=82.50、N4=57.82、N8=44.47、N16=30.42、N32=18.65 tok/s。execute sampled peak3796–3816 MiB、KV224 MiB。poll実間隔の条件別中央値約68–70ms。短いpeakを見逃しうるGPU全体のサンプル値。
- 参照4条件×3ケースの12生成は旧Long RP seed42のIDs・contentと一致。独立した追加反復として数えない。

## 評価とHTML

- [評価](large-n-review.md)：全件機械診断、条件横断の本文抜粋と代表全文・失敗例を読解。大Nに応じてRPが一貫して改善する傾向は見えず、客の主体性・語法・反復・語尾指定が課題。正式採点や改善認定ではない。
- 診断（元アーカイブ参照: `review-analysis.json`）：raw余分なchannel終端3件、メタ記述候補9件。にゃの引用外混入N0=0/1、β.5=3/17、β1=7/15回答。機械的な文字列診断で意味的完全採点ではない。
- 単一HTML（元アーカイブ参照: `s8-large-n-report.html`）：旧Long RPの図解・カード・全文UIを再利用。N/M比率別マップ、終了積み上げ、自由生成と固定負荷速度を分離した表、固定負荷速度図、全102回答/think/raw/IDs/settings/commandを収録。左右どちらも条件を選べ、同じ比率・異なるNも並べられる。約4.05 MiB、外部通信なし。
- 全保存本文/think・raw元バイトと埋込みデータを照合。2575フィルタ/比較チェック、全102件ページ表示、HTMLエスケープ、JS構文を確認。実ブラウザのレイアウト/クリック/download挙動は未確認。

## コマンドと終了コード

project rootから以下を実行、すべてexit0。

```bash
python runs/2026-10-03-s8-large-n/verify-report.py > runs/2026-10-03-s8-large-n/report-checks.log
```

このscriptから以下を逐次実行し、実argv/exit/logをreport-checks.json（元アーカイブ参照: `report-checks.json`）へ保存。

```bash
python runs/2026-10-03-s8-large-n/summarize-benchmarks.py
python runs/2026-10-03-s8-large-n/review-analysis.py
python runs/2026-10-03-s8-large-n/prepare-report-template.py
python runs/2026-10-03-s8-large-n/build-report.py
node runs/2026-10-03-s8-large-n/validate-report.cjs
node --check runs/2026-10-03-s8-large-n/report-script.js
```

各stdout/stderrはscript名に対応するログへ保存。全102保存データ検証（元アーカイブ参照: `report-validation.json`） / UIログ（元アーカイブ参照: `report-ui-validation.log`） / 固定負荷集計（元アーカイブ参照: `benchmark-summary.json`）。launch時hash対象のscript/条件/入力/source/binary/libraryを再照合して不変。GGUF hashはlaunch記録を保持し、今回は再hashしていない。

実cwd `<experiment-root>`。既存Git差分12ファイル/pacman.txtを保持。GPU照会のみ昇格でexit0、RTX2060 SUPER/driver615.71.09/8192MiB・idle1MiB/0%確認。モデル・元ログ・旧HTMLは保全、commit/pushなし。

## 残件

新条件はseed42のみ。追加4seedの候補固定/生成、正式S20・一対一/複数ターン・S9/serverは未実施。次の一作業は今回の評価を参照して追加seedへ残す候補を固定すること。今回のHTML作成を新しい測定の開始として扱わない。
