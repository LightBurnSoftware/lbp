// SPDX-License-Identifier: MIT
// Copyright (c) 2026 LightBurn Software, LLC

#include "configuration.h"
#include "log.h"
#include <lbp/beio.h>
#include <lbp/spec.h>

#include "json.hpp"

#include <fstream>
#include <cassert>
#include <QDir>
#include <QStandardPaths>

using json = nlohmann::json;

using namespace lbp;

Configuration::Configuration()
	: m_settings()
	, m_scratch()
	, m_filepath(QDir(QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation))
					 .filePath("config.json")
					 .toStdString())
{
	// if file does not exist, create it.
	std::ifstream testfile(m_filepath);
	if (!testfile.good()) {
		std::ofstream newfile(m_filepath);
		newfile << "{}\n";
	}
}

void Configuration::commit()
{
	for (const auto &[key, value] : m_scratch) {
		m_settings[key] = value;
	}
	m_scratch.clear();
	save();
}

void Configuration::save()
{
	json data;
	for (const auto &[key, value] : m_settings) {
		std::string_view key_str;
		if (cmdToName(key, key_str)) {
			data[key_str] = value;
		}
	}

	std::ofstream f(m_filepath);
	std::string tmp = data.dump(2);
	f << tmp;
}

void Configuration::load()
{
	m_settings.clear();
	std::ifstream f(m_filepath);
	json data = json::parse(f);

	for(auto & i : data.items())
	{
		if (i.value().is_number()) {
			std::uint16_t id = 0;
			if (nameToCmd(i.key(), id)) {
				m_settings[id] = (int32_t) i.value();
			}
		}
	}
	gLog().push(Log::DEBUG, QString("Loaded configuration of size %1").arg(m_settings.size()));
}

bool Configuration::process(lbp::MaxPayload &request, OutputQueue &out_q)
{
	const uint16_t cmd = request.cmd();
	if (!isCfg(cmd)) {
		return false;
	}
	if (request.size() == 2) {
		// it's a query
		gLog().push(Log::INFO, QString("Config Get %1: %2").arg(cmd).arg(m_settings[cmd]));
		out_q.push(lbp::CmdMsg(cmd, 4, m_settings[cmd]));
		return true;
	}
	if (request.size() == 6) {
		m_scratch[cmd] = request.readIntArg();
		gLog().push(Log::INFO, QString("Config Set %1: %2").arg(cmd).arg(m_scratch[cmd]));
		out_q.push(lbp::CmdMsg(cmd));
		return true;
	}
	return false;
}

int32_t Configuration::get(uint16_t key, int32_t &value) const
{
	auto found = m_settings.find(key);
	if (found == m_settings.end()) {
		return false;
	}
	value = found->second;
	return true;
}

