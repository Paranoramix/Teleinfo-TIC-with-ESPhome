#include "esphome.h"
#include "esphome/core/defines.h"
#include "esphome/components/text_sensor/text_sensor.h"

/**
 * MyTicComponent is a dedicated component to get information from TeleInfo.
 * This components read UART input data and store values in variables.
 * By default, value is read non-stop but user can desactivate it (switch).
 * When a reading is correct, data is stored inside internal variable before to be 
 * sent to Home Assistant. 
 * Data is sent at each time_interval (by default: 10 seconds)
 * 
 * Implements 
 *    - PoolingComponent to update data by time interval
 *    - UARTDevice to read data from TeleInfo device (Linky, ...)
 *    - Swtich to start / stop reading.
 */ 
class MyTicComponent : public PollingComponent, public UARTDevice, public Switch {
 public:
	MyTicComponent(UARTComponent *parent, unsigned long time_interval) : PollingComponent(time_interval), UARTDevice(parent) { }

	Sensor *sensor_IINST = new Sensor();
	Sensor *sensor_ISOUSC = new Sensor();
	Sensor *sensor_PAPP = new Sensor();
	Sensor *sensor_BASE = new Sensor();
	TextSensor *sensor_ADCO = new TextSensor();

	bool enable = false;

	bool adco_updated = false;
	bool iinst_updated = false;
	bool isousc_updated = false;
	bool papp_updated = false;
	bool base_updated = false;
	
	float iinst = 0.0;
	float isousc = 0.0;
	float papp = 0.0;
	float base = 0.0;
	
	String adco = "";
	
  	/**
	 * Returns an instance of MyTicComponent.
	 * 
	 * @param UARTComponent* parent: the UART component used to read values sent by Teleinfo.
	 * @param unsigned long time_interval: the interval between sending data to Home Assistant. Default value: 10000
	 */
	static MyTicComponent* instance(UARTComponent *parent, unsigned long time_interval = 10000) {
		static MyTicComponent* INSTANCE = new MyTicComponent(parent, time_interval);
		return INSTANCE;
	}

	void write_state(bool state) override {
		enable = state;
		publish_state(state);
	}
	
	void setup() override {
		publish_state(enable);
	}
	
	/**
	 * Update function is used to send data to Home Assistant.
	 * Period between two call: time_interval.
	 */
	void update() override {
		if (enable) {
			if (adco_updated) {
				ESP_LOGI("Update", "ADCO update: %s", adco.c_str());
				sensor_ADCO->publish_state(adco.c_str());
				adco_updated = false;
			}
			if (base_updated) {
				ESP_LOGI("Update", "BASE update: %.0f", (base / 1000.0));
				sensor_BASE->publish_state(base / 1000.0);
				base_updated = false;
			}
			if (isousc_updated) {
				ESP_LOGI("Update", "ISOUSC update: %.0f", isousc);
				sensor_ISOUSC->publish_state(isousc);
				isousc_updated = false;
			}
			if (iinst_updated) {
				ESP_LOGI("Update", "IINST update: %.0f", iinst);
				sensor_IINST->publish_state(iinst);
				iinst_updated = false;
			}
			if (papp_updated) {
				ESP_LOGI("Update", "PAPP update: %.0f", papp);
				sensor_PAPP->publish_state(papp);
				papp_updated = false;
			}
		}
	}
	
	/**
	 * This function is called by App.loop() function.
	 * 
	 * We use it to read data from UART input.
	 */
	void loop() {
		if (enable) {
			String buff = "";
			while (available() > 0) {
				char c = read();

				// \r = end of message. Data is fully acquired. We leave the loop.
				if (c == '\r') {
					break;
				}

				buff += c;

				// \n = New message (first cher is \n. We trucate the buffer.
				// IF the buffer is over 50 chars, there is a problem, so we truncate it.
				if (c == '\n' || buff.length() > 50) {
					//ESP_LOGI("Buffer", "Buffer Size :  %d", buff.length());
					if (buff.length() > 50) {
						ESP_LOGW("Buffer", "Buffer was too big, cleaned !!!");
					}

					buff = "";	
				}
			}
		
			if (buff != "") {
				processString(buff);
				buff = "";
			}

			// a little delay. I tried with yield() but program hangs.
			delay(100);
		
		}


		yield(); // Preserve time for other processes.
	}
	
	/**
	 * Proceed to dispatch message and data.
	 */
	void processString(String str) {
		ESP_LOGD("tic", "tic_received %s", str.c_str());
		
		char separator = ' ';
	
		if (str.indexOf(separator) > -1) {
			// Great, it seems the data has a couple (name<space>value)
			String label = str.substring(0, str.indexOf(separator));
			String value = str.substring(str.indexOf(separator) + 1);
			
			if (value.indexOf(separator) > -1) {
				// Great again : we have a couple (value<space>checksum_char)
				value = value.substring(0, value.indexOf(separator));

				// Checksum verification
				char checksum = 0;

				for (int i = 0 ; i < str.length() - 2; i++) {
					checksum = checksum + str[i];
				}
				
				// According to Enedis's documentation, checksum is the sum of each characters before last separator.
				// When you computed the sum, you have to keep the 6 low bits and add 0x20.
				checksum = (checksum & 0x3F) + 0x20;	

				// Data is worked only if checksum is correct
				if (str[str.length() - 1] == checksum) {
					processCommand(label, value);
				} else {
					ESP_LOGW("Checksum", "Checksum error: %s - checksum: %02X - computed: %02X", str.c_str(), str[str.length() - 1], checksum);
				}
			}
		}
	}
  
	/**
	 * This function updates internal variables to be ready to send it at next next update.
	 */
	void processCommand(String label, String value) {
		ESP_LOGD("tic", "tic_label %s", label.c_str());
		ESP_LOGD("tic", "tic_value %s", value.c_str());	  

		if (label == "ADCO") {// adresse
			if (adco != value) {
				adco = value;
				adco_updated = true;
			}
			
			return;
		}
		
		if (label == "BASE") { // Current index
			if (base != value.toFloat()) {	
				base = value.toFloat();
				base_updated = true;
			}
			
			return;
		}
		
		if (label == "ISOUSC") { // 
			if (isousc != value.toFloat()) {	
				isousc = value.toFloat();
				isousc_updated = true;
			}
			
			return;
		}
		
		if (label == "IINST") {
			if (iinst != value.toFloat()) {
				iinst = value.toFloat();
				iinst_updated = true;
			}
			
			return;
		}
		
		if (label == "PAPP") {
			if (papp != value.toFloat()) {
				papp = value.toFloat();
				papp_updated = true;
			}
			
			return;
		}
		
		ESP_LOGI("tic", "data ignored: %s %s", label.c_str(), value.c_str());
	}
};
