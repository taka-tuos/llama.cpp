# 計算仕様

層番号は0-basedです。Preludeは0-17、coreは18-23、tailは24-41。Nは通常passに追加する回数です。通常passは保持し、coreだけを追加反復します。

## prelude-input / linear-v1

```text
e = output of normal layer 17
b = output of normal layer 23
s_0 = b
u_i = s_(i-1) + alpha * (e - s_(i-1))
z_i = extra_layers_18_to_23(u_i)
s_i = u_i + (z_i - u_i) / M
h_tail = b + beta * (s_N - b)
```

alpha1はPrelude全採用、alpha0は前状態を採用。MはNと独立です。減衰は各extra core全体の出力直後だけ、beta混合は最後だけ適用します。通常passや層内部の更新は減衰しません。

## 互換方式

output（mode ID0）は入力混合なし。prelude-input-legacy（ID1 / ratio-v1）は `u=(s+alpha*e)/(1+alpha)`、M1のみ。新prelude-input（ID2 / linear-v1）のalphaと意味が異なります。alpha1はlegacyでは半々、新式ではPrelude全採用です。

## KV・role・回帰

extraは更新hiddenからQを計算し、同じ層のbase-pass KVをread-onlyで参照します。PLE、K/V projection/write、位置の増加は行いません。通常のRoPE/mask/norm/FFN/residual/layer scalingとtailのKV共有元22/23は保持します。

prompt roleはextraなし、generated-token roleだけに追加loopを適用します。roleはserverのis_promptから明示し、token数やpast KVから推測しません。最初の生成tokenは通常prefillのlogitsから選ばれるため、decode recurrenceは通常2個目の予測から作用します。

フラグ省略/N0、beta0、scope noneは追加loopなしです。設定はcontext作成時固定。roleとN/M/alpha/beta/mode/scopeをgraph reuse判定に含め、新linearの大Nではgraph nodes予算を増やしています。

C APIで呼ぶ場合、`llama_decode_with_gemma4_recur_role(ctx, batch, is_decode)` を使います。promptにはfalse、生成済みtokenの入力にはtrueを渡します。通常の `llama_decode()` はこのforkではprompt roleのままなので、recurrenceを有効にしても追加loopを実行しません。独自APIとcontext paramsを追加しており、既存のprebuilt bindingsとのABI互換性は保証しません。

## 終了時点

S8の固定技術検証と測定は完了、大Nは別runの固定34条件で検証。S7/D4は未完、S9のprefill/bothと正式S20人間評価は未実施のまま実験を終了しました。改善を証明したという結論にはしません。
