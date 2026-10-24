import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { NavigationContainer } from '@react-navigation/native';
import { GestureHandlerRootView } from 'react-native-gesture-handler';
import BenchScreen from './screens/BenchScreen';
import ChatScreen from './screens/ChatScreen';
import GridScreen from './screens/GridScreen';
import ReactScreen from './screens/ReactScreen';
import ScrollScreen from './screens/ScrollScreen';
import StylesScreen from './screens/StylesScreen';

const Tab = createBottomTabNavigator();

// flume-example://bench?scenario=chat&impl=flashlist&load=1 opens the Bench tab with that configuration.
const linking = { prefixes: ['flume-example://'], config: { screens: { Bench: 'bench' } } };

export default function App() {
  return (
    <GestureHandlerRootView style={{ flex: 1 }}>
      <NavigationContainer linking={linking}>
        <Tab.Navigator
          screenOptions={{
            tabBarLabelPosition: 'beside-icon',
            tabBarIconStyle: { display: 'none' },
            tabBarLabelStyle: { fontSize: 14 },
          }}
        >
          <Tab.Screen name="Chat" component={ChatScreen} />
          <Tab.Screen name="React" component={ReactScreen} />
          <Tab.Screen name="Grid" component={GridScreen} />
          <Tab.Screen name="Scroll" component={ScrollScreen} />
          <Tab.Screen name="Styles" component={StylesScreen} />
          <Tab.Screen name="Bench" component={BenchScreen} />
        </Tab.Navigator>
      </NavigationContainer>
    </GestureHandlerRootView>
  );
}
