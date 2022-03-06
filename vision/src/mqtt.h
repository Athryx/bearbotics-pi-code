#pragma once

#include <mosquitto.h>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include "error.h"

struct CallbackData {
	// callback data must be void pointer because we store many different types of callbacks in the callbacks hashmap
	void (*callback)(std::string_view, void*);
	void *data;
};

class MqttClient {
	public:
		static std::optional<MqttClient> create(const std::string& host, int port);
		~MqttClient();

		Error update();

		Error publish(const std::string& topic, const std::string& payload);

		// calback takes in a string_view of the message and a pointer to the passed in object
		template<typename T>
		bool subscribe(const std::string& topic, void (*callback)(std::string_view, T*), T* data) {
			auto callback_data = CallbackData {
				.callback = (void (*)(std::string_view, void*)) callback,
				.data = data
			};

			// insert doesn't remove old elements, so delete the previous callback is it exists
			m_callbacks.erase(topic);
			// do this before subscribing to avoid race condition
			m_callbacks.insert(std::pair(topic, callback_data));

			// TODO: figure out if subscribing twice is a problem
			if (mosquitto_subscribe(m_client, nullptr, topic.c_str(), 0)) {
				m_callbacks.erase(topic);
				return false;
			}

			return true;
		}

		void unsubscribe(const std::string& topic);
	
	private:
		MqttClient();

		static void mqtt_message_callback(mosquitto *mosq, void *data, const mosquitto_message *msg);

		mosquitto *m_client;
		std::unordered_map<std::string, CallbackData> m_callbacks;
};
