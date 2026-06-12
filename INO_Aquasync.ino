#include <WiFi.h>
#include <FirebaseESP32.h>
#include <PZEM004Tv30.h>

// 1. Pengaturan Wi-Fi Kosan / Rumah Kamu
#define WIFI_SSID "babyKoko"
#define WIFI_PASSWORD "babykoko18"

// 2. Pengaturan Kunci Firebase Kamu
#define FIREBASE_HOST "aquasync-dda8c-default-rtdb.asia-southeast1.firebasedatabase.app" 
#define FIREBASE_AUTH "t7NDWdbpUYxUqXkzp4IfYqF88NJ60yJPR3F0cy1U"

// 3. Definisikan PIN Hardware
#define VIBRATION_PIN 4
#define RELAY_SSR_PIN 23  // PIN GPIO 25 untuk mengendalikan Relay SSR pompa
#define PZEM_RX_PIN 16    // PIN RX2 ESP32 terhubung ke TX PZEM
#define PZEM_TX_PIN 17    // PIN TX2 ESP32 terhubung ke RX PZEM

// Inisialisasi Hardware PZEM menggunakan Serial2 Hardware bawaan ESP32
PZEM004Tv30 pzem(Serial2, PZEM_RX_PIN, PZEM_TX_PIN);

// Inisialisasi Object Firebase
FirebaseData firebaseDataRelay;
FirebaseData firebaseDataVibe;
FirebaseData firebaseDataPzem;
FirebaseConfig config;
FirebaseAuth auth;

// Manajemen Waktu Sampling Non-Blocking (Interval Kompak 3 Detik)
unsigned long lastSensorSendTime = 0;
const unsigned long sensorInterval = 3000; 

void setup() {
  Serial.begin(115200);
  delay(5000);
  Serial.println("\n--- Menginisialisasi Sistem Integrasi Total AquaSync ---");

  // Set PIN Mode Hardware
  pinMode(VIBRATION_PIN, INPUT_PULLUP);
  pinMode(RELAY_SSR_PIN, OUTPUT);
  
  // Keamanan Awal: Pastikan saat pertama kali menyala, pompa dalam kondisi MATI
  digitalWrite(RELAY_SSR_PIN, LOW);
  Serial.println("Relay SSR di-set MATI untuk keamanan awal.");

  // Jalankan Koneksi Wi-Fi
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);
  Serial.print("Menghubungkan ke Wi-Fi");
  while (WiFi.status() != WL_CONNECTED) {
    Serial.print(".");
    delay(500);
  }
  Serial.println("\nWi-Fi Terhubung!");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());

  // Konfigurasi & Mulai Firebase
  config.host = FIREBASE_HOST;
  config.signer.tokens.legacy_token = FIREBASE_AUTH;
  Firebase.begin(&config, &auth);
  Firebase.reconnectWiFi(true);
  
  Serial.println("Sistem Integrasi AquaSync (SSR + 801S + PZEM) Siap Tempur!");
}

void loop() {
  // Pastikan Firebase Siap Beroperasi
  if (Firebase.ready()) {

    // ==========================================================
    // LALUAN AKTUATOR: SAKELAR RELAY TETAP BERJALAN INSTAN (ANTI-LAG)
    // ==========================================================
    // Pengambilan data ke cloud tetap harus cepat agar pompa langsung bereaksi saat tombol web diklik,
    // TAPI cetak tulisan ke Serial Monitor-nya kita sembunyikan dulu dari loop utama.
    bool buttonCondition = false;
    if (Firebase.getBool(firebaseDataRelay, "AquaSync/Control/Button_condition")) {
      if (firebaseDataRelay.dataType() == "boolean") {
        buttonCondition = firebaseDataRelay.boolData();
        if (buttonCondition == true) {
          digitalWrite(RELAY_SSR_PIN, HIGH); // Pompa Diberi Setrum
        } else {
          digitalWrite(RELAY_SSR_PIN, LOW);  // Pompa Diputus
        }
      }
    }

    // ==========================================================
    // LALUAN BLOK LOG OUTPUT & SENSOR: DIEKSEKUSI BERSAMA TIAP 3 DETIK
    // ==========================================================
    if (millis() - lastSensorSendTime >= sensorInterval) {
      lastSensorSendTime = millis();
      
      Serial.println("\n=================[ LOG TIMEFRAME 3S ]=================");

      // --- 1. Cetak Status Log Kendali Tombol Web ---
      if (buttonCondition == true) {
        Serial.println("🔌 KENDALI WEB: Button_condition TRUE -> Relay SSR AKTIF (Pompa Menyala)");
      } else {
        Serial.println("🔌 KENDALI WEB: Button_condition FALSE -> Relay SSR NON-AKTIF (Pompa Mati)");
      }

      // --- 2. Pembacaan dan Pengiriman Data Sensor Getaran 801S ---
      bool isVibrating = false;
      unsigned long startCheck = millis();
      
      // Sampling cepat 500ms untuk menangkap kedipan sinyal LOW dari tabung emas 801S
      while (millis() - startCheck < 500) {
        if (digitalRead(VIBRATION_PIN) == HIGH) { 
          isVibrating = true; 
          break;
        }
      }

      // Kirim feedback boolean langsung ke Firebase
      if (isVibrating) {
        Serial.println("🔥 SENSOR FISIK: Pompa Bergetar (Vibration: true)");
        Firebase.setBool(firebaseDataVibe, "AquaSync/Realtime_Status/Vibration", true);
      } else {
        Serial.println("💤 SENSOR FISIK: Pompa Diam (Vibration: false)");
        Firebase.setBool(firebaseDataVibe, "AquaSync/Realtime_Status/Vibration", false);
      }

      // --- 3. Pembacaan dan Pengiriman Data Energi Elektrikal PZEM-004T ---
      float voltage = pzem.voltage();
      float current = pzem.current();
      float power   = pzem.power();
      float energy  = pzem.energy(); 

      if (isnan(voltage)) {
        Serial.println("[ERROR] Gagal membaca data PZEM! Cek sambungan kabel AC / CT Ring.");
      } else {
        Serial.println("⚡ KELISTRIKAN : Mengirim Paket Data ke AquaSync/Energy_Usage...");
        Serial.printf("               V: %.1f V | A: %.2f A | W: %.1f W | Wh: %.2f kWh\n", voltage, current, power, energy);

        FirebaseJson jsonEnergy;
        jsonEnergy.set("Voltage", voltage);
        jsonEnergy.set("Current", current);
        jsonEnergy.set("Power", power);
        jsonEnergy.set("Energy", energy);

        if (Firebase.setJSON(firebaseDataPzem, "AquaSync/Energy_Usage", jsonEnergy)) {
          Serial.println("👉 CLOUD STATUS: JSON Kelistrikan Sukses Terunggah!");
        } else {
          Serial.printf("👉 CLOUD STATUS ERROR: %s\n", firebaseDataPzem.errorReason().c_str());
        }
      }
      Serial.println("======================================================");
    }

  } else {
    static unsigned long errorLogTime = 0;
    if (millis() - errorLogTime > 5000) {
      Serial.println("Menunggu kesiapan koneksi Firebase Cloud...");
      errorLogTime = millis();
    }
  }
}