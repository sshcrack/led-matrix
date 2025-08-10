import React from 'react';
import { View, Text } from 'react-native';

interface VersionWarningProps {
    matrixVersion: string;
    appVersion: string;
    type: 'minor' | 'major';
}

const VersionWarning: React.FC<VersionWarningProps> = ({ matrixVersion, appVersion, type }) => {
    if (type === 'major') {
        return (
            <View style={{ backgroundColor: '#f87171', padding: 12, borderRadius: 8, marginBottom: 8 }}>
                <Text style={{ color: '#fff', fontWeight: 'bold', fontSize: 16 }}>⚠️ Major version mismatch!</Text>
                <Text style={{ color: '#fff', marginTop: 4 }}>
                    The matrix firmware (v{matrixVersion}) is newer than your app (v{appVersion}).
                    Please update your app to ensure compatibility.
                </Text>
            </View>
        );
    }
    return (
        <View style={{ backgroundColor: '#fbbf24', padding: 8, borderRadius: 8, marginBottom: 8 }}>
            <Text style={{ color: '#fff', fontWeight: 'bold' }}>Minor version mismatch</Text>
            <Text style={{ color: '#fff', marginTop: 2 }}>
                The matrix firmware (v{matrixVersion}) is slightly newer than your app (v{appVersion}).
                Some features may not work as expected.
            </Text>
        </View>
    );
};

export default VersionWarning;
