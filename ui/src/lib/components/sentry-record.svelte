<script>
  import { tick } from 'svelte';
  import { page } from '$app/stores';

  export let sentryRecord;

  $: kDashcamPath = $page.url.protocol + '//' + $page.url.hostname + ':8001';

  $: console.log('sentryRecord', sentryRecord);
  $: {
    for (const [clipIdx, clip] of sentryRecord.clips.entries()) {
      const nextClip = sentryRecord.clips[clipIdx + 1];
      clip.begin = clip.info.creationTime;
      clip.end = nextClip
        ? nextClip.info.creationTime
        : new Date(clip.begin.getTime() + clip.info.durationInSeconds * 1000);
    }
  }
  $: recordDuration =
    (sentryRecord.clips.at(-1).end.getTime() -
      sentryRecord.clips.at(0).begin.getTime()) /
    1000;
  $: eventTime = sentryRecord.event
    ? (new Date(sentryRecord.event.timestamp).getTime() -
        sentryRecord.clips.at(0).begin.getTime()) /
      1000
    : null;

  $: currentClip = sentryRecord.clips.at(-1);
  $: currentClipIndex = sentryRecord.clips.length - 1;

  let focusedVideoType = 'front';

  const videos = {
    front: undefined,
    back: undefined,
    left_repeater: undefined,
    right_repeater: undefined,
  };

  function cameraIdToType(id) {
    if (id == '0') {
      return 'front';
    }
    if (id == '1') {
      return 'back';
    }
    if (id == '5') {
      return 'left_repeater';
    }
    if (id == '6') {
      return 'right_repeater';
    }
  }
  $: focusedVideoType = sentryRecord.event
    ? cameraIdToType(sentryRecord.event.camera)
    : 'front';

  let time = 0;
  let paused = true;
  let isSeeking = false;
  let lastSyncedTime = 0;

  const videoLoaded = {
    front: false,
    back: false,
    left_repeater: false,
    right_repeater: false,
  };
  const videoLoadedCallbacks = {
    front: [],
    back: [],
    left_repeater: [],
    right_repeater: [],
  };

  function clearVideoLoaded() {
    Array.from(Object.keys(videos)).forEach((type) => {
      videoLoaded[type] = false;
    });
  }

  function setVideoLoaded(type) {
    videoLoaded[type] = true;
    videoLoadedCallbacks[type].forEach((callback) => callback());
    videoLoadedCallbacks[type] = [];
  }

  async function waitAllVideosLoaded() {
    await Promise.all(
      Array.from(Object.keys(videos)).map((type) => {
        if (videoLoaded[type]) {
          return;
        }
        return new Promise((resolve) => {
          videoLoadedCallbacks[type].push(() => resolve());
        });
      })
    );
  }

  $: {
    if (eventTime !== null) {
      const kSeekOffset = 2.0;
      applyTimeOnVideos(eventTime - kSeekOffset);
    }
  }

  function forEachVideo(callback) {
    for (const [type, video] of Object.entries(videos)) {
      if (!video) {
        continue;
      }
      callback(video, type);
    }
  }

  function togglePaused() {
    paused = !paused;
    forEachVideo((video) => {
      if (paused) {
        video.pause();
      } else {
        video.play();
      }
    });
  }

  function isAbsTimeIncludedInCurrentClip(date) {
    return date >= currentClip.begin && date <= currentClip.end;
  }

  function findClipIdxIncludingTime(date) {
    return sentryRecord.clips.findIndex((clip) => {
      return date >= clip.begin && date < clip.end;
    });
  }

  function pauseAllVideos() {
    forEachVideo((video) => {
      video.pause();
    });
  }

  async function resumeAllVideosOnLoaded() {
    if (!paused) {
      await waitAllVideosLoaded();
      forEachVideo((video) => {
        video.play();
      });
    }
  }

  async function seekInClip(recordRelativeTime) {
    // wait the video elements updating.
    await tick();
    time = recordRelativeTime;
    const clipRelativeTime =
      recordRelativeTime +
      sentryRecord.clips[0].begin.getTime() / 1000 -
      currentClip.begin.getTime() / 1000;
    forEachVideo((video) => {
      video.currentTime = clipRelativeTime;
    });
  }

  async function loadClip(clipIndex) {
    // console.log('loadClip', clipIndex);
    if (clipIndex === -1 || clipIndex >= sentryRecord.clips.length) {
      console.log('invalid clip index');
      return false;
    }
    currentClip = sentryRecord.clips[clipIndex];
    currentClipIndex = clipIndex;
    // console.log('load clip:', clip);
    clearVideoLoaded();
    return true;
  }

  async function applyTimeOnVideos(recordRelativeTime) {
    const date = new Date(
      sentryRecord.clips.at(0).begin.getTime() + recordRelativeTime * 1000
    );
    // console.log('applyTimeOnVideos', date.toLocaleString());
    if (!isAbsTimeIncludedInCurrentClip(date)) {
      const clipIndex = findClipIdxIncludingTime(date);
      if (!(await loadClip(clipIndex))) {
        return;
      }
    }
    await seekInClip(recordRelativeTime);
  }

  function formatDate(date) {
    date = new Date(date);

    const ymd =
      date.getFullYear().toString().slice(2) +
      '/' +
      (date.getMonth() + 1).toString().padStart(2, '0') +
      '/' +
      date.getDay().toString().padStart(2, '0');
    return ymd;
  }

  function formatDatetime(recordRelativeTime) {
    if (isNaN(time)) return '...';

    const date = new Date(
      sentryRecord.clips.at(0).begin.getTime() + recordRelativeTime * 1000
    );

    const ymd =
      date.getFullYear().toString().slice(2) +
      '/' +
      (date.getMonth() + 1).toString().padStart(2, '0') +
      '/' +
      date.getDay().toString().padStart(2, '0');
    const hms =
      date.getHours().toString().padStart(2, '0') +
      ':' +
      date.getMinutes().toString().padStart(2, '0') +
      ':' +
      date.getSeconds().toString().padStart(2, '0');
    return ymd + ' ' + hms;
  }

  function formatTime(recordRelativeTime) {
    if (isNaN(time)) return '...';

    const date = new Date(
      sentryRecord.clips.at(0).begin.getTime() + recordRelativeTime * 1000
    );

    const hms =
      date.getHours().toString().padStart(2, '0') +
      ':' +
      date.getMinutes().toString().padStart(2, '0') +
      ':' +
      date.getSeconds().toString().padStart(2, '0');
    return hms;
  }
