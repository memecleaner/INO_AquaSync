#include <WiFi.h>
#include <FirebaseESP32.h>
#include <PZEM004Tv30.h>

#define WIFI_SSID "babyKoko"
#define WIFI_PASSWORD "babykoko18"

#define FIREBASE_HOST "aquasync-dda8c-default-rtdb.asia-southeast1.firebasedatabase.app" 
#define FIREBASE_AUTH "t7NDWdbpUYxUqXkzp4IfYqF88NJ60yJPR3F0cy1U"

#define VIBRATION_PIN 4
#define RELAY_SSR_PIN 23  
#define PZEM_RX_PIN 16    
#define PZEM_TX_PIN 17    

PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

FirebaseData firebaseDataRelay;
FirebaseData firebaseDataVibe;
FirebaseData firebaseDataPzem;
FirebaseConfig config;
FirebaseAuth auth;

unsigned long lastSensorSendTime = 0;
const unsigned long sensorInterval = 3000; 

void setup() {
  Serial.begin(115200);
  delay(2000);
  Serial.println("\n--- Menginisialisasi Sistem Integrasi Total AquaSync ---");

  pinMode(VIBRATION_PIN, INPUT);
  pinMode(RELAY_SSR_PIN, OUTPUT);
  
  digitalWrite(RELAY_SSR_PIN, LOW);
  Serial.println("Relay SSR di-set MATI untuk keamanan awal.");

  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan ke Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWi-Fi Terhubung!");

  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  // TIPS SIDANG: Buka baris di bawah ini jika kamu ingin mereset chip kWh PZEM ke angka 0 dari awal
  // pzem.resetEnergy(); 
  
  Serial.println("Sistem Integrasi AquaSync Siap Beroperasi!");
}

void loop() {
  if (Firebase.ready()) {
    // ==========================================================
    // LALUAN AKTUATOR: MEMATUHI KENDALI TOMBOL WEB SECARA REAL-TIME
    // ==========================================================
    bool buttonCondition = false;
    if (Firebase.getBool(firebaseDataRelay, "AquaSync/Control/Button_condition")) {
      if (firebaseDataRelay.dataType() == "boolean") {
        buttonCondition = firebaseDataRelay.boolData();
        if (buttonCondition == true) {
          digitalWrite(RELAY_SSR_PIN, HIGH); // Jalankan setrum ke SSR
        } else {
          digitalWrite(RELAY_SSR_PIN, LOW);  // Putus setrum ke SSR
        }
      }
    }

    // ==========================================================
    // LALUAN SENSOR: KIRIM DATA KE FIREBASE SETIAP 3 DETIK
    // ==========================================================
    if (millis() - lastSensorSendTime >= sensorInterval) {
      lastSensorSendTime = millis();
      
      Serial.println("\n=================[ LOG TIMEFRAME 3S ]=================");

      if (buttonCondition == true) {
        Serial.println("👥 KENDALI WEB: Button_condition TRUE -> SSR AKTIF");
      } else {
        Serial.println("👥 KENDALI WEB: Button_condition FALSE -> SSR MATI");
      }

      // Membaca Sensor Getar 801S (Active LOW)
      bool isVibrating = (digitalRead(VIBRATION_PIN) == LOW);

      if (isVibrating) {
        Serial.println("📳 SENSOR FISIK: Pompa Bergetar (Vibration: true)");
        Firebase.setBool(firebaseDataVibe, "AquaSync/Realtime_Status/Vibration", true);
      } else {
        Serial.println("📴 SENSOR FISIK: Pompa Diam (Vibration: false)");
        Firebase.setBool(firebaseDataVibe, "AquaSync/Realtime_Status/Vibration", false);
      }

      // Pembacaan Parameter Sensor Listrik PZEM
      float voltage = pzem.voltage();
      float current = pzem.current();
      float power   = pzem.power();
      float energy  = pzem.energy(); 

      if (!isnan(voltage)) {
        // Buat Paket JSON Data Listrik
        FirebaseJson jsonEnergy;
        jsonEnergy.set("Voltage", voltage);
        jsonEnergy.set("Current", current);
        jsonEnergy.set("Power", power);
        jsonEnergy.set("Energy", energy);
        
        // POS PERTAMA: Dikirim ke node Energy_Usage untuk dikalkulasi oleh Agen AI Python
        Firebase.setJSON(firebaseDataPzem, "AquaSync/Energy_Usage", jsonEnergy);
        
        // POS KEDUA (PERBAIKAN): Kirim pecahan datanya ke Realtime_Status agar meletup di layar web dashboard kamu
        Firebase.setFloat(firebaseDataPzem, "AquaSync/Realtime_Status/Voltage", voltage);
        Firebase.setFloat(firebaseDataPzem, "AquaSync/Realtime_Status/Current", current);
        Firebase.setFloat(firebaseDataPzem, "AquaSync/Realtime_Status/Power", power);
        Firebase.setFloat(firebaseDataPzem, "AquaSync/Realtime_Status/Energy", energy);
        
        // FIX FORMAT LOG: Menampilkan data kWh di serial monitor dengan benar
        Serial.printf("⚡ PZEM -> V: %.1f V | A: %.2f A | W: %.1f W | E: %.3f kWh\n", voltage, current, power, energy);
      } else {
        Serial.println("[ERROR] PZEM Tidak Terbaca. Cek Sambungan Kabel RX/TX.");
      }
      Serial.println("======================================================");
    }
  }
}

