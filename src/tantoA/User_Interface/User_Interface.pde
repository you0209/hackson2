import processing.serial.*;

Serial myPort;
String bpmInput = "";
String message = "BPMを入力してください";
int currentBPM = 100;

void setup(){
  size(600, 700);
  String portName = Serial.list()[0];
  myPort = new Serial(this, "/dev/cu.usbmodem34B7DA62063C2", 115200);
  
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
  drawButton(225, 150, 150, 50, "演奏終了", color(200, 100, 100));
  drawButton(225, 200, 150, 50, "フェルマータ", color(100, 100, 200));
  
  drawButton(50 , 300, 100, 50, "楽器A", color(100, 100, 100));
  drawButton(150, 300, 100, 50, "楽器B", color(100, 100, 100));
  drawButton(250, 300, 100, 50, "楽器C", color(100, 100, 100));
  drawButton(350, 300, 100, 50, "楽器D", color(100, 100, 100));
  drawButton(450, 300, 100, 50, "楽器E", color(100, 100, 100));
  
  // 入力フォームの描画
  fill(255);
  rect(200, 450, 200, 40);
  fill(0);
  text("BPM: " + bpmInput + "|", 300, 470);
  
  // ステータス表示
  fill(50);
  text(message, 300, 420);
  text("現在の設定値: " + currentBPM, 300, 520);
}

void drawButton(int x, int y, int w, int h, String label, color c) {
  fill(c);
  rect(x, y, w, h, 5);
  fill(255);
  text(label, x + w/2, y + h/2);
}

void mousePressed() {
  // 演奏開始ボタン
  if (mouseX > 225 && mouseX < 375 && mouseY > 100 && mouseY < 150) {
    sendCommand("START");
  }
  // 演奏停止ボタン
  if (mouseX > 225 && mouseX < 375 && mouseY > 150 && mouseY < 200) {
    sendCommand("STOP");
  }
  // フェルマータボタン
  if (mouseX > 225 && mouseX < 375 && mouseY > 200 && mouseY < 250) {
    sendCommand("FERMATA");
  }
  
  //楽器A
  if (mouseX > 50 && mouseX < 150 && mouseY > 300 && mouseY < 350) {
    sendCue(1);
  }
  if (mouseX > 150 && mouseX < 250 && mouseY > 300 && mouseY < 350) {
    sendCue(2);
  }
  if (mouseX > 250 && mouseX < 350 && mouseY > 300 && mouseY < 350) {
    sendCue(3);
  }
  if (mouseX > 350 && mouseX < 450 && mouseY > 300 && mouseY < 350) {
    sendCue(4);
  }
  if (mouseX > 450 && mouseX < 550 && mouseY > 300 && mouseY < 350) {
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
  // BPM 60~140の範囲外ならエラー
  if (val >= 40 && val <= 160) {
    currentBPM = val;
    sendCommand("BPM:" + val);
    message = "BPM " + val + " を送信しました";
  } else {
    message = "範囲外です (60~140)";
  }
  bpmInput = ""; // 入力欄をクリア
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
