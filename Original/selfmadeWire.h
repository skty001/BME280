class selfmadeWire {
  public:
    unsigned char rcvBuf[248];
    void begin();
    void beginTransmission(unsigned char addr);
    void write(unsigned char data);
    void endTransmission();
    void requestFrom(unsigned char addr, unsigned char len);
    unsigned char available();
    unsigned char read();
  private:
    void BITWRITE(unsigned char data, unsigned char len);
    unsigned char BITREAD();
    void waitRcvAck();
    unsigned char availableFlag = 0x00;
    unsigned char readOffset = 0x00;
    unsigned char totalSize = 0x00;
};

// 自作のWire.begin()関数
void selfmadeWire::begin() {
  unsigned i;
  pinMode(SCL, OUTPUT);
  pinMode(SDA, OUTPUT);

  // ダミークロック(9クロック)
  digitalWrite(SCL, LOW);
  for(i = 0; i < 9; i++){
    digitalWrite(SCL, HIGH);
    digitalWrite(SCL, LOW);
  }

  // ストップコンディション出力
  digitalWrite(SCL, LOW);    delay(1);    // DATA：初期化
  digitalWrite(SCL, HIGH);   delay(1);    // SCL：初期化
  digitalWrite(SDA, HIGH);   delay(1);    // DATA：送信開始
  delayMicroseconds(5);       // STARTセットアップ時間
}

// 自作のWire.beginTransmission(ADDR)関数
void selfmadeWire::beginTransmission(unsigned char addr) {
  unsigned char i, buf;

  // スタートコンディション出力
  digitalWrite(SDA, HIGH);  delay(1);   // DATA：初期化
  digitalWrite(SCL, HIGH);  delay(1);   // SCL：初期化
  digitalWrite(SDA, LOW);   delay(1);   // DATA：送信開始
  delayMicroseconds(5);       // STARTセットアップ時間

  // スレーブアドレス出力
  BITWRITE(addr, 7);

  // Writeコマンド出力
  BITWRITE(0x00, 1);

  // ACK応答待ち
  waitRcvAck();       // ACK応答待ち

}

// 自作のWire.write(DATA)関数
void selfmadeWire::write(unsigned char data) {
  unsigned char buf;
  BITWRITE(data, 8);  // データ出力
  waitRcvAck();       // ACK応答待ち
}

// 自作のWire.endTransmission()関数
void selfmadeWire::endTransmission() {
  digitalWrite(SCL, LOW);     // DATA：初期化
  digitalWrite(SCL, HIGH);    // SCL：初期化
  digitalWrite(SDA, HIGH);    // DATA：送信開始
  delayMicroseconds(5);       // STARTセットアップ時間
}

// 自作のWire.requestFrom(ADDR, LENGTH)関数
void selfmadeWire::requestFrom(unsigned char addr, unsigned char len) {
  unsigned char buf, i, j;

  // availableFlag, readOffsetをクリア
  availableFlag = 1;
  readOffset = 0;
  totalSize = len;

  for (i = 0; i < 248; i++) rcvBuf[i] = 0;

  // スタートコンディション出力
  digitalWrite(SDA, HIGH);  delay(1);   // DATA：初期化
  digitalWrite(SCL, HIGH);  delay(1);   // SCL：初期化
  digitalWrite(SDA, LOW);   delay(1);   // DATA：送信開始
  delayMicroseconds(5);       // STARTセットアップ時間

  BITWRITE(addr, 7);  // スレーブアドレス出力
  BITWRITE(0x01, 1);  // Readコマンド出力
  waitRcvAck(); // ACK応答を確認

  // 受信処理
  for (i = 0; i < len; i++) {
    buf = 0;
    for (j = 0; j < 8; j++) {
      buf += (BITREAD() << (7 - j));
    }
    rcvBuf[i] = buf;
    if (i != len - 1) BITWRITE(0x00, 1); // NoACKを返す
    else              BITWRITE(0x01, 1);  // ACKを返す
  }

  // ストップコンディション出力
  digitalWrite(SCL, LOW);    delay(1);    // DATA：初期化
  digitalWrite(SCL, HIGH);   delay(1);    // SCL：初期化
  digitalWrite(SDA, HIGH);   delay(1);    // DATA：送信開始
  delayMicroseconds(5);       // STARTセットアップ時間

}

// 自作のWire.available()関数
unsigned char selfmadeWire::available() {
  if (readOffset == totalSize) availableFlag = 0;
  return availableFlag;
}

// 自作のWire.read()関数
unsigned char selfmadeWire::read() {
  readOffset += 1;
  return rcvBuf[readOffset - 1];
}

// 送信関数
// dataの下位(length)bitを上位から送信
void selfmadeWire::BITWRITE(unsigned char data, unsigned char len) {
  unsigned char i, buf;
  for (i = 0; i < len; i++) {
    digitalWrite(SCL, LOW);   // SCL：出力準備
    buf = ((data >> (len - 1 - i)) & 0x01);
    if (buf) digitalWrite(SDA, HIGH);
    else    digitalWrite(SDA, LOW);
    digitalWrite(SCL, HIGH);  delay(1);  // SCL：出力準備OK
    digitalWrite(SCL, LOW);
  }
}

// 受信関数(bit)
unsigned char selfmadeWire::BITREAD() {
  unsigned char buf;

  pinMode(SDA, INPUT);      // SDAを入力端子に
  digitalWrite(SCL, LOW);
  digitalWrite(SCL, HIGH);  delay(1); // 受信準備
  if (digitalRead(SDA) == HIGH)  buf = 0x01;
  else                           buf = 0x00;
  digitalWrite(SCL, LOW);   // 受信完了
  pinMode(SDA, OUTPUT);
  delayMicroseconds(5);
  return (buf);
}

void selfmadeWire::waitRcvAck() {
  unsigned char buf;
  while (1) {
    buf = BITREAD();
    if (buf == 0) break;
  }
}
