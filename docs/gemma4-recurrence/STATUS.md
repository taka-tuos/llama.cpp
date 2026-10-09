# PoC forkの現在地

更新: 2026-10-09（日本時間）。追加の品質実験はいったん終了しています。

## 実施内容

- base: `6a1a922d269908a29cbd4b49c27e6a8e7fd10fae`、branch: `poc/gemma4-recurrence`。
- 既存decode recurrenceと枝刈りした作業記録を保存。S8まで実装、prefill/bothは未実装。
- server実装commit: `390c5b3d2c201a2ada62276e3f343fe8a49436d4`。
- 単一slotのserverへ応答単位の設定変更を追加。既存WebUI Custom JSONまたはHTTP bodyで指定する。[使い方](usage.md)。
- 公開用文書をこのdirectoryへ整理。[作業経緯](WORKLOG.md)、[実験の記録](records/README.md)、[server追加の記録](records/server-integration.md)。

## 検証結果

新CPU版server build終了0、HTTP技術smokeは16項目PASS/終了0。元実装の保存時には大N/3seed source.patchのバイト一致を確認。新server切替はその後の変更なので既存GPU受入を引き継いで合格とは扱いません。

## 未解決点

新GPU版設定切替、実ブラウザ操作、長期利用、全設定組合せは未検証。総合RP品質改善は未認定。S7/D4、S9/D6、正式S20は未完のまま終了。[GitHub fork](https://github.com/taka-tuos/poc.recur.gemma4-e4b.llama.cpp/tree/poc/gemma4-recurrence)の `poc/gemma4-recurrence` ブランチへ公開済み。

## 次の一作業

PoCの保存・forkへの公開は完了。追加品質実験や新機能は自動で進めない。GitHubの既定branchはmasterのままなので、実験コードを見るときはpoc/gemma4-recurrenceを選択する。
