import { memo } from 'react';
import { Image, Pressable, StyleSheet, Text, View } from 'react-native';
import type { BenchChat, FeedPost } from './data';

// React twins of the worklet cells in cells.ts, with the same structure and values.
export const ChatRow = memo(function ChatRow({ item }: { item: BenchChat }) {
  return (
    <View style={styles.chat}>
      <Image source={{ uri: item.avatar }} style={styles.avatar} />
      {item.kind === 'voice' ? (
        <View style={styles.voice}>
          <Text style={styles.play}>▶</Text>
          <View style={styles.track} />
          <Text style={styles.time}>{item.duration ?? ''}</Text>
        </View>
      ) : (
        <View style={styles.body}>
          <Text style={styles.message}>{item.text}</Text>
          <Text style={styles.time}>{item.time}</Text>
        </View>
      )}
      <Pressable style={styles.like}>
        <Text style={styles.accent}>Like</Text>
      </Pressable>
    </View>
  );
});

export const FeedRow = memo(function FeedRow({ item }: { item: FeedPost }) {
  return (
    <View style={styles.post}>
      <View style={styles.header}>
        <Image source={{ uri: item.avatar }} style={styles.avatar} />
        <View>
          <Text style={styles.author}>{item.author}</Text>
          <Text style={styles.time}>{item.time}</Text>
        </View>
      </View>
      <Text style={styles.postText}>{item.text}</Text>
      {item.photo !== undefined ? (
        <Image source={{ uri: item.photo }} style={styles.photo} />
      ) : null}
      <View style={styles.actions}>
        <Text style={styles.count}>{`${item.likes} likes`}</Text>
        <Text style={styles.count}>{`${item.comments} comments`}</Text>
        <Pressable>
          <Text style={styles.share}>Share</Text>
        </Pressable>
      </View>
      <View style={styles.strip} />
    </View>
  );
});

// FlashList's separator props, also what Flume's React cells get.
export function FlashSeparator(_props: { leadingItem: unknown; trailingItem: unknown }) {
  return <View style={styles.line} />;
}

// LegendList's separator prop.
export function LegendSeparator(_props: { leadingItem: unknown }) {
  return <View style={styles.line} />;
}

const styles = StyleSheet.create({
  chat: {
    flexDirection: 'row',
    padding: 12,
    gap: 12,
    alignItems: 'center',
    backgroundColor: '#ffffff',
  },
  avatar: { width: 40, height: 40, borderRadius: 20 },
  voice: { flex: 1, flexDirection: 'row', alignItems: 'center', gap: 8 },
  play: { fontSize: 18, color: '#0066ff' },
  track: { flex: 1, height: 4, borderRadius: 2, backgroundColor: '#dddddd' },
  body: { flex: 1 },
  message: { fontSize: 16, color: '#111111' },
  time: { fontSize: 12, color: '#666666' },
  like: { padding: 8 },
  accent: { color: '#0066ff', fontWeight: '600' },
  post: { backgroundColor: '#ffffff' },
  header: { flexDirection: 'row', padding: 12, gap: 10, alignItems: 'center' },
  author: { fontSize: 15, fontWeight: '600', color: '#111111' },
  postText: { fontSize: 15, color: '#111111', paddingHorizontal: 12 },
  photo: { alignSelf: 'stretch', aspectRatio: 4 / 3, marginTop: 8, backgroundColor: '#e8e8e8' },
  actions: { flexDirection: 'row', padding: 12, gap: 16, alignItems: 'center' },
  count: { fontSize: 13, color: '#666666' },
  share: { fontSize: 13, fontWeight: '600', color: '#0066ff' },
  strip: { height: 8, backgroundColor: '#f0f0f0' },
  line: { height: 1, backgroundColor: '#dddddd' },
});
