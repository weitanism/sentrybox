<script>
  export let data;

  import SentryRecord from '$lib/components/sentry-record.svelte';

  $: sentryRecord = data.record;
</script>

<div class="back-button">
  <a href="/">back</a>
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