static constexpr uint16_t cgf_codes[99] = {cfg_head_dist,
										   cfg_laser1_freq,
										   cfg_laser1_min_power,
										   cfg_laser1_max_power,
										   cfg_laser1_preig_freq,
										   cfg_laser1_preig_pct,
										   cfg_laser2_freq,
										   cfg_laser2_min_power,
										   cfg_laser2_max_power,
										   cfg_laser2_preig_freq,
										   cfg_laser2_preig_pct,
										   cfg_x_settings,
										   cfg_x_step_length,
										   cfg_x_max_speed,
										   cfg_x_jumpoff_speed,
										   cfg_x_max_accel,
										   cfg_x_breadth,
										   cfg_x_key_jumpoff_speed,
										   cfg_x_key_accel,
										   cfg_x_estop_accel,
										   cfg_x_home_offset,
										   cfg_x_backlash,
										   cfg_y_settings,
										   cfg_y_step_length,
										   cfg_y_max_speed,
										   cfg_y_jumpoff_speed,
										   cfg_y_max_accel,
										   cfg_y_breadth,
										   cfg_y_key_jumpoff_speed,
										   cfg_y_key_accel,
										   cfg_y_estop_accel,
										   cfg_y_home_offset,
										   cfg_y_backlash,
										   cfg_z_settings,
										   cfg_z_step_length,
										   cfg_z_max_speed,
										   cfg_z_jumpoff_speed,
										   cfg_z_max_accel,
										   cfg_z_breadth,
										   cfg_z_key_jumpoff_speed,
										   cfg_z_key_accel,
										   cfg_z_estop_accel,
										   cfg_z_home_offset,
										   cfg_z_backlash,
										   cfg_u_settings,
										   cfg_u_step_length,
										   cfg_u_max_speed,
										   cfg_u_jumpoff_speed,
										   cfg_u_max_accel,
										   cfg_u_breadth,
										   cfg_u_key_jumpoff_speed,
										   cfg_u_key_accel,
										   cfg_u_estop_accel,
										   cfg_u_home_offset,
										   cfg_u_backlash,
										   cfg_idle_speed,
										   cfg_idle_acc,
										   cfg_idle_delay,
										   cfg_start_speed,
										   cfg_min_acc,
										   cfg_max_acc,
										   cfg_acc_factor_pct,
										   cfg_G0_acc_factor_pct,
										   cfg_speed_factor_pct,
										   cfg_docking_position_x,
										   cfg_docking_position_y,
										   cfg_docking_position_z,
										   cfg_engrave_x_start_speed,
										   cfg_engrave_y_start_speed,
										   cfg_engrave_x_acc,
										   cfg_engrave_y_acc,
										   cfg_line_shift_speed,
										   cfg_facula_size_pct,
										   cfg_engrave_factor_pct,
										   cfg_xy_home_speed,
										   cfg_z_home_speed,
										   cfg_z_work_speed,
										   cfg_u_home_speed,
										   cfg_u_work_speed,
										   cfg_material_thick,
										   cfg_focus_distance,
										   cfg_return_location,
										   cfg_reset_delay,
										   cfg_status_on_delay,
										   cfg_status_off_delay,
										   cfg_finish_delay,
										   cfg_feed_flags,
										   cfg_feed_pre_delay,
										   cfg_feed_post_delay,
										   cfg_feed_backlash,
										   cfg_rotary_enable,
										   cfg_rotary_pulses_per_rotation,
										   cfg_rotary_diameter,
										   cfg_wireless_panel_fast,
										   cfg_wireless_panel_slow,
										   cfg_autolayout,
										   cfg_axis_auto_home,
										   cfg_user_origin_x,
										   cfg_user_origin_y};

using NameToCmdMap = std::unordered_map<std::string_view, uint16_t>;

static const NameToCmdMap &getNameToIdMap()
{
	static NameToCmdMap names_to_cmds;
	if (names_to_cmds.empty()) {
		std::string_view name;
		for (const uint16_t cmd : cgf_codes) {
			if (cmdToName(cmd, name)) {
				names_to_cmds.emplace(name, cmd);
			}
		}
	}
	return names_to_cmds;
}

bool nameToCmd(const std::string_view &name, uint16_t &cmd)
{
	const NameToCmdMap &map = getNameToIdMap();

	auto found = map.find(name);
	if (found == map.end()) {
		return false;
	}
	cmd = found->second;
	return true;
}

bool isCfg(uint16_t cmd)
{
	int32_t value; // unused
	return getConfigDefault(cmd, value);
}

