> 保存時点の作業記録です。「次の作業」は当時の記載で、2026-10-09に実験を終了しました。未同梱ファイルへのリンクは元アーカイブ参照へ変更し、実験rootの絶対pathを置換しました。

# S8 Multiturn RP / N8上限・3seed 完了確認

2026-10-04。N0/M1/β1、N1/M4/β.5、N2/M4/β.5・1、N8/M16/β.5・1、α0/linear-v1/decode。daily/travel/nyaの各5ターン、seed42/12345/20261003。

54会話・270返答がすべて本文付きEOS。形式エラー・生成上限・context shiftは0。全54 CLI exit0。seed42は旧同build比較の18会話・90返答をhash付き参照して再利用。新規36会話・180返答は全個別runner exit0、measure.pyのrunner.exit0。CLI wallの新規分合計982.44秒、待機・準備除外。

実行コマンドは `python -u runs/2026-10-04-s8-multiturn-3seed/measure.py`（バックグラウンド、終了0）。launch.json、runner.log、runner-commands.json、各会話command.json / generation.exit / generation.logに個別argvとexitを保存。固定入力・build/source/hashと技術検証参照はprotocol.json / launch-hashes.json / reused-sessions.jsonに保持。

全件監査は `python runs/2026-10-04-s8-multiturn-3seed/build-report.py > runs/2026-10-04-s8-multiturn-3seed/build-report.log 2>&1`（終了0）。270返答のsampler/seed、履歴継承、CLIのrecurrence値と予算、本文/thinkファイル、EOS/非truncated、入力・build hash一致を確認。recurrenceはstreamed settingsにないためargvと同build技術検証を根拠とする。context8192の100token技術検証は旧runの合格証拠を再利用し、新技術/性能測定とは区別。

全270 assistant本文の読解からreview.mdへ所見を記録。語尾分離の局所改善は別seedにもあるが、総合RP改善は未認定。最終情報は全条件成功し、N0からの記憶改善を示す差はない。最大prompt903 tokenで長期context試験ではない。

単一HTMLはs8-multiturn-3seed-report.html、HTML・UIの検証はhtml-record.mdとreport-ui-validation.json参照。新推論/build/source/model変更なし。S9/server/正式S20は未実施。
