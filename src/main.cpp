// Signal K application template file.
//
// This application demonstrates core SensESP concepts in a very
// concise manner. You can build and upload the application as is
// and observe the value changes on the serial port monitor.
//
// You can use this source file as a basis for your own projects.
// Remove the parts that are not relevant to you, and add your own code
// for external hardware libraries.

#include "sensesp/sensors/analog_input.h"
#include "sensesp/sensors/digital_input.h"
#include "sensesp/sensors/sensor.h"
#include "sensesp/signalk/signalk_output.h"
#include "sensesp/system/lambda_consumer.h"
#include "sensesp_app_builder.h"
#include <Adafruit_INA260.h>
#include <Adafruit_EMC2101.h>
#include <sensesp/transforms/moving_average.h>


using namespace sensesp;

// Create an instance of the sensor using its I2C interface
Adafruit_INA260 ina260 = Adafruit_INA260();
Adafruit_EMC2101  emc2101;

// Fan state variables
int currentFanSpeed = 0; // 0 = off, 1 = 50%, 2 = 70%, 3 = 100%
const int fanSpeeds[] = {0,63,76,100};
const int speed1Pin = 32;
// pinmode(speed1Pin, OUTPUT);

////////////////////////////////////////////
//Here you put all the functions you will need to call within the setup()
////////////////////////////////////////////


float read_power_callback() { return (ina260.readPower() / 1000); }
float read_int_temp() { return (emc2101.getInternalTemperature()+ 273.15); }
float read_tach() { return (emc2101.getFanRPM());}

// Function to set fan speed
void setFanSpeed(int speedIndex) {
    if (speedIndex >= 0 && speedIndex < sizeof(fanSpeeds) / sizeof(fanSpeeds[0])) {
        emc2101.setDutyCycle(fanSpeeds[speedIndex]); // Set the fan speed using PWM
        debugD("Fan speed set to %d%%", (speedIndex + 1) * 50); // Print current speed (1 = 50%, etc.)
    }

    
    // beginnings of code to provide led indicators of fan speed.
    // switch (speedIndex) {
    //     case 1 : analogWrite(32, HIGH); break;
    //     case 2 : analogWrite(33, HIGH); break;
    //     case 3 : analogWrite(34, HIGH); break;
    //     default: {analogWrite(32, LOW);
    //               analogWrite(33, LOW);
    //               analogWrite(34, LOW);}
    // }
}

// Toggle function to cycle through fan speeds
void cycleFanSpeed() {
    currentFanSpeed = (currentFanSpeed + 1) % (sizeof(fanSpeeds) / sizeof(fanSpeeds[0])); // Cycle through speeds
    setFanSpeed(currentFanSpeed);
}

// Definitions

// Variables




// The setup function performs one-time application initialization.
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



// Set up the button input
const uint8_t kButtonPin = 35; // Use the appropriate GPIO pin for the button
pinMode(kButtonPin, INPUT_PULLUP); // Pull-up resistor

auto* button_input = new DigitalInputChange(kButtonPin, INPUT_PULLUP, CHANGE);
button_input->connect_to(new LambdaConsumer<bool>(
    [=](bool input) { 
        delay(300);  // avoid button bounce
        if (input == LOW ) {  // Check if the button is pressed
            cycleFanSpeed(); // Cycle through fan speeds
        }
    }
));



  //   This is for the ina260 power measurement to set it up and read the power
  ina260.begin();
    // Read the sensor every 2 seconds
  unsigned int read_interval_ina = 100;
  // Create a RepeatSensor with float output that reads the temperature
  // using the function defined above.
  auto* fan_control_power =
      new RepeatSensor<float>(read_interval_ina, read_power_callback);

 
 
 
 
  // This is for the emc2101 fan control board
  emc2101.begin();
  unsigned int read_interval_emc = 500;
  auto* int_temp =
      new RepeatSensor<float>(read_interval_emc, read_int_temp);

  // read the tach
  auto* fan_tach = 
      new RepeatSensor<float>(read_interval_emc, read_tach );

    setFanSpeed(currentFanSpeed);


  ////////////////////////////////////////////
  //  These are all the readings being sent to SignalK
  ////////////////////////////////////////////



  fan_control_power
    ->connect_to(new MovingAverage(15, 1.0,
         "/Sensors/StbFan/Power/avg"))
  ->connect_to(new SKOutputFloat(
      "sensors.stb_fan_power",         // Signal K path
      "/Sensors/StbFan/Power",        // configuration path, used in the
                                              // web UI and for storing the
                                              // configuration
      new SKMetadata("W",                     // Define output units
                     "Stb Fan Watts")  // Value description
      ));



  int_temp->connect_to(new SKOutputFloat(
      "sensors.stb_fan_inttemp",         // Signal K path
      "/Sensors/StbFan/IntTemp",        // configuration path, used in the
                                              // web UI and for storing the
                                              // configuration
      new SKMetadata("K",                     // Define output units
                     "Stb Fan Internal Temp")  // Value description
      ));

  fan_tach
  ->connect_to(new MovingAverage(25, 1.0,
         "/Sensors/StbFan/RPM/avg"))
  ->connect_to(new SKOutputInt(
      "sensors.stb_fan_tach",         // Signal K path
      "/Sensors/StbFan/RPM",        // configuration path, used in the
                                              // web UI and for storing the
                                              // configuration
      new SKMetadata("RPM",                     // Define output units
                     "Stb Fan Tach")  // Value description
      ));

}

void loop() { SensESPBaseApp::get_event_loop()->tick(); }