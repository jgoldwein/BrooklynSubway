# Copyright 2026 Joel W. Goldwein
# SPDX-License-Identifier: Apache-2.0
"""Extend an existing HA MQTT device-discovery payload with read-only diagnostics.

This only builds configuration. It never connects, publishes, or receives a password.
Existing components are preserved; the caller chooses which local map to test.
"""
import copy
import re

def extend_discovery(payload, device_id, prefix):
    if not re.fullmatch(r'[0-9a-f]{8}-[0-9a-f]{4}-4[0-9a-f]{3}-[89ab][0-9a-f]{3}-[0-9a-f]{12}',device_id):
        raise ValueError('canonical device UUID required')
    if not re.fullmatch(r'[a-z][a-z0-9_]*',prefix):raise ValueError('invalid entity prefix')
    result=copy.deepcopy(payload)
    components=result['components']
    event=f'brooklyn/v1/bkmap-{device_id}/event'
    valid="value_json.get('protocol') == 'brooklyn-telemetry' and value_json.get('kind') == 'diagnostics' and value_json.get('version') == 1 and value_json.get('device_id') == '"+device_id+"'"
    def add(key,name,expr,unit=None,timestamp=False):
        entity=f'sensor.{prefix}_{key}'
        fallback="states('"+entity+"') if has_value('"+entity+"') else none"
        c={'platform':'sensor','name':name,'unique_id':f'brooklyn_{device_id}_{key}',
           'default_entity_id':entity,'entity_category':'diagnostic','state_topic':event,
           'value_template':'{% if '+valid+' %}{{ '+expr+' }}{% else %}{{ '+fallback+' }}{% endif %}',
           'expire_after':180,'availability_topic':f'brooklyn/v1/bkmap-{device_id}/availability',
           'payload_available':'online','payload_not_available':'offline'}
        if unit:c.update(unit_of_measurement=unit,state_class='measurement')
        if timestamp:c['device_class']='timestamp'
        components[key]=c
    def at_time(key,name,agekey):
        expr="(as_timestamp(value_json.get('device_time_utc')) - value_json['"+agekey+"']) | timestamp_utc if value_json.get('time_synced') and value_json.get('"+agekey+"') is not none else none"
        add(key,name,expr,timestamp=True)
    add('diagnostics_report','Last diagnostics report',"value_json.get('device_time_utc') or none",timestamp=True)
    # All diagnostic fields remain inspectable on one entity without expanding the dashboard.
    components['diagnostics_report']['json_attributes_topic']=event
    components['diagnostics_report']['json_attributes_template']='{% if '+valid+' %}{{ value_json | tojson }}{% else %}{{ dict(states["sensor.'+prefix+'_diagnostics_report"].attributes) | tojson if states["sensor.'+prefix+'_diagnostics_report"] is not none else "{}" }}{% endif %}'
    add('button_age_at_report','Last button age at report',"value_json.get('last_button_age_s')",'s')
    add('control_age_at_report','Last control age at report',"value_json.get('last_control_age_s')",'s')
    at_time('last_button_press','Last button press','last_button_age_s')
    at_time('last_control_use','Last local control use','last_control_age_s')
    at_time('last_firmware_check','Last firmware check','last_check_age_s')
    add('next_firmware_check','Next firmware check due',"(as_timestamp(value_json.get('device_time_utc')) + value_json['next_check_in_s']) | timestamp_utc if value_json.get('time_synced') else none",timestamp=True)
    for key,name in [('automatic_updates','Automatic updates'),('last_check_succeeded','Last firmware check succeeded'),('quiet_hours','Quiet hours active'),('monitoring_enabled','Monitoring enabled')]:
        add(key,name,"'On' if value_json.get('"+key+"') else 'Off'")
    for key,name in [('led_output_reason','Map output reason'),('oled_output_reason','OLED output reason'),('restart_intent','Restart source'),('last_update_outcome','Last update outcome'),('last_setting_group','Last saved setting')]:
        add(key,name,"value_json.get('"+key+"') or none")
    for key,name in [('power_mode','POWER mode'),('led_brightness','LED brightness setting'),('oled_brightness','OLED brightness setting'),('settings_revision','Settings saves this boot')]:
        add(key,name,"value_json.get('"+key+"')")
    at_time('last_setting_save','Last setting saved','last_setting_age_s')
    reasons={0:'None',1:'Wi-Fi unavailable',2:'Secure connection failure',3:'HTTP failure',4:'Timeout',5:'Empty body',6:'Body too large',7:'Partial body',8:'Unknown length',9:'Buffer unavailable'}
    add('last_feed_error','Last feed error',str(reasons)+".get(value_json.get('last_error_category'), 'Unknown') if value_json.get('has_feed_error') else 'None recorded'")
    at_time('last_feed_error_time','Last feed error time','last_error_age_s')
    for transport in ['wifi','mqtt']:
        add(transport+'_outage_duration',transport.upper()+' last outage duration',"value_json.get('"+transport+"_outage_duration_s')",'s')
        at_time('last_'+transport+'_recovery','Last '+transport.upper()+' recovery',transport+'_recovery_age_s')
    # The individual label is already in all heartbeat versions. Keep identity/topic
    # and entity IDs stable when an administrator changes the friendly map name.
    components['map_name']={
        'platform':'sensor','name':'Map name','unique_id':f'brooklyn_{device_id}_map_name',
        'default_entity_id':f'sensor.{prefix}_map_name','entity_category':'diagnostic',
        'state_topic':f'brooklyn/v1/bkmap-{device_id}/status',
        'value_template':"{{ value_json.device_name }}",'expire_after':120,
        'availability_topic':f'brooklyn/v1/bkmap-{device_id}/availability',
        'payload_available':'online','payload_not_available':'offline','icon':'mdi:tag-text-outline'}
    return result
