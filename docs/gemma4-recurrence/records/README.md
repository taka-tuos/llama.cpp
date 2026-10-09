# 同梱する作業記録

実験全体の進行管理・後続計画・大量の一次ログをそのまま移す代わりに、現実装の理解に必要な完了記録・技術検証・考察を選びました。元記録は削除していません。ここでの成功件数・性能は保存済み実験の報告であり、公開準備で再測定した結果ではありません。

| 記録 | 内容 |
|---|---|
| [S8完了](s8-completion.md) / [技術検証](s8-technical.md) / [考察](s8-review.md) | 線形alpha、1/M更新、beta、none/decodeと固定比較 |
| [長文RP完了](long-rp-completion.md) / [考察](long-rp-review.md) | 19条件・5seed・285生成。強い設定の失敗も保持 |
| [大N完了](large-n-completion.md) / [考察](large-n-review.md) | N32まで固定34条件の技術検証・性能・102生成 |
| [複数ターン完了](multiturn-completion.md) / [考察](multiturn-review.md) | 6設定・3seed・54会話・270返答、局所変化と総合改善未確認 |

実測ホストはRTX 2060 SUPER 8 GiB。モデルはGemma 4 E4B Q4_K_M、SHA-256 `dff0ffba4c90b4082d70214d53ce9504a28d4d8d998276dcb3b8881a656c742a`。記録したbuild環境はRelease、CUDA/native、GCC 16.2.1、CUDA toolkit 13.4.92、driver 615.71.09。これは保存済み環境情報で、現在の環境・他の配布GGUFの一致は保証しません。

現在の実装差分は大Nrunと3seed runのsource.patchにバイト一致することを公開準備時に確認しました。patchのSHA-256は `49d6ac018f34f482843d3762dd494a3c10622b0d32d36235a8dc0475441b856b`。ファイルの出所・元SHA-256・同梱版SHA-256は[manifest](manifest.json)に記録しています。

一次ログ、commands JSON、生成全文、HTML、build、モデル、KV/hidden dump、実行harnessはこのforkには同梱していません。記録内の元アーカイブ参照やコマンドは履歴情報です。このfork単独でそれらのコマンドを再実行できる構成ではなく、完全な再現用アーカイブではありません。未同梱の一次証拠までfork上で検査できるという主張はしません。

実験の終了理由は総合的な改善を確認できず、追加探索を止めたことです。S7/D4未完、S9/D6未実施、正式S20評価未実施を合格へ変更していません。先頭のusageは現在の使い方、ここにある「次の探索」は当時の記載です。
