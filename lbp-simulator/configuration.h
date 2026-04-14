// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#pragma once

#include "simutils.h"

#include <lbp/message.h>
#include <lbp/payload.h>

#include <string>
#include <unordered_map>

using ConfigMap = std::unordered_map<uint16_t, int32_t>;

/**
 * Utility class for storing and fetching configuration values.
 *
 * Utilizes a set-and-commit pattern - after setting, a value is only updated if `commit` is called.
 * `get` will only return the canonical value, not the staged value.
 *
 * Uses json for configuration storage.
 */
class Configuration
{
public:
	Configuration();

	/** save the current configuration to disk. */
	void save();

	/** load the saved configuration from disk. */
	void load();

	/** commit the staged changes to the canonical settings. */
	void commit();

	/**
	 * get the canonical value for a key. (NOT the value that is staged to be written.)
	 * @param key The key for the setting in question.
	 * @param value The resulting value, if it exists.
	 * @return true on success, false for unknown key.
	 */
	int32_t get(uint16_t key, int32_t &value) const;

	/**
	 * @brief Attempt to process the given request. May enqueue an output packet.
	 *
	 * @param request The request to process.
	 * @param out_q Storage for resulting output packets.
	 * @return True if the  was able to process the request, false otherwise.
	 */
	bool process(lbp::MaxPayload &request, OutputQueue &out_q);

private:
	/**
	 * Stage a key/value pair for commit.
	 * @param key The key for the setting in question.
	 * @param value The value to set.
	 */
	void set(uint16_t key, int32_t value);

	ConfigMap m_settings;		  // The actual, committed settings
	ConfigMap m_scratch;		  // Scratch space for staged changes
	const std::string m_filepath; // location to write config.json
};

/**
 * @brief given a msg cmd, return the string name of that message.
 * @param cmd The command code of the message.
 * @param name the resulting name.
 * @return True for a valid cmd, false otherwise.
 */
bool cmdToName(uint16_t cmd, std::string_view &name);
/**
 * @brief given a message name, return the code of that message. Inverse of GetMsgName.
 * @param name The name of the message in question.
 * @param cmd The resulting command code, if valid.
 * @return True if cmd is a valid cfg command with a name, false otherwise.
 */
bool nameToCmd(const std::string_view &name, uint16_t &cmd);

/**
 * @brief GetConfigDefault
 * @param cmd The command code of the configuration cmd in question.
 * @return True if the cmd is a valid cfg command with a default value, false otherwise.
 */
bool getConfigDefault(std::uint16_t cmd, int32_t &value);

/**
 * @brief Check if a message command corresponds to a config message.
 * @param cmd The command code of the message in question.
 * @return True if the command code is a config message, false otherwise.
 */
bool isCfg(uint16_t cmd);
