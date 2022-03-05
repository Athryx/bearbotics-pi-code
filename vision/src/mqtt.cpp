#include "mqtt.h"
#include <mosquitto.h>
#include <unistd.h>

MqttClient::MqttClient() {}

MqttClient::~MqttClient() {
	mosquitto_destroy(m_client);
}

std::optional<MqttClient> MqttClient::create(const std::string& host, int port) {
	MqttClient out;

	auto client_name = std::string {"vision_"} + std::to_string(getpid());
	auto *client = mosquitto_new(client_name.c_str(), true, &out);
	if (client == nullptr) {
		return {};
	}

	mosquitto_message_callback_set(client, mqtt_message_callback);

	if (mosquitto_connect(client, host.c_str(), port, 60)) {
		return {};
	}

	out.m_client = client;
	return out;
}

void MqttClient::update() {
	int ret = mosquitto_loop(m_client, 0, 1);
	if (ret == MOSQ_ERR_CONN_LOST) {
		printf("warning: connection lost, reconnecting...\n");
		mosquitto_reconnect(m_client);
	} else if (ret != MOSQ_ERR_SUCCESS) {
		printf("warning: mosquitto_loop returned an error that was not connection lost\n");
	}
}

bool MqttClient::publish(const std::string& topic, const std::string &payload) {
	return mosquitto_publish(m_client, nullptr, topic.c_str(), payload.length(), payload.c_str(), 0, false) == MOSQ_ERR_SUCCESS;
}

void MqttClient::unsubscribe(const std::string& topic) {
	m_callbacks.erase(topic);
	if(mosquitto_unsubscribe(m_client, nullptr, topic.c_str())) {
		printf("warning: failed to unsubscribe from mqtt topic %s", topic.c_str());
	}
}

static void mqtt_message_callback(mosquitto *mosq, void *data, const mosquitto_message *msg) {
	MqttClient *client = (MqttClient *) data;
}
