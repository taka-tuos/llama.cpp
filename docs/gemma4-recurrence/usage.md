# HTTP server / 内蔵WebUIで使う

起動引数を既定値として、HTTP requestまたはWebUIのCustom JSONから応答ごとに上書きできます。POST /propsで変更する機能はありません。

## ビルド

forkのrepository rootで実行します。以下はCUDA環境用の例です。CUDA toolkit、対応compiler、GPUを別途用意してください。その他のbackendのビルド方法は[upstreamのビルド文書](../build.md)を参照してください。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
  -DGGML_CUDA=ON -DLLAMA_BUILD_SERVER=ON \
  -DLLAMA_BUILD_UI=OFF -DLLAMA_USE_PREBUILT_UI=ON
cmake --build build --parallel "$(nproc)" --target llama-server
```

実験ホストは80論理CPUでparallel 80を使用しました。prebuilt UIの取得にはネットワークが必要になる場合があります。この手順は固定ソースのbuild optionsに基づく案内で、公開準備時に新環境で実行したものではありません。

## 起動とWebUI

自分で用意したGemma 4 E4B Q4_K_M GGUFのpathを指定します。以下は小Nの比較例で、推奨品質設定を認定したものではありません。

```bash
./build/bin/llama-server \
  --model /path/to/gemma-4-E4B-it-Q4_K_M.gguf \
  --gpu-layers all --ctx-size 4096 \
  --threads 8 --threads-batch 8 \
  --parallel 1 --no-context-shift --jinja \
  --host 127.0.0.1 --port 8080 \
  --gemma4-recur-mode prelude-input \
  --gemma4-recur 1 --gemma4-recur-m 4 \
  --gemma4-recur-alpha 0 --gemma4-recur-beta 0.5 \
  --gemma4-recur-scope decode
```

server起動後、ブラウザで `http://127.0.0.1:8080` を開き、内蔵WebUIから会話します。起動時の既定値を変えるときはserverを再起動します。応答ごとの上書きは下記のCustom JSONを使います。ブラウザの会話履歴とserverのKV cacheは別です。再起動でserver側cacheは失われます。

baselineは上のコマンドの `--gemma4-recur 1` を `--gemma4-recur 0` に変更します。recurrence引数を全部省略してもN0です。比較するならprompt、履歴、chat template、thinking、sampler、seed、生成上限を揃えてください。再生成した履歴が異なる会話の印象は、同履歴の比較ではありません。

## HTTPリクエスト例

起動時のrecurrence設定はHTTP APIからの会話にも適用されます。設定をrequestへ追加する必要はありません。

```bash
curl http://127.0.0.1:8080/v1/chat/completions \
  -H 'Content-Type: application/json' \
  -d '{"messages":[{"role":"user","content":"こんにちは"}],"max_tokens":128,"stream":false}'
```

HTTP APIの設定反映はCPUで技術smokeを確認しています。実ブラウザでの独立WebUI操作は未確認です。APIの他の使い方は[server文書](../../tools/server/README.md)を参照してください。

## WebUIのCustom JSON / HTTPで応答ごとに変更

WebUIの設定にあるCustom JSON欄へ次を入れます。WebUI sourceではこの欄をrequest bodyへ追加します。配布UIのブラウザ操作は未確認です。

```json
{
  "gemma4_recur": {
    "n": 1,
    "m": 4,
    "alpha": 0,
    "beta": 0.5,
    "mode": "prelude-input",
    "scope": "decode"
  }
}
```

HTTPでも同じ `gemma4_recur` オブジェクトを `/completion` または `/v1/chat/completions` のbodyへ追加します。baselineは `{"gemma4_recur":{"n":0}}`。変更はそのリクエストの生成開始前に適用し、生成途中には切り替えません。

省略したキーは起動時の既定値です。前の応答の値を継承しません。N8などを指定する場合、起動modeがoutputなら `mode: "prelude-input"` も指定してください。

設定が変わるとserver側のslot prompt/checkpointとRAM prompt cacheを破棄して履歴を再prefillします。ブラウザにある会話履歴は残ります。その分、設定変更直後の応答開始は遅くなります。複数タブのリクエストは単一slotのqueueで順に処理します。

この切替は `--parallel 1`、text-only、speculativeなしで使います。不正な値・未対応scope・使えない組合せはHTTP 400を返します。適用値はserverのdebug log（`--log-verbosity 5`）とnative応答の `generation_settings.gemma4_recur` に記録します。OpenAI互換APIでは `"verbose": true` を追加すると `__verbose.generation_settings.gemma4_recur` で確認できます。通常のWebUI画面へ適用値を表示する機能は追加していません。

## 設定値

| 引数 | 既定 | 許容値・意味 |
|---|---|---|
| --gemma4-recur | 0 | 通常passに追加するN回。prelude-inputは0-32、outputは0-4、legacyは0-3 |
| --gemma4-recur-mode | output | output / prelude-input / prelude-input-legacy |
| --gemma4-recur-m | 1 | 正のint32（1-2147483647）。各extra後の更新差分を1/Mへ縮小。Nとは独立 |
| --gemma4-recur-alpha | 0 | 有限の0-1。新式ではPreludeの割合、旧legacyでは相対重み。outputは0のみ |
| --gemma4-recur-beta | 1 | 有限の0-1。最後に候補出力を混ぜる割合。0はextra計算なし |
| --gemma4-recur-scope | decode | none / decode。prefill / bothはエラー |

legacyはM1のみ。outputのN4はM1/beta1のみ。Nが正なら、beta0やscope noneでもE4B・単一sequence制限は適用されます。`--parallel 1` を明示し、speculative decodingを使わないでください。

text-onlyで検証しています。画像/音声、複数slot、context shiftやcacheの外部保存/復元を使う運用、長期会話は本PoCの受入範囲外です。context上限に達したら履歴を短くするか新しい会話へ移ります。全N/M/alpha/betaの組合せを検証したわけではありません。
