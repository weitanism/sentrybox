import { readdir, readFile } from 'fs';
import { getMp4Info } from './video-info';

const kTeslaCamRootDir = '/mnt/mass-storage/TeslaCam';
const kClipFileRegex = /(\d\d\d\d-\d\d-\d\d_\d\d-\d\d-\d\d)-([_a-z]+).mp4/;
const kDatetimeRegex = /(\d\d\d\d)-(\d\d)-(\d\d)_(\d\d)-(\d\d)-(\d\d)/;

async function listDirectory(path) {
  return new Promise((resolve, reject) => {
    readdir(path, (err, files) => {
      if (err) {
        reject(err);
      } else {
        resolve(files);
      }
    });
  });
}

async function readEventFile(path) {
  return new Promise((resolve, reject) => {
    readFile(path, (err, content) => {
      if (err) {
        reject(err);
      } else {
        try {
          const event = JSON.parse(content);
          resolve(event);
        } catch {
          reject('malformed event file');
        }
      }
    });
  });
}

function dateTimeToISOSting(timeStr) {
  const match = timeStr.match(kDatetimeRegex);
  if (!match) {
    return;
  }

  const [_, yyyy, mm, dd, HH, MM, SS] = match;
  return `${yyyy}-${mm}-${dd}T${HH}:${MM}:${SS}`;
}

async function listClipDirectory(clipDirRelativePath) {
  const files = await listDirectory(
    `${kTeslaCamRootDir}/${clipDirRelativePath}`
  );
  files.sort();
  const clips = [];
  let lastTime;
  for (const filename of files) {
    const match = filename.match(kClipFileRegex);
    if (!match) {
      continue;
    }
    const [_, time, type] = match;
    const timestamp = dateTimeToISOSting(time);
    if (!timestamp) {
      continue;
    }
    const filePath = `${clipDirRelativePath}/${filename}`;
    if (clips.length === 0 || lastTime !== time) {
      clips.push({
        info: await getMp4Info(`${kTeslaCamRootDir}/${filePath}`),
        clips: {
          [type]: filePath,
        },
      });
    } else {
      clips.at(-1).clips[type] = filePath;
    }
    lastTime = time;
  }
  return clips;
}

export async function listRecentClips() {
  return listClipDirectory('RecentClips');
}

export async function listSavedClips(savedDirName = 'SavedClips') {
  const savedClipDirs = await listDirectory(
    `${kTeslaCamRootDir}/${savedDirName}`
  );
  savedClipDirs.sort();
  const savedClips = [];
  for (const dirname of savedClipDirs) {
    savedClips.push({
      directory: dirname,
      clips: await listClipDirectory(`${savedDirName}/${dirname}`),
      event: await readEventFile(
        `${kTeslaCamRootDir}/${savedDirName}/${dirname}/event.json`
      ),
      thumb: `${savedDirName}/${dirname}/thumb.png`,
    });
  }
  return savedClips;
}

export async function listSentryClips() {
  return listSavedClips('SentryClips');
}
