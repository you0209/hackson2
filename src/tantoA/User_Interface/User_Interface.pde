import processing.serial.*;

Serial myPort;
String bpmInput = "";
String message = "BPMを入力してください";
int currentBPM = 60;
String currentStateLabel = "待機状態";  // Arduino から受信した状態
int cueCount = 0;  // 事前セット済みキュー数（最大5色）

void setup(){
  size(600, 700);
  String portName = Serial.list()[0];
  myPort = new Serial(this, "/dev/cu.usbmodem34B7DA62063C2", 115200);
  myPort.bufferUntil('\n');  // 改行ごとに serialEvent() を呼ぶ
  
  textAlign(CENTER, CENTER);
  textSize(16);
  
  PFont font = createFont("Meiryo", 20);
  textFont(font);
}

void draw(){
  background(240);
  
  fill(0);
  text("指揮者人形コントロール用アプリケーション", 300, 50);
  
  drawButton(225, 100, 150, 50, "演奏開始", color(100, 200, 100));
  drawButton(225, 160, 150, 50, "演奏終了", color(200, 100, 100));
  if (currentStateLabel.equals("フェルマータ")) {
    drawButton(225, 220, 150, 50, "再開", color(255, 165, 0));
  } else {
    drawButton(225, 220, 150, 50, "フェルマータ", color(100, 100, 200));
  }
  drawButton(225, 280, 150, 50, "キュー開始", color(220, 120, 0));
  
  drawButton(50 , 340, 100, 50, "楽器A", color(100, 100, 100));
  drawButton(150, 340, 100, 50, "楽器B", color(100, 100, 100));
  drawButton(250, 340, 100, 50, "楽器C", color(100, 100, 100));
  drawButton(350, 340, 100, 50, "楽器D", color(100, 100, 100));
  drawButton(450, 340, 100, 50, "楽器E", color(100, 100, 100));

  // キュー状態表示
  fill(cueCount >= 5 ? color(100, 200, 100) : color(50));
  text("キュー: " + cueCount + " / 5", 300, 380);
  
  // 入力フォームの描画
  fill(255);
  rect(200, 450, 200, 40);
  fill(0);
  text("BPM: " + bpmInput + "|", 300, 470);
  
  // ステータス表示
  fill(50);
  text(message, 300, 420);
  text("現在の設定値: " + currentBPM, 300, 520);
  
  // 演奏状態表示
  drawStateLabel(300, 570);
}

// 演奏状態をラベルで表示
void drawStateLabel(int cx, int cy) {
  color stateColor;
  if (currentStateLabel.equals("演奏中")) {
    stateColor = color(100, 200, 100);
  } else if (currentStateLabel.equals("フェルマータ")) {
    stateColor = color(100, 100, 200);
  } else if (currentStateLabel.equals("予備拍")) {
    stateColor = color(255, 200, 0);
  } else {
    stateColor = color(160, 160, 160);  // 待機状態・その他
  }
  
  fill(stateColor);
  rect(cx - 90, cy - 20, 180, 40, 8);
  fill(255);
  text(currentStateLabel, cx, cy);
}

void drawButton(int x, int y, int w, int h, String label, color c) {
  fill(c);
  rect(x, y, w, h, 5);
  fill(255);
  text(label, x + w/2, y + h/2);
}

// Arduino からの受信
void serialEvent(Serial p) {
  String line = trim(p.readStringUntil('\n'));
  if (line == null || line.length() == 0) return;
  
  println("Recv: " + line);
  
  if (line.startsWith("CUE:")) {
    // "CUE:x/5" → キューカウンター更新
    int slash = line.indexOf('/');
    if (slash > 4) {
      cueCount = int(line.substring(4, slash));
      message = "キューセット: " + cueCount + "/5";
    }
  } else if (line.equals("CUE_ACTIVE")) {
    message = "キュー再生開始！";
  } else if (line.equals("ERR:CUE_FULL")) {
    message = "キューは満杯です（5/5）";
  } else if (line.startsWith("STATE:")) {
    // STANDBYに戻ったらキューカウンターリセット
    if (line.equals("STATE:STANDBY")) cueCount = 0;
    String state = line.substring(6);
    if (state.equals("STANDBY"))       currentStateLabel = "待機状態";
    else if (state.equals("PREBEAT"))  currentStateLabel = "予備拍";
    else if (state.equals("PLAYING"))  currentStateLabel = "演奏中";
    else if (state.equals("FERMATA"))  currentStateLabel = "フェルマータ";
    else if (state.equals("STOP"))     currentStateLabel = "停止中";
  } else if (line.startsWith("BPM_OK:")) {
    currentBPM = int(line.substring(7));
  }
}

void mousePressed() {
  // 演奏開始ボタン
  if (mouseX > 225 && mouseX < 375 && mouseY > 100 && mouseY < 150) {
    sendCommand("START");
  }
  // 演奏停止ボタン
  if (mouseX > 225 && mouseX < 375 && mouseY > 160 && mouseY < 210) {
    sendCommand("STOP");
  }
  // フェルマータボタン
  if (mouseX > 225 && mouseX < 375 && mouseY > 220 && mouseY < 270) {
    if (currentStateLabel.equals("フェルマータ")) {
      sendCommand("RESUME");
    } else {
      sendCommand("FERMATA");
    }
  }
  // キュー開始ボタン
  if (mouseX > 225 && mouseX < 375 && mouseY > 280 && mouseY < 330) {
    sendCommand("START_CUE");
  }

  // 楽器ボタン
  if (mouseX > 50 && mouseX < 150 && mouseY > 340 && mouseY < 390) {
    sendCue(1);
  }
  if (mouseX > 150 && mouseX < 250 && mouseY > 340 && mouseY < 390) {
    sendCue(2);
  }
  if (mouseX > 250 && mouseX < 350 && mouseY > 340 && mouseY < 390) {
    sendCue(3);
  }
  if (mouseX > 350 && mouseX < 450 && mouseY > 340 && mouseY < 390) {
    sendCue(4);
  }
  if (mouseX > 450 && mouseX < 550 && mouseY > 340 && mouseY < 390) {
    sendCue(5);
  }
}

void keyPressed() {
  if (key >= '0' && key <= '9') {
    bpmInput += key;
  } else if (key == BACKSPACE && bpmInput.length() > 0) {
    bpmInput = bpmInput.substring(0, bpmInput.length() - 1);
  } else if (key == ENTER || key == RETURN) {
    validateAndSendBPM();
  }
}

void validateAndSendBPM() {
  if (bpmInput.length() == 0) {
    message = "数値を入力してください";
    return;
  }
  
  int val = int(bpmInput);
  if (val >= 40 && val <= 160) {
    currentBPM = val;
    sendCommand("BPM:" + val);
    message = "BPM " + val + " を送信しました";
  } else {
    message = "範囲外です (40~160)";
  }
  bpmInput = "";
}

void sendCommand(String cmd) {
  myPort.write(cmd + "\n");
  println("Sent: " + cmd);
}

void sendCue(int id) {
  myPort.write("INSTRUMENT:" + id + "\n");
  println("Sent: " + "INSTRUMENT:" + id);
}

void setBPM(int bpm) {
  myPort.write("BPM:" + bpm + "\n");
  println("Sent: " + "BPM:" + bpm);
}
