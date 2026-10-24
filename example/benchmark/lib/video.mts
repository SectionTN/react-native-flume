import { execFileSync, spawn } from 'node:child_process';
import { blankFraction } from './blank.mts';

export function videoSize(path: string): [number, number] {
  const out = execFileSync(
    'ffprobe',
    [
      '-v',
      'error',
      '-select_streams',
      'v:0',
      '-show_entries',
      'stream=width,height',
      '-of',
      'csv=p=0:s=x',
      path,
    ],
    { encoding: 'utf8' },
  );
  const [width, height] = out.trim().split('x').map(Number);
  return [width ?? 0, height ?? 0];
}

// One blank fraction per decoded frame of the crop [left, top, right, bottom], at a quarter of its size.
export function blankFractions(
  path: string,
  crop: [number, number, number, number],
  sentinel: readonly [number, number, number],
  tolerance: number,
): Promise<number[]> {
  const [left, top, right, bottom] = crop;
  const width = right - left;
  const height = bottom - top;
  const frameBytes = (width / 4) * (height / 4) * 3;
  const ffmpeg = spawn('ffmpeg', [
    '-v',
    'error',
    '-i',
    path,
    '-vf',
    `crop=${width}:${height}:${left}:${top},scale=iw/4:ih/4`,
    '-fps_mode',
    'passthrough',
    '-f',
    'rawvideo',
    '-pix_fmt',
    'rgb24',
    '-',
  ]);
  const fractions: number[] = [];
  let pending: Buffer = Buffer.alloc(0);
  let errors = '';
  ffmpeg.stdout.on('data', (chunk: Buffer) => {
    pending = pending.length === 0 ? chunk : Buffer.concat([pending, chunk]);
    while (pending.length >= frameBytes) {
      fractions.push(blankFraction(pending.subarray(0, frameBytes), sentinel, tolerance));
      pending = pending.subarray(frameBytes);
    }
  });
  ffmpeg.stderr.on('data', (chunk: Buffer) => {
    errors += chunk.toString();
  });
  return new Promise((resolve, reject) => {
    ffmpeg.on('error', reject);
    ffmpeg.on('close', (code) => {
      if (code === 0) resolve(fractions);
      else reject(new Error(`ffmpeg exited with ${code}: ${errors.trim()}`));
    });
  });
}