// #include <WiFi.h>
// #include <FirebaseESP32.h>
// #include <PZEM004Tv30.h>

// #define WIFI_SSID "babyKoko"
// #define WIFI_PASSWORD "babykoko18"

// #define FIREBASE_HOST "aquasync-dda8c-default-rtdb.asia-southeast1.firebasedatabase.app" 
// #define FIREBASE_AUTH "t7NDWdbpUYxUqXkzp4IfYqF88NJ60yJPR3F0cy1U"

// #define VIBRATION_PIN 4
// #define RELAY_SSR_PIN 23  
// #define PZEM_RX_PIN 16    
// #define PZEM_TX_PIN 17    

// PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// FirebaseData firebaseDataRelay;
// FirebaseData firebaseDataVibe;
// FirebaseData firebaseDataPzem;
// FirebaseConfig config;
// FirebaseAuth auth;

// unsigned long lastSensorSendTime = 0;
// const unsigned long sensorInterval = 3000; 

// void setup() {
//   Serial.begin(115200);
//   delay(2000);
//   Serial.println("\n--- Menginisialisasi Sistem Integrasi Total AquaSync ---");

//   pinMode(VIBRATION_PIN, INPUT);
//   pinMode(RELAY_SSR_PIN, OUTPUT);
  
//   digitalWrite(RELAY_SSR_PIN, LOW);
//   Serial.println("Relay SSR di-set MATI untuk keamanan awal.");

//   WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
//   Serial.print("Menghubungkan ke Wi-Fi");
//   while (WiFi.status() != WL_CONNECTED) {
//     Serial.print(".");
//     delay(500);
//   }
//   Serial.println("\nWi-Fi Terhubung!");

//   config.host = FIREBASE_HOST;
//   config.signer.tokens.legacy_token = FIREBASE_AUTH;
//   Firebase.begin(&config, &auth);
//   Firebase.reconnectWiFi(true);
  
//   Serial.println("Sistem Integrasi AquaSync Siap Beroperasi!");
// }

// void loop() {
//   if (Firebase.ready()) {
//     // ==========================================================
//     // LALUAN AKTUATOR: MEMATUHI KENDALI TOMBOL WEB SECARA REAL-TIME
//     // ==========================================================
//     bool buttonCondition = false;
//     if (Firebase.getBool(firebaseDataRelay, "AquaSync/Control/Button_condition")) {
//       if (firebaseDataRelay.dataType() == "boolean") {
//         buttonCondition = firebaseDataRelay.boolData();
//         if (buttonCondition == true) {
//           digitalWrite(RELAY_SSR_PIN, HIGH); // Jalankan setrum ke SSR
//         } else {
//           digitalWrite(RELAY_SSR_PIN, LOW);  // Putus setrum ke SSR
//         }
//       }
//     }

//     // ==========================================================
//     // LALUAN SENSOR: KIRIM DATA KE FIREBASE SETIAP 3 DETIK
//     // ==========================================================
//     if (millis() - lastSensorSendTime >= sensorInterval) {
//       lastSensorSendTime = millis();
      
//       Serial.println("\n=================[ LOG TIMEFRAME 3S ]=================");

//       if (buttonCondition == true) {
//         Serial.println("🔌 KENDALI WEB: Button_condition TRUE -> SSR AKTIF");
//       } else {
//         Serial.println("🔌 KENDALI WEB: Button_condition FALSE -> SSR MATI");
//       }

//       // Membaca Sensor Getar 801S (Active LOW)
//       bool isVibrating = (digitalRead(VIBRATION_PIN) == LOW);

//       if (isVibrating) {
//         Serial.println("🔥 SENSOR FISIK: Pompa Bergetar (Vibration: true)");
//         Firebase.setBool(firebaseDataVibe, "AquaSync/Realtime_Status/Vibration", true);
//       } else {
//         Serial.println("💤 SENSOR FISIK: Pompa Diam (Vibration: false)");
//         Firebase.setBool(firebaseDataVibe, "AquaSync/Realtime_Status/Vibration", false);
//       }

//       // Pembacaan PZEM
//       float voltage = pzem.voltage();
//       float current = pzem.current();
//       float power   = pzem.power();
//       float energy  = pzem.energy(); 

//       if (!isnan(voltage)) {
//         FirebaseJson jsonEnergy;
//         jsonEnergy.set("Voltage", voltage);
//         jsonEnergy.set("Current", current);
//         jsonEnergy.set("Power", power);
//         jsonEnergy.set("Energy", energy);
//         Firebase.setJSON(firebaseDataPzem, "AquaSync/Energy_Usage", jsonEnergy);
//         Serial.printf("⚡ PZEM -> V: %.1f V | A: %.2f A | W: %.1f W\n", voltage, current, power, energy);
//       } else {
//         Serial.println("[ERROR] PZEM Tidak Terbaca.");
//       }
//       Serial.println("======================================================");
//     }
//   }
// }