import { listRecentClips } from '$lib/server/video-clips.js';

export async function load() {
  return {
    record: {
      clips: await listRecentClips(),
    },
  };
}
