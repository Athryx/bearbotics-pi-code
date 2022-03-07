#include "mqtt.h"
#include <mosquitto.h>
#include <optional>
#include <unistd.h>
#include <stdexcept>
#include "logging.h"

MqttClient::MqttClient(MqttClient&& client) {
	m_client = client.m_client;
	client.m_client = nullptr;
	m_callback_data = std::move(client.m_callback_data);
}

MqttClient& MqttClient::operator=(MqttClient&& client) {
	m_client = client.m_client;
	client.m_client = nullptr;
	m_callback_data = std::move(client.m_callback_data);
	return *this;
}

MqttClient::MqttClient(mosquitto *client, std::unique_ptr<CallbackData>&& callback_data):
m_client(client),
m_callback_data(std::move(callback_data)) {}

MqttClient::~MqttClient() {
	if (m_client) {
		mosquitto_destroy(m_client);
	}
}

std::optional<MqttClient> MqttClient::create(const std::string& host, int port) {
	auto *callback_data = new CallbackData;
	auto client_name = std::string {"vision_"} + std::to_string(getpid());

	auto *client = mosquitto_new(client_name.c_str(), true, callback_data);
	if (client == nullptr) {
		return {};
	}

	mosquitto_message_callback_set(client, mqtt_message_callback);

	if (mosquitto_connect(client, host.c_str(), port, 60)) {
		return {};
	}

	return std::make_optional<MqttClient>(MqttClient(client, std::unique_ptr<CallbackData>(callback_data)));
}

Error MqttClient::update() {
	int ret = mosquitto_loop(m_client, 0, 1);
	switch (ret) {
		case MOSQ_ERR_SUCCESS:
			return Error::ok();
		case MOSQ_ERR_INVAL:
			return Error::library("invalid paramaters passed to mosquitto_publish");
		case MOSQ_ERR_NOMEM:
			return Error::memory("mosquitto_loop");
		case MOSQ_ERR_NO_CONN:
			return Error::network("cannot execute mosquitto_loop when not connected to broker");
		case MOSQ_ERR_CONN_LOST: {
			lg::warn("connection lost, reconnecting...");
			// TODO: maybe use a non blocking version to avoid stopping remote viewing which does not require mqtt
			// not a big deal since currently the broker also runs on the pi
			switch (mosquitto_reconnect(m_client)) {
				case MOSQ_ERR_SUCCESS:
					return Error::ok();
				case MOSQ_ERR_NOMEM:
					return Error::memory("mosquitto_reconnect");
				case MOSQ_ERR_ERRNO:
					return Error::library("mosquitto library encountered bad errno");
				default:
					return Error::unknown("mosquitto_reconnect");
			}
		}
		case MOSQ_ERR_PROTOCOL:
			return Error::library("mqtt protocol mismatch");
		case MOSQ_ERR_ERRNO:
			return Error::library("mosquitto library encountered bad errno");
		default:
			return Error::unknown("mosquitto_loop");
	}
}

Error MqttClient::publish(const std::string& topic, const std::string &payload) {
	switch(mosquitto_publish(m_client, nullptr, topic.c_str(), payload.length(), payload.c_str(), 0, false)) {
		case MOSQ_ERR_SUCCESS:
			return Error::ok();
		case MOSQ_ERR_INVAL:
		case MOSQ_ERR_QOS_NOT_SUPPORTED:
			return Error::library("invalid paramaters passed to mosquitto_publish");
		case MOSQ_ERR_NOMEM:
			return Error::memory("mosquitto_publish");
		case MOSQ_ERR_NO_CONN:
			return Error::network("cannot send publish message to broker");
		case MOSQ_ERR_PROTOCOL:
			return Error::library("mqtt protocol mismatch");
		case MOSQ_ERR_PAYLOAD_SIZE:
		case MOSQ_ERR_OVERSIZE_PACKET:
			return Error::invalid_args("payload is too large");
		case MOSQ_ERR_MALFORMED_UTF8:
			return Error::invalid_args("payload is not valid utf-8");
		default:
			return Error::unknown("mosquitto_publish");
	}
}

void MqttClient::unsubscribe(const std::string& topic) {
	m_callback_data->callbacks.erase(topic);
	if(mosquitto_unsubscribe(m_client, nullptr, topic.c_str())) {
		// don't return an error, since this isn't an error that needs to be handled
		// MqttClient will handle it by not calling the callback anymore
		lg::warn("failed to tell mqtt broker to unsubscribe from mqtt topic %s", topic.c_str());
	}
}

void MqttClient::mqtt_message_callback(mosquitto *mosq, void *data, const mosquitto_message *msg) {
	CallbackData *callback_data = (CallbackData *) data;
	std::string topic(msg->topic);

	try {
		auto callback = callback_data->callbacks.at(topic);
		callback.callback(std::string_view((char *) msg->payload, msg->payloadlen), callback.data);
	} catch (const std::out_of_range& err) {
		lg::warn("warning: no callback for topic %s", topic.c_str());
		return;
	}
}
