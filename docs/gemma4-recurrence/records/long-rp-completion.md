> 保存時点の作業記録です。「次の作業」は当時の記載で、2026-10-09に実験を終了しました。未同梱ファイルへのリンクは元アーカイブ参照へ変更し、実験rootの絶対pathを置換しました。

# S8 Long RP完了確認・共有HTML

2026-10-03（日本時間）。保存済みrunの確認とHTML作成のみ。新しい推論・build・source変更なし。

## 完了と結果

- resume.exit=0、19条件×3ケース×5 seed=285件。label重複/欠落なし、各条件15件。全285 CLIとtoken decoderの終了コード0を確認。初回runner.exit=1とそのログを保全。
- 回答到達246/285、EOS232、上限51（回答あり14・回答なし37）、形式エラー2。settings283件を保存しsampler/seed/予算を照合。エラー2件は本文/think/実settings unknown、raw/IDsは保存。context shiftなし。
- β.5は135/135件回答、EOS134/135。β1・N2/M2は回答0/15・全上限、N3/M2は2/15、N3/M3は6/15。N0は15/15 EOS。回答成立と品質を分離。
- 自由生成decodeの条件別中央値：N0=76.98、N1=69.73–69.89、N2=63.42–63.92、N3=58.52–59.02 tok/s。283件のsettings timingsから集計、エラー2件は速度unknown。固定負荷benchmark/VRAM新測定ではない。
- 各生成wall合計8728.02秒（約2.42時間）、load/prefill/decode込み。復号・停止時間等を含まない。

## HTMLと試読

- 単一HTML（元アーカイブ参照: `s8-long-rp-report.html`）：通常pass/extra/N/M/βの図解、差分1/MのSVG、S6との模式比較、N×M回答到達マップ、終了状態の積み上げ表、速度図。説明を短文・対比表・図へ分割。外部ファイルを必要としない単一ファイルで約11.1 MiB。
- 同じケース/seedのN0対候補を並列表示。全285件をN/M/β/case/seed/終了/全文検索で絞り込み、15件ずつページ送り。回答/think/raw/IDs/全settings/argv/cwd/exit/wallを収録。raw元バイトもダウンロード可能。
- 代表本文の試読所見を短いカードに収録。N0の注文前の飲み物準備/字数メタ文、β.5の客の指差し追加や説明反復、β1/N3/M4の語法崩れ/Self-Correction混入を確認。N1/M4/β.5・nya・seed42では台詞/地の文の語尾分離が読解上成立。全件採点・改善認定・正式人間評価ではない。

## コマンド・検証

cwdはproject root、以下すべてexit0。

```bash
python runs/2026-10-03-s8-long-rp/build-report.py > runs/2026-10-03-s8-long-rp/report-build.log
node runs/2026-10-03-s8-long-rp/validate-report.cjs > runs/2026-10-03-s8-long-rp/report-ui-validation.log
node --check runs/2026-10-03-s8-long-rp/report-script.js
```

- Pythonで285 label、19×15、全exit、保存本文字数、ID件数、sampler/seed/生成予算、error unknownを照合。検証JSON（元アーカイブ参照: `report-validation.json`）。
- NodeのDOM代替環境で実HTMLのJSを実行。375フィルタ組合せ＋270同seed比較=645チェック、全285件のページ表示、比較本文の一致、raw元バイト一致、HTMLエスケープ、図表renderを確認。UI検証ログ（元アーカイブ参照: `report-ui-validation.log`）。実ブラウザのレイアウト・クリック・download挙動は未確認。
- HTML内の本文はtext/エスケープ表示、script-data内のHTML文字もエスケープ。raw不正UTF-8は表示のみ置換しbase64で元バイトを保全。

## 残件

正式S20・複数ターン比較・S9は未実施。既存の別multiturn準備を保持。次の一作業は本文/HTMLの確認後、準備済み別runのN0で5ターン試行。今回その推論やserver実装へは進んでいない。
