import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

export default {
    zigbeeModel: ['XIAO-SHTC3-CO2'],
    model: 'XIAO-SHTC3-CO2',
    vendor: 'DIY',
    description: 'Seeed XIAO ESP32-C6 SHTC3 Zigbee temperature and humidity sensor SCD4x CO2 sensor',
    extend: [m.temperature(), m.humidity(), m.co2()],
    meta: {},
};
