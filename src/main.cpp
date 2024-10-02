// Signal K & SensESP fan control 

#include "sensesp/sensors/analog_input.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp_app_builder.h"
#include <Adafruit_INA260.h>
#include <Adafruit_EMC2101.h>
#include <sensesp/transforms/moving_average.h>
#include <sensesp/net/http_server.h>
#include <WebServer.h>


using namespace sensesp;

// Create an instance of the sensor using its I2C interface
Adafruit_INA260 ina260 = Adafruit_INA260();   // Power measurement breakout board
Adafruit_EMC2101  emc2101;                    // Fan control breakout board

// Fan state variables
int currentFanSpeed = 0; // 0 = off, 1 = 50%, 2 = 70%, 3 = 100%
int SpeedSetting = currentFanSpeed;
const int fanSpeeds[] = {0,61,74,100};

////////////////////////////////////////////
//Here you put all the functions you will need to call within the setup()
////////////////////////////////////////////
float read_power_callback() { return (ina260.readPower() / 1000); }
float read_int_temp() { return (emc2101.getInternalTemperature()+ 273.15); }
float read_tach() { 
    int tachReading {};
    int tachTotal {};
    int tachNum {};
    for(int i=1;i<10;++i){
        delay(50);
        tachReading=emc2101.getFanRPM();
        tachTotal += tachReading;
        tachNum = i; 
    }
    return (tachTotal/tachNum);
    }

// Function to set fan speed
void setFanSpeed(int speedIndex) {
    if (speedIndex >= 0 && speedIndex < sizeof(fanSpeeds) / sizeof(fanSpeeds[0])) {
        emc2101.setDutyCycle(fanSpeeds[speedIndex]); // Set the fan speed using PWM
        ESP_LOGD(__FILE__,"Fan speed set to %d%%", (speedIndex + 1) * 50); // Print current speed (1 = 50%, etc.)
    }
}

// Toggle function to cycle through fan speeds
void cycleFanSpeed() {
    currentFanSpeed = (currentFanSpeed + 1) % (sizeof(fanSpeeds) / sizeof(fanSpeeds[0])); // Cycle through speeds
    setFanSpeed(currentFanSpeed);
}

// Create a WebServer instance
WebServer server(8081); // Create a web server on port 8081

// Create HTML content
const char* htmlPage = R"rawliteral(
<!DOCTYPE html>
<html>
<head>
    <meta charset="UTF-8">
    <title>Fan Speed Control</title>
    <style>
        body { font-family: Arial; }
        h1 { text-align: center; }
        .button { display: block; margin: 10px auto; padding: 10px 20px; font-size: 20px; }
        .status { text-align: center; font-size: 18px; margin: 10px; }
    </style>
</head>
<body>
    <h1>Fan Speed Control</h1>
    <div class="status">
        <p id="currentSpeed">Current Speed: 0 (Off)</p>
        <p id="currentRPM">Current RPM: 0</p>
        <p id="currentTemp">Temperature: 0°C</p>
        <p id="currentPower">Power: 0W</p>
    </div>
    <button class="button" onclick="setSpeed(0)">Turn Off</button>
    <button class="button" onclick="setSpeed(1)">50% Speed</button>
    <button class="button" onclick="setSpeed(2)">70% Speed</button>
    <button class="button" onclick="setSpeed(3)">100% Speed</button>

    <script>
        function setSpeed(speedIndex) {
            fetch('/setFanSpeed', {
                method: 'PUT',
                body: JSON.stringify(speedIndex), // Send the speed index
                headers: { 'Content-Type': 'application/json' }
            })
            .then(response => response.text()) // Get the response as text
            .then(data => console.log(data)); // Log the response for debugging
        }

        function updateStatus() {
            fetch('/status')
                .then(response => response.json()) // Expect JSON response
                .then(data => {
                    // Update HTML elements with values
                    document.getElementById('currentSpeed').innerText = "Current Speed: " + data.currentSpeed;
                    document.getElementById('currentRPM').innerText = "Current RPM: " + data.currentRPM;
                    document.getElementById('currentTemp').innerText = "Temperature: " + (data.currentTemp).toFixed(2) + " °C"; // Display in °C
                    document.getElementById('currentPower').innerText = "Power: " + data.currentPower + "W";
                });
        }

        // Update the status every second
        setInterval(updateStatus, 1000);
    </script>
