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
git add sections/tantoA_basic.tex

# コミット
git commit -m "担当A：機能一覧を追記"

# プッシュ
git push
```

---

## 担当ごとの編集ファイル

| 担当 | 編集するファイル |
|------|----------------|
| 担当A | `sections/tantoA_basic.tex`（第2章）/ `sections/tantoA_detail.tex`（第3章） |
| 担当B | `sections/tantoB_basic.tex`（第2章）/ `sections/tantoB_detail.tex`（第3章） |
| 担当C | `sections/tantoC_basic.tex`（第2章）/ `sections/tantoC_detail.tex`（第3章） |
| 担当D | `sections/tantoD_basic.tex`（第2章）/ `sections/tantoD_detail.tex`（第3章） |
| 全員 | `sections/common.tex`（第1章：概要・スコープ・WBS・V&V等） |

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
│   ├── common.tex            # 第1章：全員で記入（概要・FBS/PBS・WBS・V&V等）
│   ├── tantoA_basic.tex      # 担当A：第2章 基本設計
│   ├── tantoA_detail.tex     # 担当A：第3章 詳細設計
│   ├── tantoB_basic.tex      # 担当B：第2章 基本設計
│   ├── tantoB_detail.tex     # 担当B：第3章 詳細設計
│   ├── tantoC_basic.tex      # 担当C：第2章 基本設計
│   ├── tantoC_detail.tex     # 担当C：第3章 詳細設計
│   ├── tantoD_basic.tex      # 担当D：第2章 基本設計
│   └── tantoD_detail.tex     # 担当D：第3章 詳細設計
└── figures/                  # 図ファイルをここに入れる（PNG推奨）
```