</script>

{#if currentClip}
  <div class="container">
    <div class="video-group">
      {#each ['front', 'back', 'left_repeater', 'right_repeater'] as type (type)}
        <div
          class="video-container"
          class:big={focusedVideoType === type}
          on:pointerdown={() => {
            if (focusedVideoType == type) {
              togglePaused();
            }
            focusedVideoType = type;
          }}
        >
          <span class="video-title">{type}</span>
          <video
            muted
            src="{kDashcamPath}/{currentClip.clips[type]}"
            bind:this={videos[type]}
            on:canplaythrough={(e) => {
              setVideoLoaded(type);
            }}
            on:ended={() => {
              if (isSeeking) {
                console.log('is seeking');
                return;
              }
              loadClip(currentClipIndex + 1).then((success) => {
                if (success) {
                  resumeAllVideosOnLoaded();
                }
              });
            }}
            on:timeupdate={(e) => {
              if (type === 'front') {
                time =
                  (currentClip.begin.getTime() -
                    sentryRecord.clips.at(0).begin.getTime()) /
                    1000 +
                  e.target.currentTime;
                const now = performance.now();
                if (now - lastSyncedTime > 2000) {
                  forEachVideo((video, t) => {
                    if (t !== 'front') {
                      video.currentTime = e.target.currentTime;
                    }
                  });
                  lastSyncedTime = now;
                }
              }
            }}
          >
          </video>
        </div>
      {/each}
    </div>

    <div class="player">
      <button
        class="play"
        aria-label={paused ? 'play' : 'pause'}
        on:click={togglePaused}
      />

      <div class="info">
        <div class="time">
          <div
            class="slider"
            on:pointerdown={(e) => {
              const div = e.currentTarget;
              isSeeking = true;
              pauseAllVideos();

              function seek(e) {
                const { left, width } = div.getBoundingClientRect();

                let p = (e.clientX - left) / width;
                if (p < 0) p = 0;
                if (p > 1) p = 1;

                applyTimeOnVideos(p * recordDuration);
              }

              seek(e);

              window.addEventListener('pointermove', seek);

              window.addEventListener(
                'pointerup',
                () => {
                  window.removeEventListener('pointermove', seek);
                  isSeeking = false;
                  resumeAllVideosOnLoaded();
                },
                {
                  once: true,
                }
              );
            }}
          >
            <div
              class="progress"
              style="--progress: {time / recordDuration}%"
            />
            {#if eventTime !== null}
              <div
                class="event-indicator"
                style="--position: {eventTime / recordDuration}%"
              />
            {/if}
          </div>
          <span>{formatTime(time)}</span>
          <!-- <span>{recordDuration ? format(recordDuration) : '--:--'}</span> -->
        </div>
        {#if sentryRecord.event}
          <div class="event-info">
            <span class="date">{formatDate(sentryRecord.event.timestamp)}</span>
            <span class="city">{sentryRecord.event.city}</span>
            <span class="reason">{sentryRecord.event.reason}</span>
          </div>
        {/if}
      </div>
    </div>
  </div>
{/if}

<style>
  .container {
    width: 100dvw;
    height: 100dvh;
    display: flex;
    flex-direction: column;
    background: #223;
  }

  .video-group {
    flex-grow: 1;

    display: flex;
    flex-direction: column;
  }

  .video-container {
    position: relative;
    width: 100%;
    height: auto;
    flex-grow: 1;
  }

  .video-container.big {
    flex-grow: 0;
    aspect-ratio: 4 / 3;
  }

  .video-title {
    position: absolute;
    top: 0;
    left: 50%;
    transform: translateX(-50%);
    z-index: 1;

    color: white;
    font-family: monospace;
    font-weight: bold;
    font-size: 1em;
    opacity: 0.65;
  }

  video {
    position: absolute;
    top: 0;
    left: 0;
    width: 100%;
    height: 100%;
    object-fit: contain;
  }

  .player {
    display: grid;
    grid-template-columns: 2em 1fr;
    align-items: center;
    gap: 1em;
    padding: 0.5em 1em 0.5em 0.5em;
    border-radius: 2em;
    transition: filter 0.2s;
    user-select: none;
  }

  button {
    width: 100%;
    aspect-ratio: 1;
    background-repeat: no-repeat;
    background-position: 50% 50%;
    border-radius: 50%;
    border: 0;
    background-color: #1c7ed4;
  }

  [aria-label='pause'] {
    background-image: url($lib/assets/pause.svg);
  }

  [aria-label='play'] {
    background-image: url($lib/assets/play.svg);
  }

  .info {
    font-family: monospace;
    color: #cce;
  }

  .time {
    display: flex;
    align-items: center;
    gap: 0.5em;
  }

  .time span {
    font-size: 0.7em;
    font-family: monospace;
    font-weight: bold;
  }

  .slider {
    flex: 1;
    height: 0.5em;
    background: #ddd;
    border-radius: 0.5em;
    overflow: hidden;
    position: relative;
    touch-action: none;
  }

  .progress {
    width: calc(100 * var(--progress));
    height: 100%;
    background: #bbb;
  }

  .event-indicator {
    position: absolute;
    top: 0;
    left: calc(
      100 * var(--position) - 2px
    ); /* -2px make it visible even at 99.99% */
    height: 100%;
    width: 2px;
    background: #d55;
  }
</style>
