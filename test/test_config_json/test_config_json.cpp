#include <unity.h>
#include <ArduinoJson.h>
#include <cstring>

void test_default_config_serializes_correctly() {
    JsonDocument doc;
    doc["num_strips"] = 1;
    doc["active_programme"] = 0;

    JsonArray strips = doc["strips"].to<JsonArray>();
    JsonObject s = strips.add<JsonObject>();
    s["pin"] = 5;
    s["num_leds"] = 60;
    s["brightness"] = 128;
    s["enabled"] = true;

    char buffer[1024];
    size_t len = serializeJson(doc, buffer, sizeof(buffer));
    TEST_ASSERT_GREATER_THAN(0, len);

    JsonDocument doc2;
    DeserializationError err = deserializeJson(doc2, buffer);
    TEST_ASSERT_TRUE(err == DeserializationError::Ok);
    TEST_ASSERT_EQUAL(1, doc2["num_strips"].as<int>());
    TEST_ASSERT_EQUAL(0, doc2["active_programme"].as<int>());
    TEST_ASSERT_EQUAL(5, doc2["strips"][0]["pin"].as<int>());
    TEST_ASSERT_EQUAL(60, doc2["strips"][0]["num_leds"].as<int>());
    TEST_ASSERT_EQUAL(128, doc2["strips"][0]["brightness"].as<int>());
    TEST_ASSERT_TRUE(doc2["strips"][0]["enabled"].as<bool>());
}

void test_missing_fields_get_defaults() {
    const char* minimal = "{\"num_strips\":2}";
    JsonDocument doc;
    DeserializationError err = deserializeJson(doc, minimal);
    TEST_ASSERT_TRUE(err == DeserializationError::Ok);

    uint8_t prog = doc["active_programme"] | (uint8_t)0;
    TEST_ASSERT_EQUAL(0, prog);

    JsonArray strips = doc["strips"];
    TEST_ASSERT_TRUE(strips.isNull());
}

void test_wifi_config_round_trip() {
    JsonDocument doc;
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["ssid"] = "MyNetwork";
    wifi["password"] = "MyPassword123";

    char buffer[512];
    serializeJson(doc, buffer, sizeof(buffer));

    JsonDocument doc2;
    deserializeJson(doc2, buffer);

    char ssid[33];
    char pass[65];
    strlcpy(ssid, doc2["wifi"]["ssid"] | "", sizeof(ssid));
    strlcpy(pass, doc2["wifi"]["password"] | "", sizeof(pass));

    TEST_ASSERT_EQUAL_STRING("MyNetwork", ssid);
    TEST_ASSERT_EQUAL_STRING("MyPassword123", pass);
}

void test_mqtt_config_round_trip() {
    JsonDocument doc;
    JsonObject mqtt = doc["mqtt"].to<JsonObject>();
    mqtt["host"] = "192.168.1.100";
    mqtt["port"] = 1883;
    mqtt["user"] = "leduser";
    mqtt["password"] = "ledpass";
    mqtt["base_topic"] = "home/led/lounge";
    mqtt["device_name"] = "Lounge-LEDs";

    char buffer[512];
    serializeJson(doc, buffer, sizeof(buffer));

    JsonDocument doc2;
    deserializeJson(doc2, buffer);

    TEST_ASSERT_EQUAL_STRING("192.168.1.100", doc2["mqtt"]["host"].as<const char*>());
    TEST_ASSERT_EQUAL(1883, doc2["mqtt"]["port"].as<int>());
    TEST_ASSERT_EQUAL_STRING("home/led/lounge", doc2["mqtt"]["base_topic"].as<const char*>());
    TEST_ASSERT_EQUAL_STRING("Lounge-LEDs", doc2["mqtt"]["device_name"].as<const char*>());
}

void test_five_strips_config() {
    JsonDocument doc;
    doc["num_strips"] = 5;
    JsonArray strips = doc["strips"].to<JsonArray>();

    uint8_t pins[] = {5, 4, 14, 12, 13};
    uint16_t lengths[] = {60, 30, 45, 120, 90};

    for (int i = 0; i < 5; i++) {
        JsonObject s = strips.add<JsonObject>();
        s["pin"] = pins[i];
        s["num_leds"] = lengths[i];
        s["brightness"] = 200;
        s["enabled"] = true;
    }

    char buffer[2048];
    serializeJson(doc, buffer, sizeof(buffer));

    JsonDocument doc2;
    deserializeJson(doc2, buffer);

    TEST_ASSERT_EQUAL(5, doc2["num_strips"].as<int>());
    TEST_ASSERT_EQUAL(5, doc2["strips"].size());
    TEST_ASSERT_EQUAL(14, doc2["strips"][2]["pin"].as<int>());
    TEST_ASSERT_EQUAL(120, doc2["strips"][3]["num_leds"].as<int>());
}

int main(int argc, char** argv) {
    UNITY_BEGIN();
    RUN_TEST(test_default_config_serializes_correctly);
    RUN_TEST(test_missing_fields_get_defaults);
    RUN_TEST(test_wifi_config_round_trip);
    RUN_TEST(test_mqtt_config_round_trip);
    RUN_TEST(test_five_strips_config);
    return UNITY_END();
}
