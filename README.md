# hackson2 — Arduinoオーケストラ 計画書リポジトリ

## 初回のみ（クローン）

```bash
# 1. 好きな場所に移動（例：デスクトップ）
cd ~/Desktop

# 2. リポジトリをクローン
git clone https://github.com/you0209/hackson2.git

# 3. フォルダに入る
cd hackson2
```

---

## 毎回の作業フロー

```bash
# 作業前：必ず最新を取得
git pull

# （ここで自分の担当ファイルを編集）

# 変更を確認
git status

# 変更をステージ（自分のファイルだけ指定する）
git add sections/design_basic.tex

# コミット
git commit -m "担当A：回路図の説明を追記"

# プッシュ
git push
```

---

## 担当ごとの編集ファイル

| 担当 | 編集するファイル |
|------|----------------|
| 担当A | `sections/design_basic.tex` / `sections/design_detail.tex`（担当Aの節） |
| 担当B | `sections/design_basic.tex` / `sections/design_detail.tex`（担当Bの節） |
| 担当C | `sections/design_basic.tex` / `sections/design_detail.tex`（担当Cの節） |
| 担当D | `sections/design_basic.tex` / `sections/design_detail.tex`（担当Dの節） |
| 全員 | `sections/overview.tex` / `sections/schedule.tex`（代表者1人が担当） |

**一番大事なルール：編集前に必ず `git pull` してから作業する。同じファイルを同時に触らなければ競合は起きない。**

---

## よくあるエラーと対処

**pushしたら弾かれた**
```bash
# 誰かが先にpushしていた → pullしてから再度push
git pull
git push
```

**pullしたらコンフリクトが出た**
```bash
# 同じファイルを同時に触った場合
# ファイルを開いて <<<< ==== >>>> の部分を手で直す
# 直したら
git add 直したファイル名
git commit -m "コンフリクト解消"
git push
```

---

## ファイル構成

```
hackson2/
├── management_plan.tex       # メインファイル（基本触らない）
├── sections/
│   ├── overview.tex          # 1.1 概要・目的・独自性
│   ├── scope.tex             # 1.2 FBS / PBS
│   ├── schedule.tex          # 1.3 WBS・ガントチャート
│   ├── vv_plan.tex           # 1.4 MOE / MOP / TPM
│   ├── resources.tex         # 1.5 資源・調達管理
│   ├── risk.tex              # 1.6 リスク管理
│   ├── design_basic.tex      # 第2章 基本設計
│   └── design_detail.tex     # 第3章 詳細設計
└── figures/                  # 図ファイルをここに入れる
```
