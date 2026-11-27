// Teste minimo da barra (74HC4067 + 12 QRE1113) numa ESP32-S3.
// Ligacao J2 -> S3: 1 VCC->3V3, 2 GND->GND, 4 SIG->GPIO4,
//                   5 S3->GPIO5, 6 S2->GPIO6, 7 S1->GPIO7, 8 S0->GPIO15
// Imprime os 16 canais do mux (C0..C15). Sensores em C2..C13;
// C0, C1, C14 e C15 vao ao GND e devem ler ~0.

const int S[4] = {15, 7, 6, 5};   // S0, S1, S2, S3
const int SIG = 4;

void setup() {
  Serial.begin(115200);
  for (int i = 0; i < 4; i++) pinMode(S[i], OUTPUT);
  analogReadResolution(12);
  analogSetAttenuation(ADC_11db);
}

void loop() {
  for (int c = 0; c < 16; c++) {
    for (int b = 0; b < 4; b++) digitalWrite(S[b], (c >> b) & 1);
    delayMicroseconds(100);
    analogRead(SIG);                       // descarta a primeira
    Serial.printf("%5d", analogRead(SIG));
  }
  Serial.println();
  delay(200);
}
