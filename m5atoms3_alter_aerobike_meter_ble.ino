#include "Wire.h"
#include "math.h"
#include "M5UNIT_DIGI_CLOCK.h"
#include <M5AtomS3.h> 
#include <BLEDevice.h>
#include <BLE2902.h>


// #define SDA 38
// #define SCL 39
#define SDA 2
#define SCL 1
#define ADD 0x30

#define INPUT_PIN0 5

// BLE UUID
#define SERVICE_UUID "36ecea7a-5d39-11ee-8c99-0242ac120002"
#define CHARACTERISTIC_UUID "36eced54-5d39-11ee-8c99-0242ac120002"
#define DURATION_CHARACTERISTIC_UUID "0718ad22-7d7b-11ee-b962-0242ac120002"
#define DISTANCE_CHARACTERISTIC_UUID "4a9f9920-7d7b-11ee-b962-0242ac120002"
#define SPEED_CHARACTERISTIC_UUID "786e7b82-7d7b-11ee-b962-0242ac120002"
#define AVERAGE_SPEED_CHARACTERISTIC_UUID "7f765d3c-7d7b-11ee-b962-0242ac120002"
#define BURNED_CALORIES_CHARACTERISTIC_UUID "848c4462-7d7b-11ee-b962-0242ac120002"

enum display_mode {
  Time,
  Speed,
  Calorie,
  Distance
};

enum display_mode increment_mode(enum display_mode current_mode) {
    return (enum display_mode)((current_mode + 1) % 4);
}

M5UNIT_DIGI_CLOCK Digiclock;

char time_buff[] = "8.8.:8.8.";
char speed_buff[] = "1.1.:1.1.";
char calorie_buff[] = "2.2.:2.2.";
char distance_buff[] = "3.3.:3.3.";

bool moving = false;

float start_time = 0.0;
float last_low_time = 0.0;
float total_time = 0.0;
float last_time = 0.0;

bool last_state = HIGH;
bool current_state = HIGH;

int low_count = 0;
float total_calorie = 0.0;
float total_distance = 0.0;

float velocity_kmps = 0.0;

float mass = 66.0;
float torque = 11.3;

// 自転車の抵抗計算のパラメータ
float myHeight = 171;

float bicMass = 15;
float bagMass = 0;
float tireDia = 700;
float airTemp = 25;
float windVelocity = 0;
float roadSlope = 0;
float myInclin = 80;

float road = 1.0;
float resist=0.008;
float w = 38;  
float dress=1.1;

float BMI = mass/ pow(0.01*myHeight,2);
float bodyIndex = sqrt(BMI/22);
float heightIndex = myHeight/170;
float trunkAngle = M_PI *myInclin/180;

float faceW = 0.14;
float shoulderW = 0.45;
float shoulderT = 0.12;
float trunkW = 0.4;
float arm2W = 0.16;
float leg2W = 0.24;

float faceA = 0.12*0.01*myHeight*heightIndex*faceW;
float shoulderA = heightIndex*shoulderW*heightIndex*shoulderT;
float trunkA = 0.4*0.01*myHeight*heightIndex*trunkW;
float armA = 0.32*0.01*myHeight*heightIndex*arm2W;
float legA = 0.45*0.01*myHeight*heightIndex*leg2W;
float bodyA = 1*faceA + trunkA*sin(trunkAngle) + shoulderA*cos(trunkAngle) + armA*sin(trunkAngle) + 1*legA;
float myArea = bodyA*bodyIndex*dress;
float bicArea = tireDia*0.001*w*0.001;
float area = 1*myArea + 1*bicArea;

float totalMass = mass + bicMass + bagMass;
float a = 0.30784 * area;
float c = 0.0816192 * totalMass;

enum display_mode mode = Time;

BLECharacteristic *pCharacteristic;
BLECharacteristic *pDurationCharacteristic;
BLECharacteristic *pDistanceCharacteristic;
BLECharacteristic *pSpeedCharacteristic;
BLECharacteristic *pAverageSpeedCharacteristic;
BLECharacteristic *pBurnCaloriesCharacteristic;

bool deviceConnected = false;
bool needRestart = false;

