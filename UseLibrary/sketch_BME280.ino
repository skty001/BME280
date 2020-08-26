
/***************************************************/
/* BME280制御                                       */
/* 参考：http://trac.switch-science.com/wiki/BME280 */
/***************************************************/

#include<Wire.h>

#define BME280_ADDRESS  0x76  // BME280アドレス SDO:LOW → 0x76
#define ADDR_CONFIG     0xF5  // CONFIGレジスタ
#define ADDR_CTRL_MSG   0xF4  // コントロールレジスタ(温度・気圧)
#define ADDR_CTRL_HUM   0xF2  // コントロールレジスタ(湿度)

unsigned long int raw_temp, raw_hum, raw_press;       // 生のセンサ値

// 温度調整パラメータ
unsigned short dig_T1;
signed short dig_T2, dig_T3;

// 気圧調整パラメータ
unsigned short dig_P1;
signed short dig_P2, dig_P3, dig_P4, dig_P5;
signed short dig_P6, dig_P7, dig_P8, dig_P9;

// 湿度調整パラメータ
unsigned char dig_H1, dig_H3;
signed char dig_H6;
signed short dig_H2, dig_H4, dig_H5;

long signed int t_fine;

void setup() {
  Serial.begin(115200);

  // BME280初期設定
  Serial.print("BME280_初期化処理:");
  Wire.begin();
  writeRegister(ADDR_CONFIG, 0b00010000);
  writeRegister(ADDR_CTRL_MSG, 0b01010111);
  writeRegister(ADDR_CTRL_HUM, 0b00000001);
  Serial.println("Done");

  // 調整パラメータ取得
  Serial.print("BME280_調整パラメータ取得:");
  readCalibration();
  Serial.println("Done");

}

void loop() {
  readData();
  dispData();
  delay(1000);
}

void writeRegister(unsigned char addr, unsigned char data) {
  Wire.beginTransmission(BME280_ADDRESS); // SDO:LOW → 0x76
  Wire.write(addr);
  Wire.write(data);
  Wire.endTransmission();
}

void readData() {
  int i = 0;
  unsigned long data[8];
  Wire.beginTransmission(BME280_ADDRESS); // 送信開始
  Wire.write(0xF7);                       //
  Wire.endTransmission();                 // 送信完了
  Wire.requestFrom(BME280_ADDRESS, 8);    // 受信開始
  while (Wire.available()) {              // 8byte受信まで
    data[i] = Wire.read();
    i++;
  }

  int j;
  raw_press = (data[0] << 12) | (data[1] << 4) | (data[2] >> 4);  // 気圧
  raw_temp = (data[3] << 12) | (data[4] << 4) | (data[5] >> 4);   // 温度
  raw_hum = (data[6] << 8) | data[7];                             // 湿度
}

void dispData() {

  signed long int comp_temp;              // 計算用
  unsigned long int comp_hum, comp_press; // 計算用
  double disp_temp, disp_hum, disp_press; // 表示用
  
  comp_temp = BME280_compensate_T_int32(raw_temp);
  comp_hum = BME280_compensate_H_int32(raw_hum);
  comp_press = BME280_compensate_P_int64(raw_press);

  disp_temp = (double)comp_temp / 100;          // 分解能0.01 Ex.5123→51.23℃
  disp_hum = (double)comp_hum / 1024;           // Q22.10形式32Bit整数(22bit整数と10bit小数)
  disp_press = (double)comp_press / 256 / 100;  // Q24.8形式32Bit整数(24bit整数と8bit小数) (Pa→hPa)
  
  Serial.print("Tmp:");
  Serial.print(disp_temp);
  Serial.print("℃ Hum:");
  Serial.print(disp_hum);
  Serial.print("％ Press:");
  Serial.print(disp_press);
  Serial.println("hPa");
}