bool cmdToName(std::uint16_t cmd, std::string_view &name)
{
	switch (cmd) {
	case cfg_head_dist:
		name = "cfg_head_dist";
		return true;
	case cfg_laser1_freq:
		name = "cfg_laser1_freq";
		return true;
	case cfg_laser1_min_power:
		name = "cfg_laser1_min_power";
		return true;
	case cfg_laser1_max_power:
		name = "cfg_laser1_max_power";
		return true;
	case cfg_laser1_preig_freq:
		name = "cfg_laser1_preig_freq";
		return true;
	case cfg_laser1_preig_pct:
		name = "cfg_laser1_preig_pct";
		return true;
	case cfg_laser2_freq:
		name = "cfg_laser2_freq";
		return true;
	case cfg_laser2_min_power:
		name = "cfg_laser2_min_power";
		return true;
	case cfg_laser2_max_power:
		name = "cfg_laser2_max_power";
		return true;
	case cfg_laser2_preig_freq:
		name = "cfg_laser2_preig_freq";
		return true;
	case cfg_laser2_preig_pct:
		name = "cfg_laser2_preig_pct";
		return true;
	case cfg_x_settings:
		name = "cfg_x_settings";
		return true;
	case cfg_x_step_length:
		name = "cfg_x_step_length";
		return true;
	case cfg_x_max_speed:
		name = "cfg_x_max_speed";
		return true;
	case cfg_x_jumpoff_speed:
		name = "cfg_x_jumpoff_speed";
		return true;
	case cfg_x_max_accel:
		name = "cfg_x_max_accel";
		return true;
	case cfg_x_breadth:
		name = "cfg_x_breadth";
		return true;
	case cfg_x_key_jumpoff_speed:
		name = "cfg_x_key_jumpoff_speed";
		return true;
	case cfg_x_key_accel:
		name = "cfg_x_key_accel";
		return true;
	case cfg_x_estop_accel:
		name = "cfg_x_estop_accel";
		return true;
	case cfg_x_home_offset:
		name = "cfg_x_home_offset";
		return true;
	case cfg_x_backlash:
		name = "cfg_x_backlash";
		return true;
	case cfg_y_settings:
		name = "cfg_y_settings";
		return true;
	case cfg_y_step_length:
		name = "cfg_y_step_length";
		return true;
	case cfg_y_max_speed:
		name = "cfg_y_max_speed";
		return true;
	case cfg_y_jumpoff_speed:
		name = "cfg_y_jumpoff_speed";
		return true;
	case cfg_y_max_accel:
		name = "cfg_y_max_accel";
		return true;
	case cfg_y_breadth:
		name = "cfg_y_breadth";
		return true;
	case cfg_y_key_jumpoff_speed:
		name = "cfg_y_key_jumpoff_speed";
		return true;
	case cfg_y_key_accel:
		name = "cfg_y_key_accel";
		return true;
	case cfg_y_estop_accel:
		name = "cfg_y_estop_accel";
		return true;
	case cfg_y_home_offset:
		name = "cfg_y_home_offset";
		return true;
	case cfg_y_backlash:
		name = "cfg_y_backlash";
		return true;
	case cfg_z_settings:
		name = "cfg_z_settings";
		return true;
	case cfg_z_step_length:
		name = "cfg_z_step_length";
		return true;
	case cfg_z_max_speed:
		name = "cfg_z_max_speed";
		return true;
	case cfg_z_jumpoff_speed:
		name = "cfg_z_jumpoff_speed";
		return true;
	case cfg_z_max_accel:
		name = "cfg_z_max_accel";
		return true;
	case cfg_z_breadth:
		name = "cfg_z_breadth";
		return true;
	case cfg_z_key_jumpoff_speed:
		name = "cfg_z_key_jumpoff_speed";
		return true;
	case cfg_z_key_accel:
		name = "cfg_z_key_accel";
		return true;
	case cfg_z_estop_accel:
		name = "cfg_z_estop_accel";
		return true;
	case cfg_z_home_offset:
		name = "cfg_z_home_offset";
		return true;
	case cfg_z_backlash:
		name = "cfg_z_backlash";
		return true;
	case cfg_u_settings:
		name = "cfg_u_settings";
		return true;
	case cfg_u_step_length:
		name = "cfg_u_step_length";
		return true;
	case cfg_u_max_speed:
		name = "cfg_u_max_speed";
		return true;
	case cfg_u_jumpoff_speed:
		name = "cfg_u_jumpoff_speed";
		return true;
	case cfg_u_max_accel:
		name = "cfg_u_max_accel";
		return true;
	case cfg_u_breadth:
		name = "cfg_u_breadth";
		return true;
	case cfg_u_key_jumpoff_speed:
		name = "cfg_u_key_jumpoff_speed";
		return true;
	case cfg_u_key_accel:
		name = "cfg_u_key_accel";
		return true;
	case cfg_u_estop_accel:
		name = "cfg_u_estop_accel";
		return true;
	case cfg_u_home_offset:
		name = "cfg_u_home_offset";
		return true;
	case cfg_u_backlash:
		name = "cfg_u_backlash";
		return true;
	case cfg_idle_speed:
		name = "cfg_idle_speed";
		return true;
	case cfg_idle_acc:
		name = "cfg_idle_acc";
		return true;
	case cfg_idle_delay:
		name = "cfg_idle_delay";
		return true;
	case cfg_start_speed:
		name = "cfg_start_speed";
		return true;
	case cfg_min_acc:
		name = "cfg_min_acc";
		return true;
	case cfg_max_acc:
		name = "cfg_max_acc";
		return true;
	case cfg_acc_factor_pct:
		name = "cfg_acc_factor_pct";
		return true;
	case cfg_G0_acc_factor_pct:
		name = "cfg_G0_acc_factor_pct";
		return true;
	case cfg_speed_factor_pct:
		name = "cfg_speed_factor_pct";
		return true;
	case cfg_docking_position_x:
		name = "cfg_docking_position_x";
		return true;
	case cfg_docking_position_y:
		name = "cfg_docking_position_y";
		return true;
	case cfg_docking_position_z:
		name = "cfg_docking_position_z";
		return true;
	case cfg_engrave_x_start_speed:
		name = "cfg_engrave_x_start_speed";
		return true;
	case cfg_engrave_y_start_speed:
		name = "cfg_engrave_y_start_speed";
		return true;
	case cfg_engrave_x_acc:
		name = "cfg_engrave_x_acc";
		return true;
	case cfg_engrave_y_acc:
		name = "cfg_engrave_y_acc";
		return true;
	case cfg_line_shift_speed:
		name = "cfg_line_shift_speed";
		return true;
	case cfg_facula_size_pct:
		name = "cfg_facula_size_pct";
		return true;
	case cfg_engrave_factor_pct:
		name = "cfg_engrave_factor_pct";
		return true;
	case cfg_xy_home_speed:
		name = "cfg_xy_home_speed";
		return true;
	case cfg_z_home_speed:
		name = "cfg_z_home_speed";
		return true;
	case cfg_z_work_speed:
		name = "cfg_z_work_speed";
		return true;
	case cfg_u_home_speed:
		name = "cfg_u_home_speed";
		return true;
	case cfg_u_work_speed:
		name = "cfg_u_work_speed";
		return true;
	case cfg_material_thick:
		name = "cfg_material_thick";
		return true;
	case cfg_focus_distance:
		name = "cfg_focus_distance";
		return true;
	case cfg_return_location:
		name = "cfg_return_location";
		return true;
	case cfg_reset_delay:
		name = "cfg_reset_delay";
		return true;
	case cfg_status_on_delay:
		name = "cfg_status_on_delay";
		return true;
	case cfg_status_off_delay:
		name = "cfg_status_off_delay";
		return true;
	case cfg_finish_delay:
		name = "cfg_finish_delay";
		return true;
	case cfg_feed_flags:
		name = "cfg_feed_flags";
		return true;
	case cfg_feed_pre_delay:
		name = "cfg_feed_pre_delay";
		return true;
	case cfg_feed_post_delay:
		name = "cfg_feed_post_delay";
		return true;
	case cfg_feed_backlash:
		name = "cfg_feed_backlash";
		return true;
	case cfg_rotary_enable:
		name = "cfg_rotary_enable";
		return true;
	case cfg_rotary_pulses_per_rotation:
		name = "cfg_rotary_pulses_per_rotation";
		return true;
	case cfg_rotary_diameter:
		name = "cfg_rotary_diameter";
		return true;
	case cfg_wireless_panel_fast:
		name = "cfg_wireless_panel_fast";
		return true;
	case cfg_wireless_panel_slow:
		name = "cfg_wireless_panel_slow";
		return true;
	case cfg_autolayout:
		name = "cfg_autolayout";
		return true;
	case cfg_axis_auto_home:
		name = "cfg_axis_auto_home";
		return true;
	case cfg_user_origin_x:
		name = "cfg_user_origin_x";
		return true;
	case cfg_user_origin_y:
		name = "cfg_user_origin_y";
		return true;
	default:
		return false;
	}
}