</body>
</html>
)rawliteral";

// Handle the root request to show the HTML page
void handleRoot() {
    server.send(200, "text/html", htmlPage);
}
void handlePutFanSpeed() {
    if (server.method() == HTTP_PUT) {
        String body = server.arg("plain"); // Get the body of the PUT request
        body.trim(); // Trim whitespace from the body
        debugD("Raw body received: '%s'", body.c_str()); // Log raw body for debugging
        int newSpeed = body.toInt(); // Convert to integer

        // Prepare a response message
        String responseMessage;

        // Add the received value to the response
        responseMessage += "Received PUT request with requested speed: '" + body + "'\n";
        debugD("Conversion result: %d", newSpeed); // Log the integer result

        // Check if the new speed is within a valid range
        if (newSpeed >= 0 && newSpeed < sizeof(fanSpeeds) / sizeof(fanSpeeds[0])) {
            currentFanSpeed = newSpeed; // Update the current speed index
            setFanSpeed(currentFanSpeed); // Set the fan speed

            // Log updated information in response
            responseMessage += "Fan speed updated to index: " + String(currentFanSpeed) +
                               " (Duty Cycle: " + String(fanSpeeds[currentFanSpeed]) + "%)\n";

            server.send(200, "text/plain", responseMessage); // Send back success
        } else {
            responseMessage += "Invalid speed value: " + String(newSpeed) +
                               ". Must be between 0 and " + String(sizeof(fanSpeeds) / sizeof(fanSpeeds[0]) - 1) + ".\n";

            server.send(400, "text/plain", responseMessage); // Send back error for invalid speed
        }
    }
}

// Handle GET request for current status
void handleGetStatus() {
    // Prepare a response object in JSON format
    String response;
    response += "{";
    response += "\"currentSpeed\":" + String(currentFanSpeed) + ",";
    response += "\"currentRPM\":" + String(read_tach()) + ","; // Get current RPM
    response += "\"currentTemp\":" + String(read_int_temp() - 273.15) + ","; // Convert to Celsius
    response += "\"currentPower\":" + String(ina260.readPower() / 1000.0); // Get power in Watts
    response += "}"; // Close the JSON object

    server.send(200, "application/json", response); // Send JSON response
}


//////////////////////////////////////////////////////////////////
// The setup function performs one-time application initialization.
//////////////////////////////////////////////////////////////////

