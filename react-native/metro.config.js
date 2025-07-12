const { getDefaultConfig } = require('expo/metro-config');
const { withNativeWind } = require('nativewind/metro');
const {
    wrapWithReanimatedMetroConfig,
} = require('react-native-reanimated/metro-config');
const path = require('path');

const config = getDefaultConfig(__dirname);

// Add resolver configuration for path aliases
config.resolver = {
    ...config.resolver,
    alias: {
        '~': path.resolve(__dirname, '.'),
    },
};

module.exports = wrapWithReanimatedMetroConfig(withNativeWind(config, { input: './global.css' }));