bool getConfigDefault(uint16_t cmd, int32_t &value)
{
	switch (cmd) {
	case cfg_head_dist:
	case cfg_laser1_freq:
	case cfg_laser1_min_power:
	case cfg_laser1_max_power:
	case cfg_laser1_preig_freq:
	case cfg_laser1_preig_pct:
	case cfg_laser2_freq:
	case cfg_laser2_min_power:
	case cfg_laser2_max_power:
	case cfg_laser2_preig_freq:
	case cfg_laser2_preig_pct:
	case cfg_x_settings:
	case cfg_x_step_length:
	case cfg_x_max_speed:
	case cfg_x_jumpoff_speed:
	case cfg_x_max_accel:
	case cfg_x_breadth:
	case cfg_x_key_jumpoff_speed:
	case cfg_x_key_accel:
	case cfg_x_estop_accel:
	case cfg_x_home_offset:
	case cfg_x_backlash:
	case cfg_y_settings:
	case cfg_y_step_length:
	case cfg_y_max_speed:
	case cfg_y_jumpoff_speed:
	case cfg_y_max_accel:
	case cfg_y_breadth:
	case cfg_y_key_jumpoff_speed:
	case cfg_y_key_accel:
	case cfg_y_estop_accel:
	case cfg_y_home_offset:
	case cfg_y_backlash:
	case cfg_z_settings:
	case cfg_z_step_length:
	case cfg_z_max_speed:
	case cfg_z_jumpoff_speed:
	case cfg_z_max_accel:
	case cfg_z_breadth:
	case cfg_z_key_jumpoff_speed:
	case cfg_z_key_accel:
	case cfg_z_estop_accel:
	case cfg_z_home_offset:
	case cfg_z_backlash:
	case cfg_u_settings:
	case cfg_u_step_length:
	case cfg_u_max_speed:
	case cfg_u_jumpoff_speed:
	case cfg_u_max_accel:
	case cfg_u_breadth:
	case cfg_u_key_jumpoff_speed:
	case cfg_u_key_accel:
	case cfg_u_estop_accel:
	case cfg_u_home_offset:
	case cfg_u_backlash:
	case cfg_idle_speed:
	case cfg_idle_acc:
	case cfg_idle_delay:
	case cfg_start_speed:
	case cfg_min_acc:
	case cfg_max_acc:
	case cfg_acc_factor_pct:
	case cfg_G0_acc_factor_pct:
	case cfg_speed_factor_pct:
	case cfg_docking_position_x:
	case cfg_docking_position_y:
	case cfg_docking_position_z:
	case cfg_engrave_x_start_speed:
	case cfg_engrave_y_start_speed:
	case cfg_engrave_x_acc:
	case cfg_engrave_y_acc:
	case cfg_line_shift_speed:
	case cfg_facula_size_pct:
	case cfg_engrave_factor_pct:
	case cfg_xy_home_speed:
	case cfg_z_home_speed:
	case cfg_z_work_speed:
	case cfg_u_home_speed:
	case cfg_u_work_speed:
	case cfg_material_thick:
	case cfg_focus_distance:
	case cfg_return_location:
	case cfg_reset_delay:
	case cfg_status_on_delay:
	case cfg_status_off_delay:
	case cfg_finish_delay:
	case cfg_feed_flags:
	case cfg_feed_pre_delay:
	case cfg_feed_post_delay:
	case cfg_feed_backlash:
	case cfg_rotary_enable:
	case cfg_rotary_pulses_per_rotation:
	case cfg_rotary_diameter:
	case cfg_wireless_panel_fast:
	case cfg_wireless_panel_slow:
	case cfg_autolayout:
	case cfg_axis_auto_home:
	case cfg_user_origin_x:
	case cfg_user_origin_y:
		value = 0;
		return true;
	default:
		return false;
	}
}