// BME280上の調整パラメータを取得
void readCalibration(){
  int i = 0;
  unsigned char data[32];
  
  // まず0x88から0xA1まで取得
  Wire.beginTransmission(BME280_ADDRESS); // 送信開始
  Wire.write(0x88);
  Wire.endTransmission();                 // 送信完了
  Wire.requestFrom(BME280_ADDRESS, 25);   // 0x88→0xA1
  while(Wire.available()){
    data[i] = Wire.read();
    i++;
  }

  // 次に0xE1から0xE7を取得
  Wire.beginTransmission(BME280_ADDRESS); // 送信開始
  Wire.write(0xE1);
  Wire.endTransmission();                 // 送信完了
  Wire.requestFrom(BME280_ADDRESS, 7);    // 0xE1→0xE8
  while(Wire.available()){
    data[i] = Wire.read();
    i++;
  }

  // 各々の変数に展開
  dig_T1 = data[0] + (data[1] << 8);
  dig_T2 = data[2] + (data[3] << 8);
  dig_T3 = data[4] + (data[5] << 8);
  dig_P1 = data[6] + (data[7] << 8);
  dig_P2 = data[8] + (data[9] << 8);
  dig_P3 = data[10] + (data[11] << 8);
  dig_P4 = data[12] + (data[13] << 8);
  dig_P5 = data[14] + (data[15] << 8);
  dig_P6 = data[16] + (data[17] << 8);
  dig_P7 = data[18] + (data[19] << 8);
  dig_P8 = data[20] + (data[21] << 8);
  dig_P9 = data[22] + (data[23] << 8);
  dig_H1 = data[24];
  dig_H2 = data[25] + (data[26] << 8);
  dig_H3 = data[27];
  dig_H4 = (data[28] << 4) + (data[29] & 0x0F);
  dig_H5 = ((data[29] >> 4) & 0x0F) + (data[30] << 4);
  dig_H6 = data[31];
}



/*****************************************************************/
/* 以下データシートより                                             */
/* http://www.ne.jp/asahi/o-family/extdisk/BME280/BME280_DJP.pdf */
/*****************************************************************/

// 温度を℃で返します。 分解能は0.01℃です。 「5123」の出力値は、51.23℃に相当します。
// t_fineは、グローバル値として細かい温度値を持ちます。
long signed int BME280_compensate_T_int32(long signed int adc_T) {
  long signed int var1, var2, T;
  var1 = ((((adc_T >> 3) - ((long signed int)dig_T1 << 1))) * ((long signed int)dig_T2)) >> 11;
  var2 = (((((adc_T >> 4) - ((long signed int)dig_T1)) * ((adc_T >> 4) - ((long signed int)dig_T1))) >> 12) * ((long signed int)dig_T3)) >> 14;
  t_fine = var1 + var2;
  T = (t_fine * 5 + 128) >> 8;
  return T;
}

// 圧力（Pa）を、Q24.8形式の符号なし32ビット整数として返します。 （24個の整数ビットと8個の小数ビット）
// 「24674867」の出力値は、24674867/256 = 96386.2Pa = 963.862hPaに相当します。
long unsigned int BME280_compensate_P_int64(long unsigned int adc_P)
{
  long long signed int var1, var2, p;
  var1 = ((long long signed int)t_fine) - 128000;
  var2 = var1 * var1 * (long long signed int)dig_P6;
  var2 = var2 + ((var1 * (long long signed int)dig_P5) << 17);
  var2 = var2 + (((long long signed int)dig_P4) << 35);
  var1 = ((var1 * var1 * (long long signed int)dig_P3) >> 8) + ((var1 * (long long signed int)dig_P2) << 12);
  var1 = (((((long long signed int)1) << 47) + var1)) * ((long long signed int)dig_P1) >> 33;
  if (var1 == 0)
  {
    return 0; // ゼロ除算による例外を避ける。
  }
  p = 1048576 - adc_P;
  p = (((p << 31) - var2) * 3125) / var1;
  var1 = (((long long signed int)dig_P9) * (p >> 13) * (p >> 13)) >> 25;
  var2 = (((long long signed int64_t)dig_P8) * p) >> 19;
  p = ((p + var1 + var2) >> 8) + (((long long signed int)dig_P7) << 4);
  return (long unsigned int)p;
}

// 湿度（%RH）を、Q22.10形式の符号なし32ビット整数として返します。 （22個の整数と10個の小数ビット）
// 「47445」の出力値は、47445/1024 = 46.333%RHに相当します。
long unsigned int BME280_compensate_H_int32(long signed int adc_H)
{
  long signed int v_x1_u32r;
  v_x1_u32r = (t_fine - ((long signed int)76800));
  v_x1_u32r = (((((adc_H << 14) - (((long signed int)dig_H4) << 20) - (((long signed int)dig_H5) * v_x1_u32r)) +
                 ((long signed int)16384)) >> 15) * (((((((v_x1_u32r * ((long signed int)dig_H6)) >> 10) * (((v_x1_u32r *
                     ((long signed int)dig_H3)) >> 11) + ((long signed int)32768))) >> 10) + ((long signed int)2097152)) *
                     ((long signed int)dig_H2) + 8192) >> 14));
  v_x1_u32r = (v_x1_u32r - (((((v_x1_u32r >> 15) * (v_x1_u32r >> 15)) >> 7) * ((long signed int)dig_H1)) >> 4));
  v_x1_u32r = (v_x1_u32r < 0 ? 0 : v_x1_u32r);
  v_x1_u32r = (v_x1_u32r > 419430400 ? 419430400 : v_x1_u32r);
  return (long unsigned int)(v_x1_u32r >> 12);
}
