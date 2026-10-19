# NEON MAZE for MeOS

M5Stack Tab5のMeOS向けに作った、オリジナルの迷路チェイスゲームです。

## 遊び方

- 矢印キー／WASD: 移動
- Space／Enter: ゲーム開始・リスタート
- Q: 終了

迷路内のスパークをすべて集めるとクリアです。大きなパワーコアを取ると、一定時間ハンターを倒せます。

## インストール

Tab5で `install` を実行し、[`neonmaze_qr.png`](neonmaze_qr.png) をカメラに見せてください。インストール後は `neonmaze` で起動できます。

## ビルドとシミュレーション

[MeOS App SDK](https://github.com/shi3z/MeOS-app-sdk) を用意し、そのルートから実行します。

```sh
tools/meos-sim /path/to/MeOS-NeonMaze/neonmaze.c
tools/meos-build /path/to/MeOS-NeonMaze/neonmaze.c
```

`neonmaze.c` と `neonmaze.manifest` は同じディレクトリに置いてください。
