import { getSentryRecord } from '../../../lib/server/video-clips';

export async function load({ params }) {
  return {
    sentryRecord: await getSentryRecord(params.time),
  };
}
