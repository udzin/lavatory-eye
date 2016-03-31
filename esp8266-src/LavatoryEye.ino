#include <ESP8266WiFi.h>
#include <ESP8266WebServer.h>
#include <EEPROM.h>
//#include <WiFiUdp.h>

#define PUBLIC_SSID "LavatoryEye"
#define CONFIG_VERSION "lv1"
#define CONFIG_START 0
#define SENSOR_PIN 0
#define RESET_PIN 2

volatile bool stateVacant = true;

ESP8266WebServer httpWebServer(80);

struct ServerSettings {
	char ssid[25];
	char password[25];
	char version[4];
} settings = {
	PUBLIC_SSID,
	"",
	//false,
	//(127,0,0,1),
	//999,
	CONFIG_VERSION
};


/**
 * Алгоритм работы
 *
 * 1) запускаемся.. проверяем в eeprom наличие настроек, если есть, то подключаемся к wifi и начинаем слушать данные с датчика и отправлять результат на указанный адрес
 * 2) если настроек нет, то создаем открытую точку доступа LavatoryEye
 * 3) на порту 80 всегда сидит сервис отображающий справку по настройке и текущий статус с датчика
 * 4) настройка происходит методом post с указанной строкой в url: http://ip/setup?ssid=ssid&password=password
 */
void setup() {
	wdt_disable();
	Serial.begin(115200);

	delay(1000);
	Serial.println("Init");

	loadConfig();
	if (String(settings.ssid) == String(PUBLIC_SSID)) {
		Serial.println("WiFi public AP");
		WiFi.mode(WIFI_AP);
		WiFi.softAP(PUBLIC_SSID);
		IPAddress myIP = WiFi.softAPIP();
		Serial.print("AP IP address: ");
		Serial.println(myIP);
	}
	else {
		Serial.print("WiFi connect: ");
		Serial.println(settings.ssid);
		WiFi.mode(WIFI_STA);
		WiFi.begin(settings.ssid, settings.password);
		while (WiFi.status() != WL_CONNECTED) {
			delay(500);
		}
		Serial.println("WiFi connected");
		IPAddress myIP = WiFi.localIP();
		Serial.print("AP IP address: ");
		Serial.println(myIP);
	}


	httpWebServer.on("/", showHelpAndStatus);
	httpWebServer.on("/setup", HTTP_POST, saveSettings);
	httpWebServer.on("/state", showState);
	httpWebServer.on("/status", showStatus);
	httpWebServer.begin();

	attachInterrupt(digitalPinToInterrupt(SENSOR_PIN), stateChanged, CHANGE);
	attachInterrupt(digitalPinToInterrupt(RESET_PIN), clearConfig, ONLOW);

	wdt_enable(WDTO_8S);
	Serial.println("Setup complete");
}

void stateChanged() {
	stateVacant = digitalRead(SENSOR_PIN) == LOW;
	Serial.println("State changed: " + stateVacant);
}


void loop() {
	httpWebServer.handleClient();
	wdt_reset();
}

void loadConfig() {
	Serial.println("Load config");

	EEPROM.begin(sizeof(settings));

	// To make sure there are settings, and they are YOURS!
	// If nothing is found it will use the default settings.
	if (//EEPROM.read(CONFIG_START + sizeof(settings) - 1) == settings.version_of_program[3] // this is '\0'
		EEPROM.read(CONFIG_START + sizeof(settings) - 2) == settings.version[2] &&
		EEPROM.read(CONFIG_START + sizeof(settings) - 3) == settings.version[1] &&
		EEPROM.read(CONFIG_START + sizeof(settings) - 4) == settings.version[0])
	{ // reads settings from EEPROM
		for (unsigned int t = 0; t < sizeof(settings); t++)
			*((char*)&settings + t) = EEPROM.read(CONFIG_START + t);
	}
	else {
		// settings aren't valid! will overwrite with default settings
		saveSettings();
	}

}

