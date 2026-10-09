# server設定切替の技術smoke

追加の品質実験ではなく、少数tokenでHTTP経路を確認するscriptです。既存のE4B GGUFを指定してください。repository rootから実行します。

```bash
python docs/gemma4-recurrence/checks/server-smoke.py \
  --server build/bin/llama-server \
  --model /path/to/gemma-4-E4B-it-Q4_K_M.gguf \
  --output build/server-smoke
```

既定はCPU（gpu-layers0）。CUDA版buildをGPUで確認する場合は `--gpu-layers all` を追加します。その場合scriptは43/43層offloadのlogも確認します。localhostの一時portでserverを起動し、終了時に停止します。実行環境のsocket/model読取権限が必要です。

2026-10-09の[保存結果](server-smoke-result.json)はCPUで16項目PASS、終了0です。context512、batch/ubatch64、threads8、greedy/4tokenを使用。GPUの設定切替やWebUIブラウザ操作まで確認した結果ではありません。個別応答/実argv/server.logは指定したoutputへ保存します。公開したJSONは集計だけです。
