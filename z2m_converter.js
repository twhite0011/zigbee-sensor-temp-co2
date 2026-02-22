import * as m from 'zigbee-herdsman-converters/lib/modernExtend';

export default {
    zigbeeModel: ['XIAO-SHTC3-CO2'],
    model: 'XIAO-SHTC3-CO2',
    vendor: 'DIY',
    description: 'Seeed XIAO ESP32-C6 SHTC3 + SCD4X Zigbee sensor',
    extend: [
        m.temperature(),
        m.humidity(),
        m.numeric({
            name: 'co2',
            cluster: 'msCO2',
            attribute: 'measuredValue',
            unit: 'ppm',
            valueMin: 0,
            valueMax: 5000,
            valueStep: 1,
            reporting: {min: '10_SECONDS', max: '1_HOUR', change: 25},
            access: 'STATE_GET',
            description: 'Measured carbon dioxide concentration',
        }),
    ],
    meta: {},
};