void clearConfig() {
	Serial.println("Clear config");
	EEPROM.begin(sizeof(settings));
	for (unsigned int i = 0; i < sizeof(settings); i++) {
		EEPROM.write(i, 0);
	}
	EEPROM.commit();
	Serial.println("restart");
	delay(1000);
	ESP.restart();
}

String saveConfig() {
	Serial.println("Save config");
	for (unsigned int t = 0; t < sizeof(settings); t++)
	{ // writes to EEPROM
		EEPROM.write(CONFIG_START + t, *((char*)&settings + t));
		// and verifies the data
		if (EEPROM.read(CONFIG_START + t) != *((char*)&settings + t))
		{
			// error writing to EEPROM
			return "Error writing to EEPROM!";
		}
	}
	EEPROM.commit();
	return "Save OK!";
}

void saveSettings() {
	Serial.println("Save settings");
	for (uint8_t i = 0; i < httpWebServer.args(); i++) {
		if (httpWebServer.argName(i) == "ssid") {
			Serial.println(httpWebServer.arg(i));
			httpWebServer.arg(i).toCharArray(settings.ssid, sizeof(settings.ssid), 0);

		}
		if (httpWebServer.argName(i) == "password") {
			Serial.println(httpWebServer.arg(i));
			httpWebServer.arg(i).toCharArray(settings.password, sizeof(settings.password), 0);
		}
	}

	Serial.println(settings.ssid);
	Serial.println(settings.password);

	httpWebServer.send(200, "text/html", "<h1>" + saveConfig() + "</h1>");
	delay(1000);
	ESP.restart();
	return;
}

void showHelpAndStatus() {
	Serial.println("Show help");
	String state = stateVacant ? "vacant" : "occupied";
	String helpContent = "";

	helpContent += "<html>";
	helpContent += "<head>";
	helpContent += "<meta http-equiv='refresh' content='10'>";
	helpContent += "<title>Lavatory: " + state + "</title>";
	helpContent += "</head>";
	helpContent += "<body>";
	helpContent += "<h1>Welcome to lavatory state monitor</h1>";
	helpContent += "<p>Auto refresh every 10 seconds</p>";
	helpContent += "<h1>State</h1>";
	helpContent += "<p>Lavatory is <b>" + state + "</b></p>";
	helpContent += "<h1>Help</h1>";
	helpContent += "<p><b>/</b> this help page</p>";
	helpContent += "<p><b>/status</b> colorful state</p>";
	helpContent += "<p><b>/setup?ssid=ssid&password=password</b> POST Method save settings</p>";
	helpContent += "<p><b>/state</b> return json state {state:'vacant', open:true}</p>";
	helpContent += "<h1>Reset</h1>";
	helpContent += "<p>press reset button to clear settings</p>";
	helpContent += "<h1>Author</h1>";
	helpContent += "<p>Evgeny Galkin | u-gin@bk.ru | 2016</p>";
	helpContent += "</body>";
	helpContent += "</html>";

	httpWebServer.send(200, "text/html", helpContent);
}

void showStatus() {
	Serial.println("Show status");
	String state = stateVacant ? "vacant" : "occupied";
	String color = stateVacant ? "green" : "red";
	String helpContent = "";

	helpContent += "<html>";
	helpContent += "<head>";
	helpContent += "<meta http-equiv='refresh' content='10'>";
	helpContent += "<title>Lavatory: " + state + "</title>";
	helpContent += "</head>";
	helpContent += "<body style='background-color:"+color+"'>";
	helpContent += "<h1><b>" + state + "</b></p>";

	httpWebServer.send(200, "text/html", helpContent);
}

void showState() {
	Serial.println("Show state json");
	String state = stateVacant ? "vacant" : "occupied";
	httpWebServer.send(200, "application/json", "{state: '" + state + "', open: " + stateVacant + "}");
}
