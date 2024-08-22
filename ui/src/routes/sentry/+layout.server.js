import { listSentryClips } from '$lib/server/video-clips.js';

export async function load() {
  return {
    records: await listSentryClips(true),
  };
}