// BLE calback
class MyServerCallbacks: public BLEServerCallbacks {
    void onConnect(BLEServer* pServer) {
      USBSerial.println("connect");
      delay(2000);
      deviceConnected = true;
    };
    void onDisconnect(BLEServer* pServer) {
      USBSerial.println("disconnect");
      deviceConnected = false;
      needRestart = true;
    }
};

class MyCallbacks: public BLECharacteristicCallbacks {
  void onRead(BLECharacteristic *pCharacteristic) {
    USBSerial.printf("read");
//    std::string value = pCharacteristic->getValue();
//    Serial.println(value.c_str());
  }
  void onWrite(BLECharacteristic *pCharacteristic) {
    USBSerial.println("write");
  }
};

void startService(BLEServer *pServer)
{
  BLEService *pService = pServer->createService(SERVICE_UUID);

  pServer->setCallbacks(new MyServerCallbacks());
  pCharacteristic = pService->createCharacteristic(
      CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ);

  pCharacteristic->addDescriptor(new BLE2902()); // Descriptorを定義しておかないとClient側でエラーログが出力される
  String value = String(random(0, 10000));
  pCharacteristic->setValue(value.c_str());
  pCharacteristic->setCallbacks(new MyCallbacks());
  
  pDurationCharacteristic = pService->createCharacteristic(
      DURATION_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ);
  pDurationCharacteristic->addDescriptor(new BLE2902());

  pDistanceCharacteristic = pService->createCharacteristic(
      DISTANCE_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ);

  pSpeedCharacteristic = pService->createCharacteristic(
      SPEED_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ);

  pAverageSpeedCharacteristic = pService->createCharacteristic(
      AVERAGE_SPEED_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ);

  pBurnCaloriesCharacteristic = pService->createCharacteristic(
      BURNED_CALORIES_CHARACTERISTIC_UUID,
      BLECharacteristic::PROPERTY_READ);

  pService->start();
}

void startAdvertising()
{
  BLEAdvertising *pAdvertising = BLEDevice::getAdvertising();
  pAdvertising->addServiceUUID(SERVICE_UUID);
  pAdvertising->setScanResponse(true); // trueにしないと、Advertising DataにService UUIDが含まれない。
  // minIntervalはデフォルトの20でとくに問題なさそうなため、setMinPreferredは省略
  BLEDevice::startAdvertising();
}


void showDisplay(void* pvParameters){
  while(1){
  if(mode == Time){
    Digiclock.setString(time_buff);
  } else if(mode == Speed) {
    Digiclock.setString(speed_buff);
  } else if(mode == Calorie) {
    Digiclock.setString(calorie_buff);
  } else if(mode == Distance) {
    Digiclock.setString(distance_buff);
  } 
  delay(500);
  }
}

void bleUpdate(void* pvParameters){
  while(1){

    if (deviceConnected){
      M5.dis.drawpix(0xff0000);
      M5.dis.show();
    }
    else {
      M5.dis.drawpix(0x00ff00);
      M5.dis.show();
    }


    if (needRestart) {
      startAdvertising();
      USBSerial.println("Advertising!");
      needRestart = false;
    }

    String duration = String(total_time /1000);
    String distance = String(total_distance);
    String speed    = String(velocity_kmps);
    String a_speed  = String(total_distance / (total_time /1000/3600 ));
    String calories = String(total_calorie);

    pDurationCharacteristic->setValue(duration.c_str());
    pDistanceCharacteristic->setValue(distance.c_str());
    pSpeedCharacteristic->setValue(speed.c_str());
    pAverageSpeedCharacteristic->setValue(a_speed.c_str());
    pBurnCaloriesCharacteristic->setValue(calories.c_str());
    
    delay(300);
  }

}
void setup() 
{
    delay(2000);
    M5.begin(false, false, true, true);
    USBSerial.begin(115200);
    M5.dis.drawpix(0x0000ff);
    M5.dis.show();
    Wire.begin(SDA, SCL);

    pinMode(INPUT_PIN0, INPUT_PULLUP);

    /* Digital clock init */
    if (Digiclock.begin(&Wire, SDA, SCL, ADD)) 
    {
        USBSerial.println("Digital clock init successful");
    } 
    else 
    {
        USBSerial.println("Digital clock init error");
        while (1);
    }
    sprintf(time_buff, "8.8:8.8");
    Digiclock.setString(time_buff);
 //   Digiclock.setBrightness(9);
    delay(2000);
    sprintf(time_buff, "00:00");
    Digiclock.setString(time_buff);

    last_state = digitalRead(INPUT_PIN0);

    BLEDevice::init("M5AtomS3 Aerobike BLE");
    BLEServer *pServer = BLEDevice::createServer();
    startService(pServer);
    startAdvertising();


    xTaskCreatePinnedToCore(showDisplay, "7seg_task", 4096, NULL, 1, NULL, 0);
    xTaskCreatePinnedToCore(bleUpdate, "ble_task", 4096, NULL, 2, NULL, 0);

}

