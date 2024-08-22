<script>
  import SentryRecord from '$lib/components/sentry-record.svelte';

  export let data;

  $: sentryRecord = data.sentryRecord;
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
