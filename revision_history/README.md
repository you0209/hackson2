# 課題3 — 変更履歴文書

提出物3「変更履歴の記述（初回を除き，変更履歴を追記していく）」用の独立文書フォルダ．

## 2系統の変更履歴

本プロジェクトでは，変更履歴を **2つのファイル** に分けて管理する．

| ファイル | 用途 | リセット |
|---|---|---|
| `../sections/changelog_current.tex` | 課題1・課題2の変更履歴章 | **提出毎にリセット** |
| `./sections/changelog.tex` | 課題3の累積変更履歴 | **リセットしない** |

- `changelog_current.tex` は，前回提出から今回提出までの差分のみを保持する．
- `revision_history/sections/changelog.tex` は，プロジェクト開始時から現在までの全履歴を保持する．

## フォルダ構成

```
hackson2/
├── sections/
│   └── changelog_current.tex      ← 直近版（課題1,2用）
└── revision_history/
    ├── revision_history.tex       ← 課題3の独立PDFメイン文書
    ├── sections/
    │   └── changelog.tex          ← 累積版（課題3用）
    └── README.md                  ← このファイル
```

## 履歴の運用フロー

### 1. 変更が発生したら

`sections/changelog_current.tex` の `\endhead` と `\bottomrule` の間に，次の書式で1行追記する．

```
YYYY-MM-DD & 学籍番号 : 氏名 & 対象（章・節） & 変更内容 \\
```

例：

```
2026-06-01 & 25G1099 : 長山 魁良 & 第3章 担当C基本設計 & 関数設計の表を改訂 \\
```

**この段階では累積版（`revision_history/sections/changelog.tex`）には触らない．**

### 2. 提出のタイミングで

1. `sections/changelog_current.tex` に追記された全ての履歴行を，
   `revision_history/sections/changelog.tex` の末尾（`\endhead` と `\bottomrule` の間）にコピーする．
2. `sections/changelog_current.tex` の履歴行を全て削除する（表のヘッダ・フッタは残す）．

これにより，

- 課題1・課題2 のPDF（マネジメント計画書／設計書）には「今回提出分の変更履歴」だけが載る．
- 課題3 のPDF（独立した変更履歴文書）には「全期間の累積変更履歴」が載る．

### 記載のコツ

- **日付**: その変更を行った日（コミット日に合わせるのが目安）
- **担当**: 学籍番号と氏名（`management_plan.tex` の著者表記に合わせる）
- **対象**: 章節を具体的に書く（例: 第2章 担当A基本設計 → 機能一覧）
- **変更内容**: 「何を」だけでなく「どう変えたか」を簡潔に．長くなる場合は複数行に分けてよい

## 課題3のビルド方法

このフォルダ内で実行：

```bash
cd revision_history
lualatex revision_history.tex
```

`revision_history.pdf` が同フォルダに生成される．

## 注意

- 課題3のメイン文書（`revision_history.tex`）は，親文書の `hack-cover.sty` `hack-fonts.sty` を **使っていない**．
  課題3の提出物は変更履歴のみのシンプルなPDFで十分なため，独自のミニマル設定で完結している．
- 親文書とビジュアル統一を取りたい場合は，スタイルファイルの読込を追加する．
