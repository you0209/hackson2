/**
 * OST4ML8132A チャンネル診断テスト
 *
 * conductor.ino / conductor_2.ino で「白がYellowに、CyanがGreenに見える」
 * （＝Bチャンネルだけ欠落しているように見える）症状の切り分け用．
 *
 * sendLED() は常に B→G→R の順で24bit送信するため，Bは常に「最初に送る
 * チャンネル」になる．そこで送信順序を入れ替えたモードを用意し，
 *   ・どのモードでも青(B)だけ光らない        → LED/配線のB系統が物理故障
 *   ・「常に最初に送ったチャンネル」が光らない → 送信タイミング（プロトコル）の問題
 * を切り分ける．
 *
 * 接続:
 *   D2 — OST4ML8132A DIN_H（10kΩ経由）
 *   D3 — OST4ML8132A DIN_L（10kΩ経由）
 *   （conductor.ino と同じ配線）
 *
 * 使い方（シリアルモニタ 115200bps，改行不要・1文字コマンド）:
 *   1 : 送信順を NORMAL（B→G→R，conductor.ino と同じ）に切り替え
 *   2 : 送信順を SWAPPED（R→G→B，Bを最後に送る）に切り替え
 *   r : 現在のモードで純赤 (255,0,0) を送信
 *   g : 現在のモードで純緑 (0,255,0) を送信
 *   b : 現在のモードで純青 (0,0,255) を送信
 *   w : 白 (255,255,255)
 *   c : シアン (0,255,255)
 *   y : イエロー (255,255,0)
 *   m : マゼンタ (255,0,255)
 *   o : 消灯
 *   h : ヘルプ表示
 *
 * 診断手順:
 *   1. '1'（NORMAL）のまま r/g/b を試す
 *      → conductor.ino と同じ症状なら b だけ光らない（消灯）はず
 *   2. '2'（SWAPPED）に切り替えて r/g/b を試す
 *      → r（今度は最初に送られる）が光らず，b（今度は最後に送られる）が
 *        光るなら「送信順序＝タイミング起因」が濃厚
 *      → それでも b だけ光らないなら「LED/配線のB系統そのものが故障」が濃厚
 */

#define PIN_DIN_H 2  // D2 — OST4ML8132A DIN_H（10kΩ経由）
#define PIN_DIN_L 3  // D3 — OST4ML8132A DIN_L（10kΩ経由）

enum SendOrder { ORDER_NORMAL, ORDER_SWAPPED };
SendOrder currentOrder = ORDER_NORMAL;

void setup() {
  Serial.begin(115200);

  pinMode(PIN_DIN_H, OUTPUT);
  pinMode(PIN_DIN_L, OUTPUT);
  digitalWrite(PIN_DIN_H, LOW);
  digitalWrite(PIN_DIN_L, HIGH);

  ledOff();
  printHelp();
}

void loop() {
  if (!Serial.available()) return;
  char c = Serial.read();

  switch (c) {
    case '1':
      currentOrder = ORDER_NORMAL;
      Serial.println("mode -> NORMAL (B->G->R, conductor.inoと同じ)");
      break;
    case '2':
      currentOrder = ORDER_SWAPPED;
      Serial.println("mode -> SWAPPED (R->G->B, Bを最後に送る)");
      break;
    case 'r': sendColor(255, 0,   0);   Serial.println("-> Red");     break;
    case 'g': sendColor(0,   255, 0);   Serial.println("-> Green");   break;
    case 'b': sendColor(0,   0,   255); Serial.println("-> Blue");    break;
    case 'w': sendColor(255, 255, 255); Serial.println("-> White");   break;
    case 'c': sendColor(0,   255, 255); Serial.println("-> Cyan");    break;
    case 'y': sendColor(255, 255, 0);   Serial.println("-> Yellow");  break;
    case 'm': sendColor(255, 0,   255); Serial.println("-> Magenta"); break;
    case 'o': ledOff();                 Serial.println("-> Off");     break;
    case 'h': printHelp();              break;
    default: break;  // 改行・スペース等は無視
  }
}

void printHelp() {
  Serial.println("=== LED channel test ===");
  Serial.println("1:NORMAL(B->G->R)  2:SWAPPED(R->G->B)");
  Serial.println("r:Red g:Green b:Blue w:White c:Cyan y:Yellow m:Magenta o:Off h:Help");
  Serial.print("current mode: ");
  Serial.println(currentOrder == ORDER_NORMAL ? "NORMAL" : "SWAPPED");
}

// r,g,bは「意味」としての色。実際に何番目に送るかはcurrentOrderで決める。
void sendColor(uint8_t r, uint8_t g, uint8_t b) {
  if (currentOrder == ORDER_NORMAL) {
    sendLED(b, g, r);  // conductor.inoと同じ: B->G->R
  } else {
    sendLED(r, g, b);  // 入れ替え: R->G->B（Bを最後に送る）
  }
}

// ===================================================================
// sendLED()  — 引数の順にそのまま24bit送信する（呼び出し側が順序を決める）
//   conductor.ino の sendLED()/sendBit() と同一のタイミング実装
// ===================================================================
void sendLED(uint8_t first, uint8_t second, uint8_t third) {
  for (int i = 7; i >= 0; i--) sendBit((first  >> i) & 0x01);
  for (int i = 7; i >= 0; i--) sendBit((second >> i) & 0x01);
  for (int i = 7; i >= 0; i--) sendBit((third  >> i) & 0x01);

  // ラッチ：DIN_H=LOW, DIN_L=HIGH を 3ms 以上保持
  digitalWrite(PIN_DIN_H, LOW);
  digitalWrite(PIN_DIN_L, HIGH);
  delayMicroseconds(3000);
}

void sendBit(bool bit) {
  if (bit) {
    digitalWrite(PIN_DIN_H, HIGH);
    digitalWrite(PIN_DIN_L, HIGH);
    delayMicroseconds(10);  // TH
    digitalWrite(PIN_DIN_L, LOW);
    delayMicroseconds(10);  // TN
  } else {
    digitalWrite(PIN_DIN_H, LOW);
    digitalWrite(PIN_DIN_L, LOW);
    delayMicroseconds(10);  // TL
    digitalWrite(PIN_DIN_H, HIGH);
    delayMicroseconds(10);  // TN
  }
  delayMicroseconds(5);
}

void ledOff() {
  sendLED(0, 0, 0);
}
