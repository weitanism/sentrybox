<script>
  import { page } from '$app/stores';
  import SentryRecord from '$lib/components/sentry-record.svelte';

  $: sentryRecord = $page.data.records.find(
    (r) => r.directory === $page.params.time
  );
</script>

<div class="back-button">
  <a href="/sentry">back</a>
</div>

{#if sentryRecord}
  <SentryRecord {sentryRecord} />
{/if}

<style>
  .back-button {
    position: absolute;
    top: 4px;
    left: 4px;
    color: #ccd;
    z-index: 1;
    font-family: monospace;
    font-size: 1.5em;
  }
</style>