void loop() 
{
  float now = millis();
  current_state = digitalRead(INPUT_PIN0);
  
  M5.update();

  if (M5.Btn.wasReleased() || M5.Btn.pressedFor(1000)) {
    USBSerial.print('A');
    mode = increment_mode(mode);
  }

// シャットダウン
if ( !moving &&  ((now - last_low_time)/1000 > 180.0) ) {
  Digiclock.setBrightness(0);
  USBSerial.println("enter sleep!");
  esp_deep_sleep_start();  
}

// 始動処理
  if (! moving) {
    velocity_kmps = 0.0;
    if (current_state == last_state) {
       last_time = now;
       last_state = current_state;

//       showDisplay();
 //      USBSerial.println("skip!");
       return;
    }
    else {
      moving = true;
      last_low_time = now;
      USBSerial.println("started!");
    }
   }

// 停止処理
if ( moving && (last_low_time != 0) && ((now - last_low_time)/1000 > 3.0) ) {
  moving = false;
  last_state = current_state;
  snprintf(speed_buff,sizeof(speed_buff),"0.0K.");
  USBSerial.println("stopped!");
  return;
}



//　センサーの反応があった場合の処理
if (last_state == HIGH && current_state == LOW) {

    // 初回の反応は計算させない
    if( now == last_low_time ) {
      last_state = current_state;
      last_time = now;
      last_low_time = now;
      return;
    }

  low_count ++;
  float erasped = (now - last_low_time)/1000;
  float watt = torque * 2 * 3.14 / erasped;

  // カロリー計算
  total_calorie += (torque * 2 * 3.14 / erasped * 12.0 + mass * 3.5) / 3.5 *1.05 * erasped / 3600;
  if(total_calorie <100.0){
      snprintf(calorie_buff,sizeof(calorie_buff),"%.1fC", total_calorie);
  } else{
      snprintf(calorie_buff,sizeof(calorie_buff),"%.0fC", total_calorie);
  }

  //速度計算
  float d = watt;
  float k1 = d/ (2 * a) + (sqrt(3.0*(27.0* pow(a*d, 2) + 4.0* a * pow(c,3)) )) / (18.0 * pow(a,2));
  float k2 = d/ (2 * a) - (sqrt(3.0*(27.0* pow(a*d, 2) + 4.0* a * pow(c,3)) )) / (18.0 * pow(a,2));
  float k2_sign;
  if(k2 < 0){
    k2_sign = -1.0;
  } 
  else {
     k2_sign = 1.0;
  }

  float velocity_mps = pow(k1, 1.0/3)  + pow(k2*k2_sign, 1.0/3) * k2_sign;
  velocity_kmps = velocity_mps * 3600 / 1000;

  snprintf(speed_buff,sizeof(speed_buff),"%.1fK.", velocity_kmps);

  // 距離
  total_distance += velocity_mps * erasped /1000;
  snprintf(distance_buff,sizeof(distance_buff),"%.1fK", total_distance);

  last_low_time = now;
  USBSerial.println(low_count);
    
  }


  total_time += now - last_time;

   int min = int(int(total_time /1000)/60);
   int sec = int(int(total_time /1000)%60);
  

  sprintf(time_buff, "%d%d:%d%d", min/10, min%10, sec/10, sec%10);

//  showDisplay();

  last_state = current_state;
  last_time = now;
  delay(10);
}