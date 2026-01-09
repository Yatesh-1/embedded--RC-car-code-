#include <WiFi.h>
#include <WebServer.h>

// ============================================
// PIN DEFINITIONS
// ============================================
#define ENA 4    // Left Motor PWM
#define IN1 2    // Left Motor Forward
#define IN2 18   // Left Motor Reverse
#define ENB 5    // Right Motor PWM
#define IN3 19   // Right Motor Forward
#define IN4 21   // Right Motor Reverse

#define TRIG_PIN 12  // Ultrasonic Trigger
#define ECHO_PIN 13  // Ultrasonic Echo
#define LED_PIN  14  // Status LED

// ============================================
// GLOBAL OBJECTS
// ============================================
WebServer server(80);

// ============================================
// GLOBAL VARIABLES
// ============================================
int distanceCM = 999;
unsigned long lastUltrasonicRead = 0;
unsigned long lastJoystickCommand = 0;
bool obstacleDetected = false;

// ============================================
// SETUP FUNCTION
// ============================================
void setup() {
  Serial.begin(115200);
  delay(1000);
  
  initializePins();
  initializeMotors();
  initializeWiFi();
  initializeWebServer();
  
  Serial.println("✅ RC CAR - ALL SYSTEMS READY!");
}

// ============================================
// MAIN LOOP
// ============================================
void loop() {
  server.handleClient();
  handleAutoStop();
  handleUltrasonicSensor();
}

// ============================================
// INITIALIZATION FUNCTIONS
// ============================================
void initializePins() {
  // Motor Pins
  pinMode(ENA, OUTPUT); pinMode(IN1, OUTPUT); pinMode(IN2, OUTPUT);
  pinMode(ENB, OUTPUT); pinMode(IN3, OUTPUT); pinMode(IN4, OUTPUT);
  
  // Sensor Pins
  pinMode(TRIG_PIN, OUTPUT); pinMode(ECHO_PIN, INPUT); pinMode(LED_PIN, OUTPUT);
}

void initializeMotors() {
  stopMotors();
  digitalWrite(LED_PIN, LOW);
}