void setup() {
  SetupLogging();

  // Construct the global SensESPApp() object
  SensESPAppBuilder builder;
  sensesp_app = (&builder)
                    // Set a custom hostname for the app.
                    ->set_hostname("stb_fan_control")
                    // Optionally, hard-code the WiFi and Signal K server
                    // settings. This is normally not needed.
                    // ->set_wifi("KBL", "blacksmith")
                    //->set_sk_server("192.168.10.3", 80)
                    ->enable_ota("transport")
                    ->get_app();

  // Set up the button input for fan speed control
  const uint8_t kButtonPin = 35; // Use the appropriate GPIO pin for the button
  pinMode(kButtonPin, INPUT_PULLUP); // Pull-up resistor

  auto* button_input = new DigitalInputChange(kButtonPin, INPUT_PULLUP, CHANGE);
  button_input->connect_to(new LambdaConsumer<bool>(
      [=](bool input) { 
          delay(300);            // avoid button bounce
          if (input == LOW ) {   // Check if the button is pressed
              cycleFanSpeed();   // Cycle through fan speeds
          }
       }
  ));

  // Power Measurement ina260 set it up and read the power
  ina260.begin();

  unsigned int read_interval_ina = 10000;
  auto* fan_control_power =
      new RepeatSensor<float>(read_interval_ina, read_power_callback);
 
  // Fan Control Board emc2101 set up to read
  emc2101.begin();
  emc2101.enableTachInput(true);
  emc2101.setPWMDivisor(0);
  emc2101.setDutyCycle(0);
  unsigned int read_interval_emc = 1200;

  // Read the temperature
  auto* int_temp =
      new RepeatSensor<float>(read_interval_emc, read_int_temp );

  // Read the tachometer
  auto* fan_tach = 
      new RepeatSensor<float>(read_interval_emc, read_tach );
  
   // Create a Sensor for sharing the variable currentFanSpeed
auto* speedSettingSensor = new RepeatSensor<int>(500, []() { return currentFanSpeed; });
  // Create a Sensor for reading the duty cycle from the emc2101
auto* currentDutyCycle = new RepeatSensor<int>(500, []() { return (emc2101.getDutyCycle()); });

    // Register the handler for the root URL to serve the HTML page
    server.on("/", HTTP_GET, handleRoot);
    
    // Register the handler for the PUT request to change fan speed
    server.on("/setFanSpeed", HTTP_PUT, handlePutFanSpeed);

    // Register the handler for the GET request to fetch current status
    server.on("/status", HTTP_GET, handleGetStatus);

    // Start the server
    server.begin();
    debugD("HTTP server started on port 8081");


  ////////////////////////////////////////////
  //  These are all the readings being sent to SignalK
  ////////////////////////////////////////////

  fan_control_power
     ->connect_to(new MovingAverage(15, 1.0,
      "/Sensors/StbFan/Power/avg"))
     ->connect_to(new SKOutputFloat(
      "sensors.stb_fan_power",                 // Signal K path
      "/Sensors/StbFan/Power",                 // configuration path, used in the
                                               // web UI and for storing the
                                               // configuration
       new SKMetadata("W",                     // Define output units
                     "Stb Fan Watts")          // Value description
      ));



  int_temp->connect_to(new SKOutputFloat(
      "sensors.stb_fan_inttemp",               // Signal K path
      "/Sensors/StbFan/IntTemp",               // configuration path, used in the
                                               // web UI and for storing the
                                               // configuration
      new SKMetadata("K",                      // Define output units
                     "Stb Fan Internal Temp")  // Value description
      ));

  fan_tach
    //  ->connect_to(new MovingAverage(3, 1.0,
    //   "/Sensors/StbFan/RPM/avg"))
     ->connect_to(new SKOutputInt(
      "sensors.stb_fan_tach",                  // Signal K path
      "/Sensors/StbFan/RPM",                   // configuration path, used in the
                                               // web UI and for storing the
                                               // configuration
      new SKMetadata("RPM",                    // Define output units
                     "Stb Fan Tach")           // Value description
      ));

    speedSettingSensor
        ->connect_to(new SKOutputInt(
        "sensors.stb_fan_setspeed",              // Signal K path
        "/Sensors/StbFan/setspeed",              // configuration path, used in the
                                                 // web UI and for storing the
                                                 // configuration
        new SKMetadata("set",                    // Define output units
                        "Stb Fan Set Speed")     // Value description
        ));    
 
    currentDutyCycle
        ->connect_to(new SKOutputInt(
        "sensors.stb_fan_dutyCycle",              // Signal K path
        "/Sensors/StbFan/dutyCycle",              // configuration path, used in the
                                                  // web UI and for storing the
                                                  // configuration
        new SKMetadata("DS",                      // Define output units
                        "Stb Fan Get Duty Cycle") // Value description
        )); 

}

void loop() { 
    server.handleClient(); // Handle incoming client requests
    SensESPBaseApp::get_event_loop()->tick(); 
    }