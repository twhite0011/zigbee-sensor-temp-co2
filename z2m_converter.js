import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

const tempReporting = {min: 40, max: 60, change: 100};
const humidityReporting = {min: 40, max: 60, change: 100};
const co2Reporting = {min: 40, max: 60, change: 0.000005};

const base = {
    vendor: 'DIY',
    meta: {},
};

export default [
    {
        ...base,
        zigbeeModel: ['XIAO-SHTC3-CO2'],
        model: 'XIAO-SHTC3-CO2',
        description: 'Seeed XIAO ESP32-C6 SHTC3 Zigbee temperature/humidity + SCD4x CO2 sensor',
        extend: [
            m.temperature({reporting: tempReporting}),
            m.humidity({reporting: humidityReporting}),
            m.co2({reporting: co2Reporting}),
        ],
    },
    {
        ...base,
        zigbeeModel: ['XIAO-SHTC3'],
        model: 'XIAO-SHTC3',
        description: 'Seeed XIAO ESP32-C6 SHTC3 Zigbee temperature/humidity sensor',
        extend: [
            m.temperature({reporting: tempReporting}),
            m.humidity({reporting: humidityReporting}),
        ],
    },
];
