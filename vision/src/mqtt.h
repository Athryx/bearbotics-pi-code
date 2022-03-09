#pragma once

#include <mosquitto.h>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <memory>
#include "error.h"

struct CallbackEntry {
	// callback data must be void pointer because we store many different types of callbacks in the callbacks hashmap
	void (*callback)(std::string_view, void*);
	void *data;
};

struct CallbackData {
	std::unordered_map<std::string, CallbackEntry> callbacks {};
};

class MqttClient {
	public:
		MqttClient(MqttClient&& client);
		MqttClient& operator=(MqttClient&& client);

		// creates new mqtt client, returns none on failure
		static std::optional<MqttClient> create(const std::string& host, int port);
		~MqttClient();

		Error update();

		Error publish(const std::string& topic, const std::string& payload);

		// calback takes in a string_view of the message and a pointer to the passed in object
		template<typename T>
		Error subscribe(const std::string& topic, void (*callback)(std::string_view, T*), T* data) {
			auto callback_entry = CallbackEntry {
				.callback = (void (*)(std::string_view, void*)) callback,
				.data = data
			};

			// insert doesn't remove old elements, so delete the previous callback is it exists
			m_callback_data->callbacks.erase(topic);
			// do this before subscribing to avoid race condition
			m_callback_data->callbacks.insert(std::pair(topic, callback_entry));

			// TODO: figure out if subscribing twice is a problem
			if (mosquitto_subscribe(m_client, nullptr, topic.c_str(), 0)) {
				m_callback_data->callbacks.erase(topic);
				// TODO: report more specific error
				return Error::library("could not subscribe to mqtt topic");
			}

			return Error::ok();
		}

		void unsubscribe(const std::string& topic);

	private:
		MqttClient(mosquitto *client, std::unique_ptr<CallbackData>&& callback_data);

		static void mqtt_message_callback(mosquitto *mosq, void *data, const mosquitto_message *msg);

		mosquitto *m_client;
		std::unique_ptr<CallbackData> m_callback_data;
};
