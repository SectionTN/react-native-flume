// Shared with the Node runner's tests, so no React Native imports here.
export type BenchChat = {
  id: string;
  kind: 'message' | 'voice';
  text: string;
  time: string;
  avatar: string;
  duration?: string;
};

export type FeedPost = {
  id: string;
  author: string;
  avatar: string;
  time: string;
  text: string;
  photo?: string;
  likes: number;
  comments: number;
};

const WORDS =
  'the quick brown fox jumps over a lazy dog while the river keeps running past old stones and new bridges under a pale evening sky where swallows turn and the town settles into its slow quiet hour'.split(
    ' ',
  );

const AUTHORS = ['Ada', 'Grace', 'Linus', 'Margaret', 'Dennis', 'Barbara', 'Ken', 'Frances'];

function random(seed: number) {
  let state = seed;
  return () => {
    state = (state * 1103515245 + 12345) % 2147483648;
    return state / 2147483648;
  };
}

function words(rand: () => number, target: number): string {
  let text = '';
  while (text.length < target) text += (text ? ' ' : '') + WORDS[Math.floor(rand() * WORDS.length)];
  return text;
}

function clock(i: number): string {
  return `${String(8 + ((i * 3) % 12)).padStart(2, '0')}:${String((i * 7) % 60).padStart(2, '0')}`;
}

// Oldest first, the Chat tab's text bands without day bars; a new version gives the same rows new ids.
export function makeBenchChats(count: number, version = 0): BenchChat[] {
  const rand = random(42);
  const chats: BenchChat[] = [];
  for (let i = 0; i < count; i++) {
    const band = rand();
    const target =
      band < 0.4 ? 20 + rand() * 20 : band < 0.75 ? 60 + rand() * 40 : 120 + rand() * 40;
    const text = words(rand, target);
    const voice = i % 5 === 4;
    chats.push({
      id: `c${version}-${i}`,
      kind: voice ? 'voice' : 'message',
      text,
      time: clock(i),
      avatar: `https://i.pravatar.cc/80?img=${(i % 20) + 1}`,
      duration: voice
        ? `${Math.floor(rand() * 3)}:${String(Math.floor(rand() * 60)).padStart(2, '0')}`
        : undefined,
    });
  }
  return chats;
}

// The newest count items become new objects with the same ids; the rest keep their references.
export function changeChats(chats: readonly BenchChat[], count: number): BenchChat[] {
  const from = Math.max(0, chats.length - count);
  return chats.map((chat, i) => (i < from ? chat : { ...chat, text: `${chat.text} (edited)` }));
}

// One to five lines of text per post and a photo on every even index.
export function makeFeed(count: number, version = 0): FeedPost[] {
  const rand = random(7);
  return Array.from({ length: count }, (_, i) => ({
    id: `p${version}-${i}`,
    author: AUTHORS[i % AUTHORS.length] ?? '',
    avatar: `https://i.pravatar.cc/80?img=${(i % 20) + 1}`,
    time: clock(i),
    text: words(rand, (1 + Math.floor(rand() * 5)) * 40 - 10),
    photo: i % 2 === 0 ? `https://picsum.photos/seed/flume-feed-${i}/600/450` : undefined,
    likes: Math.floor(rand() * 500),
    comments: Math.floor(rand() * 80),
  }));
}
