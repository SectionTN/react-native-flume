import { createBottomTabNavigator } from '@react-navigation/bottom-tabs';
import { NavigationContainer } from '@react-navigation/native';
import ChatScreen from './screens/ChatScreen';
import GridScreen from './screens/GridScreen';
import ReactScreen from './screens/ReactScreen';
import ScrollScreen from './screens/ScrollScreen';
import StylesScreen from './screens/StylesScreen';

const Tab = createBottomTabNavigator();

export default function App() {
  return (
    <NavigationContainer>
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
      </Tab.Navigator>
    </NavigationContainer>
  );
}