void initializeWiFi() {
  WiFi.mode(WIFI_AP);
  WiFi.softAP("RC-Car", "", 1);  // Channel 1 for stability
  
  Serial.print("📶 WiFi AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.println("📱 Connect phone → RC-Car WiFi → 192.168.4.1");
}

void initializeWebServer() {
  server.on("/", handleMainPage);
  server.on("/joy", handleJoystick);
  server.on("/dist", handleDistance);
  server.begin();
}

// ============================================
// LOOP HELPER FUNCTIONS
// ============================================
void handleAutoStop() {
  if (millis() - lastJoystickCommand > 500) {
    stopMotors();
  }
}

void handleUltrasonicSensor() {
  if (millis() - lastUltrasonicRead > 150) {
    lastUltrasonicRead = millis();
    distanceCM = readUltrasonicDistance();
    
    Serial.print("📏 Distance: ");
    Serial.print(distanceCM);
    Serial.println(" cm");
    
    updateObstacleStatus();
  }
}

void updateObstacleStatus() {
  if (distanceCM <= 40 && distanceCM > 0) {
    obstacleDetected = true;
    digitalWrite(LED_PIN, HIGH);
    Serial.println("🚨 OBSTACLE DETECTED! Motors BLOCKED!");
  } else {
    obstacleDetected = false;
    digitalWrite(LED_PIN, LOW);
    Serial.println("✅ PATH CLEAR! Motors READY!");
  }
}

// ============================================
// MOTOR CONTROL FUNCTIONS
// ============================================
void motorControl(int leftSpeed, int rightSpeed) {
  leftSpeed = constrain(leftSpeed, -255, 255);
  rightSpeed = constrain(rightSpeed, -255, 255);
  
  Serial.print("⚙️ Left: "); Serial.print(leftSpeed);
  Serial.print(" | Right: "); Serial.println(rightSpeed);
  
  controlLeftMotor(leftSpeed);
  controlRightMotor(rightSpeed);
}

void controlLeftMotor(int speed) {
  if (speed > 0) {
    digitalWrite(IN1, HIGH); digitalWrite(IN2, LOW);
    analogWrite(ENA, speed);
  } else if (speed < 0) {
    digitalWrite(IN1, LOW); digitalWrite(IN2, HIGH);
    analogWrite(ENA, -speed);
  } else {
    digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
    analogWrite(ENA, 0);
  }
}

void controlRightMotor(int speed) {
  if (speed > 0) {
    digitalWrite(IN3, HIGH); digitalWrite(IN4, LOW);
    analogWrite(ENB, speed);
  } else if (speed < 0) {
    digitalWrite(IN3, LOW); digitalWrite(IN4, HIGH);
    analogWrite(ENB, -speed);
  } else {
    digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
    analogWrite(ENB, 0);
  }
}

void stopMotors() {
  analogWrite(ENA, 0);
  analogWrite(ENB, 0);
  digitalWrite(IN1, LOW); digitalWrite(IN2, LOW);
  digitalWrite(IN3, LOW); digitalWrite(IN4, LOW);
}

// ============================================
// SENSOR FUNCTIONS
// ============================================
int readUltrasonicDistance() {
  digitalWrite(TRIG_PIN, LOW);  delayMicroseconds(2);
  digitalWrite(TRIG_PIN, HIGH); delayMicroseconds(10);
  digitalWrite(TRIG_PIN, LOW);
  
  long duration = pulseIn(ECHO_PIN, HIGH, 30000);
  return (duration == 0) ? 999 : (duration * 0.034) / 2;
}

// ============================================
// WEB SERVER HANDLERS
// ============================================
void handleMainPage() {
  server.send(200, "text/html", getMainPageHTML());
}

void handleJoystick() {
  int x = server.arg("x").toInt();
  int y = server.arg("y").toInt();
  
  processJoystickInput(x, y);
  server.send(200, "text/plain", "OK");
}

void handleDistance() {
  server.send(200, "text/plain", String(distanceCM));
}

// ============================================
// JOYSTICK PROCESSING
// ============================================
void processJoystickInput(int x, int y) {
  lastJoystickCommand = millis();
  
  Serial.print("🎮 X: "); Serial.print(x);
  Serial.print(" | Y: "); Serial.println(y);
  
  if (!obstacleDetected) {
    calculateMotorSpeeds(x, y);
  } else {
    stopMotors();
    Serial.println("❌ OBSTACLE BLOCK!");
  }
}

void calculateMotorSpeeds(int x, int y) {
  int forwardSpeed = map(y, -100, 100, -200, 200);
  int turnSpeed = map(x, -100, 100, -120, 120);
  
  int leftSpeed = forwardSpeed + turnSpeed;
  int rightSpeed = forwardSpeed - turnSpeed;
  
  if (abs(leftSpeed) > 30 || abs(rightSpeed) > 30) {
    motorControl(leftSpeed, rightSpeed);
    Serial.println("✅ TANK STEERING ACTIVE!");
  } else {
    stopMotors();
  }
}

// ============================================
// PERFECTLY STRUCTURED HTML
// ============================================
String getMainPageHTML() {
  return R"html(
<!DOCTYPE html>
<html lang="en">
<head>
  <meta charset="UTF-8">
  <meta name="viewport" content="width=device-width, initial-scale=1.0">
  <title>RC Car Dashboard</title>
  <style>
    /* GLOBAL STYLES */
    * {box-sizing:border-box;margin:0;padding:0;font-family:"Segoe UI",Arial,sans-serif;}
    
    body {
      background: 
        radial-gradient(circle at 20% 80%, rgba(120,119,198,0.3) 0%, transparent 50%),
        radial-gradient(circle at 80% 20%, rgba(255,119,198,0.3) 0%, transparent 50%),
        radial-gradient(circle at 40% 40%, rgba(120,219,255,0.3) 0%, transparent 50%),
        linear-gradient(135deg, #0f0f0f 0%, #1a1a2a 50%, #16213e 100%);
      color:#ffffff;height:100vh;display:flex;flex-direction:column;position:relative;overflow:hidden;
    }
    
    /* ANIMATIONS */
    body::before {
      content:'';position:absolute;top:0;left:0;right:0;bottom:0;
      background-image:radial-gradient(2px 2px at 20px 30px,#eee,transparent),
                      radial-gradient(2px 2px at 40px 70px,rgba(255,255,255,0.8),transparent),
                      radial-gradient(1px 1px at 90px 40px,#fff,transparent);
      background-repeat:repeat;background-size:200px 100px;animation:sparkle 20s linear infinite;opacity:0.1;
    }
    @keyframes sparkle{0%{transform:translateX(0) translateY(0);}100%{transform:translateX(-200px) translateY(-100px);}}
    
    /* HEADER */
    header{padding:15px;text-align:center;border-bottom:1px solid rgba(255,255,255,0.1);background:rgba(0,0,0,0.3);backdrop-filter:blur(10px);}
    header h1{font-size:22px;letter-spacing:2px;text-shadow:0 0 10px rgba(255,255,255,0.5);}
    
    /* STATUS */
    .status-box{padding:12px;text-align:center;border-radius:10px;font-weight:bold;font-size:14px;margin:0 20px 10px 20px;}
    .connected{background:rgba(46,204,113,0.2);border:2px solid #2ecc71;color:#2ecc71;box-shadow:0 0 15px rgba(46,204,113,0.4);}
    .disconnected{background:rgba(231,76,60,0.2);border:2px solid #e74c3c;color:#e74c3c;box-shadow:0 0 15px rgba(231,76,60,0.3);}
    
    /* MAIN LAYOUT */
    .main{flex:1;display:grid;grid-template-columns:1fr 1fr;gap:20px;padding:20px;}
    .left{display:flex;justify-content:center;align-items:center;}
    
    /* JOYSTICK */
    #joystick{width:300px;height:300px;border:3px solid rgba(255,255,255,0.6);border-radius:50%;position:relative;touch-action:none;background:rgba(255,255,255,0.05);box-shadow:0 0 30px rgba(255,255,255,0.1);}
    #knob{width:70px;height:70px;background:linear-gradient(145deg,#ffffff,#e0e0e0);border-radius:50%;position:absolute;top:50%;left:50%;transform:translate(-50%,-50%);box-shadow:0 8px 25px rgba(0,0,0,0.4);}
    
    /* RIGHT PANEL */
    .right{display:flex;flex-direction:column;gap:20px;}
    .card{border:1px solid rgba(255,255,255,0.3);padding:20px;text-align:center;border-radius:15px;background:rgba(0,0,0,0.4);backdrop-filter:blur(10px);}
    .card h2{font-size:16px;margin-bottom:10px;letter-spacing:1px;opacity:0.8;}
    .distance{font-size:36px;font-weight:bold;text-shadow:0 0 10px currentColor;}
    .danger{color:#ff4757;}
    
    /* DIRECTION BUTTONS */
    .direction{display:grid;grid-template-columns:1fr 1fr;gap:15px;}
    .dir-box{border:2px solid rgba(255,255,255,0.3);padding:20px;text-align:center;font-size:18px;font-weight:bold;border-radius:12px;background:rgba(0,0,0,0.3);transition:all 0.2s ease;cursor:pointer;}
    .dir-box:hover{border-color:rgba(255,255,255,0.6);box-shadow:0 5px 15px rgba(255,255,255,0.1);}
    .active{background:linear-gradient(145deg,#ffffff,#e0e0e0) !important;color:#000 !important;box-shadow:0 0 20px rgba(255,255,255,0.6);transform:scale(1.05);}
    
    /* FOOTER */
    footer{border-top:1px solid rgba(255,255,255,0.1);text-align:center;padding:10px;font-size:14px;letter-spacing:2px;background:rgba(0,0,0,0.3);backdrop-filter:blur(10px);opacity:0.8;}
    
    /* RESPONSIVE */
    @media (max-width:768px){.main{grid-template-columns:1fr;padding:10px;}#joystick{width:280px;height:280px;}}
  </style>
</head>
<body>
  <header><h1>🚗 RC CAR DASHBOARD</h1></header>
  <div class="status-box connected" id="statusBox">✅ ESP32 CONNECTED</div>
  <div class="main">
    <div class="left"><div id="joystick"><div id="knob"></div></div></div>
    <div class="right">
      <div class="card"><h2>ULTRASONIC SENSOR</h2><div class="distance" id="distanceValue">-- cm</div></div>
      <div class="direction">
        <div class="dir-box" id="dirForward">⬆️ FORWARD</div>
        <div class="dir-box" id="dirBackward">⬇️ BACKWARD</div>
        <div class="dir-box" id="dirLeft">⬅️ LEFT</div>
        <div class="dir-box" id="dirRight">➡️ RIGHT</div>
      </div>
    </div>
  </div>
  <footer>RC CAR v2.0 - PROFESSIONAL</footer>
  
  <script>
    let joystick=document.getElementById("joystick"),knob=document.getElementById("knob"),dragging=false,center=150,maxRadius=100;
    joystick.addEventListener("pointerdown",e=>{dragging=true;move(e);});
    document.addEventListener("pointermove",e=>dragging&&move(e));
    document.addEventListener("pointerup",()=>{dragging=false;knob.style.transform="translate(-50%,-50%)";send(0,0);setDirection("STOP");});
    function move(e){let rect=joystick.getBoundingClientRect(),x=e.clientX-rect.left-center,y=e.clientY-rect.top-center,d=Math.sqrt(x*x+y*y);if(d>maxRadius){x=x/d*maxRadius;y=y/d*maxRadius;}knob.style.transform=`translate(${x}px,${y}px)`;send(x,y);detectDirection(x,y);}
    function send(x,y){fetch(`/joy?x=${Math.round(x)}&y=${Math.round(y)}`);}
    function clearDir(){document.querySelectorAll(".dir-box").forEach(b=>b.classList.remove("active"));}
    function setDirection(dir){clearDir();if(dir==="FORWARD")document.getElementById("dirForward").classList.add("active");if(dir==="BACKWARD")document.getElementById("dirBackward").classList.add("active");if(dir==="LEFT")document.getElementById("dirLeft").classList.add("active");if(dir==="RIGHT")document.getElementById("dirRight").classList.add("active");}
    function detectDirection(x,y){if(Math.abs(y)>Math.abs(x)){y<0?setDirection("FORWARD"):setDirection("BACKWARD");}else{x<0?setDirection("LEFT"):setDirection("RIGHT");}}
    setInterval(()=>{fetch("/dist").then(r=>r.text()).then(d=>{let distanceValue=document.getElementById("distanceValue");distanceValue.innerText=d+" cm";if(parseInt(d)<=40&&d!=="999"){distanceValue.classList.add("danger");}else{distanceValue.classList.remove("danger");}}).catch(()=>{document.getElementById("statusBox").className="status-box disconnected";document.getElementById("statusBox").innerText="❌ ESP32 DISCONNECTED";});},300);
  </script>
</body>
</html>
)html";
}
