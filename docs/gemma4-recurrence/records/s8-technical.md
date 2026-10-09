> 保存時点の作業記録です。「次の作業」は当時の記載で、2026-10-09に実験を終了しました。未同梱ファイルへのリンクは元アーカイブ参照へ変更し、実験rootの絶対pathを置換しました。

# S8 技術検証

D5の数値・KV・role検証は実RTX 2060 SUPERで実行。CPU検証と区別し、旧S7/D4未完を遡って完了にしない。

| 検証 | 結果 |
|---|---|
| CLI | 73ケース、default/common/context伝搬、不正M/N/α/β/mode/scopeをN0/β0/noneでも拒否。speculative追加拒否 |
| API | 不正値・複数sequence・非E4Bを拒否。scope none/β0も非E4B制限維持 |
| graph reuse | N/β/mode/α/M/scope/role変更でreuse不可。同値reuse可 |
| 44条件100token | 全段階有限、extra Q6N・hidden6N、PLE/K/V projection0、KV write48、extra write0、RoPE位置維持・mask同一・tail22/23共有 |
| callback有無 | 全44条件step0/1/100一致。さらに全7診断地点のlogits/state一致 |
| base KV | 全44条件のbase有効KV/stateが同固定履歴N0と一致。各extra直後の48有効KV bytewise不変 |
| 入力/更新/β式 | 保存hidden574pass地点を独立float32 sub/scale/addで再計算、事前8*FLT_EPSILON基準合格 |
| 数学的対照 | α1同M/βのN1–3のstep1/100 logitsがbitwise一致。N1/α0のM2β1対M1β.5も保存値で照合 |
| 旧回帰 | legacy旧19条件のstep0/1/100 logits/state一致。新M1/α0の旧S6共通10条件（N0＋9）の同地点一致。旧output N4の100token評価成功 |
| 無効/role | β0/none各N1–3はN0同一。1-token/複数token/末尾1-token promptでextra0。prompt→decode→prompt→decode→decodeを検証 |

技術proof（元アーカイブ参照: `technical-proof.json`） / 数式とhidden差分（元アーカイブ参照: `independent-formula-diagnostics.json`） / 数学的対照（元アーカイブ参照: `mathematical-controls.json`） / top logits/EOG順位（元アーカイブ参照: `representative-top-logits.json`） / 失敗と修正（元アーカイブ参照: `failure-alpha1.md`）。

技術検証のコマンドは technical-commands.json と before-alpha1-update-fix/technical-commands.json。α1の6条件だけ最終修正後に再実行し、他経路は変更されていないことと保存値を照合。各.log/.exitと同期dumpを保存。
