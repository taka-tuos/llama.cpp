# Gemma 4 E4B recurrence PoC

2026-10-09: 実験をいったん終了し、現状を保存したforkです。学習なしで中間層を追加反復すると、日本語RPの品質が改善するかを試しました。語尾指定などに局所的な変化はありましたが、総合的な品質改善は確認できませんでした。最適設定や実用上の優位を示すものではありません。

基準commit: `6a1a922d269908a29cbd4b49c27e6a8e7fd10fae`。

実装と文書整備にはAI支援を使用しました。既存のupstreamライセンスを保持します。

- [HTTP server / 内蔵WebUIでの設定方法](usage.md)
- [計算仕様とAPIの制限](implementation.md)
- [枝刈りした作業記録・検証結果・考察](records/README.md)

対象はGemma 4 E4Bのtext入力、単一sequence、decode側のみです。新しい方式では追加回数Nを0-32に設定できます。prefill/both、応答ごとの設定変更、専用WebUI設定パネルは未実装です。設定はserver起動時に固定し、変更時は再起動します。

既存のCLIは内部HTTP server経由で検証されています。独立serverの内蔵WebUI操作、長期利用、context shift、他モデル/量子化/各backendの動作は検証済みとして扱いません。この公開準備では再build・新推論・GPU再検証を行っていません。
