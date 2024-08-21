import * as fs from 'fs';
import * as MP4Box from 'mp4box';

const FILE_PATH_TO_INFO_CACHE = {};

export function getMp4Info(filePath, cache = true) {
  if (cache && FILE_PATH_TO_INFO_CACHE[filePath]) {
    return new Promise((resolve) => resolve(FILE_PATH_TO_INFO_CACHE[filePath]));
  }

  return new Promise((resolve, reject) => {
    const timeout = setTimeout(() => {
      reject('timeout ' + filePath);
    }, 5000);

    const mp4boxFile = MP4Box.createFile();

    mp4boxFile.onError = (err) => {
      clearTimeout(timeout);
      console.log('mp4boxFile err:', err);
      reject(err);
    };

    mp4boxFile.onReady = (info) => {
      clearTimeout(timeout);
      const durationInSeconds = info.duration / info.timescale;
      const creationTime = info.created;
      const result = { creationTime, durationInSeconds };
      if (cache) {
        FILE_PATH_TO_INFO_CACHE[filePath] = result;
      }
      resolve({ ...result });
    };

    fs.readFile(filePath, (err, date) => {
      if (err) {
        reject(err);
        return;
      }

      const arrayBuffer = new Uint8Array(date).buffer;
      arrayBuffer.fileStart = 0;
      mp4boxFile.appendBuffer(arrayBuffer);
      mp4boxFile.flush();
    });
  });
}
